#include "ble_transport.h"

#include "aiot_serialization.h"
#include "command_dispatcher.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "ble_transport";

#if CONFIG_AIOT_BLE_ENABLED
#include <string.h>

#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define TELEMETRY_PAYLOAD_MAX 32U

enum characteristic_id {
    CHARACTERISTIC_DEVICE_INFO = 1,
    CHARACTERISTIC_TELEMETRY,
    CHARACTERISTIC_WAKE_EVENT,
    CHARACTERISTIC_DEVICE_STATUS,
    CHARACTERISTIC_COMMAND,
    CHARACTERISTIC_OTA_STATUS,
};

static uint16_t s_connection_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_telemetry_handle;
static bool s_telemetry_subscribed;
static uint8_t s_own_address_type;

static const ble_uuid128_t s_service_uuid = BLE_UUID128_INIT(
    0x7a, 0x2c, 0x00, 0x00, 0xe2, 0xa4, 0x4f, 0x88,
    0x9a, 0xd3, 0x5d, 0x6f, 0x01, 0x00, 0xa1, 0x10);

#define AIOT_UUID(number) BLE_UUID128_INIT( \
    (number), 0x2c, 0x00, 0x00, 0xe2, 0xa4, 0x4f, 0x88, \
    0x9a, 0xd3, 0x5d, 0x6f, 0x01, 0x00, 0xa1, 0x10)

static const ble_uuid128_t s_device_info_uuid = AIOT_UUID(0x01);
static const ble_uuid128_t s_telemetry_uuid = AIOT_UUID(0x02);
static const ble_uuid128_t s_wake_uuid = AIOT_UUID(0x03);
static const ble_uuid128_t s_status_uuid = AIOT_UUID(0x04);
static const ble_uuid128_t s_command_uuid = AIOT_UUID(0x05);
static const ble_uuid128_t s_ota_uuid = AIOT_UUID(0x06);

static int access_callback(uint16_t connection_handle, uint16_t attribute_handle,
                           struct ble_gatt_access_ctxt *context, void *argument)
{
    (void)connection_handle;
    (void)attribute_handle;
    const enum characteristic_id id = (enum characteristic_id)(uintptr_t)argument;
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR && id == CHARACTERISTIC_DEVICE_INFO) {
        static const char information[] = "{\"schema\":1,\"target\":\"esp32s3\"}";
        return os_mbuf_append(context->om, information, sizeof(information) - 1U) == 0
                   ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR && id == CHARACTERISTIC_COMMAND) {
        const uint16_t length = OS_MBUF_PKTLEN(context->om);
        if (length == 0U || length > 512U) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        char payload[513] = {0};
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(context->om, payload, length, &copied) != 0 || copied != length) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        aiot_command_t command = {0};
        if (aiot_parse_command_json(payload, length, &command) != ESP_OK) {
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }
        /* Encryption alone is not product authorization. Reject until a
         * provisioned BLE identity/ACL is implemented. */
        return command_dispatcher_submit(&command, AIOT_COMMAND_SOURCE_BLE, false) == ESP_OK
                   ? 0 : BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {.uuid = &s_device_info_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_DEVICE_INFO, .flags = BLE_GATT_CHR_F_READ},
            {.uuid = &s_telemetry_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_TELEMETRY,
             .val_handle = &s_telemetry_handle, .flags = BLE_GATT_CHR_F_NOTIFY},
            {.uuid = &s_wake_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_WAKE_EVENT, .flags = BLE_GATT_CHR_F_NOTIFY},
            {.uuid = &s_status_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_DEVICE_STATUS, .flags = BLE_GATT_CHR_F_NOTIFY},
            {.uuid = &s_command_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_COMMAND, .flags = BLE_GATT_CHR_F_WRITE},
            {.uuid = &s_ota_uuid.u, .access_cb = access_callback,
             .arg = (void *)(uintptr_t)CHARACTERISTIC_OTA_STATUS, .flags = BLE_GATT_CHR_F_NOTIFY},
            {0},
        },
    },
    {0},
};

static void start_advertising(void);

static int gap_event(struct ble_gap_event *event, void *argument)
{
    (void)argument;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connection_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE connected");
        } else {
            start_advertising();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        s_connection_handle = BLE_HS_CONN_HANDLE_NONE;
        s_telemetry_subscribed = false;
        start_advertising();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_telemetry_handle) {
            s_telemetry_subscribed = event->subscribe.cur_notify != 0;
        }
        return 0;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated to %u", event->mtu.value);
        return 0;
    default:
        return 0;
    }
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name = ble_svc_gap_device_name();
    fields.name = (const uint8_t *)name;
    fields.name_len = (uint8_t)strlen(name);
    fields.name_is_complete = 1;
    fields.uuids128 = (ble_uuid128_t *)&s_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) {
        ESP_LOGE(TAG, "unable to set advertising data");
        return;
    }
    const struct ble_gap_adv_params parameters = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    const int rc = ble_gap_adv_start(s_own_address_type, NULL, BLE_HS_FOREVER,
                                     &parameters, gap_event, NULL);
    if (rc != 0) ESP_LOGE(TAG, "advertising failed: %d", rc);
}

static void on_sync(void)
{
    if (ble_hs_id_infer_auto(0, &s_own_address_type) != 0) {
        ESP_LOGE(TAG, "unable to determine BLE address");
        return;
    }
    start_advertising();
}

static void host_task(void *argument)
{
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t aiot_ble_transport_init(void)
{
    int rc = nimble_port_init();
    if (rc != 0) return ESP_FAIL;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_svc_gap_device_name_set("AIoT-Edge");
    if (rc != 0) return ESP_FAIL;
    rc = ble_gatts_count_cfg(s_services);
    if (rc != 0) return ESP_FAIL;
    rc = ble_gatts_add_svcs(s_services);
    if (rc != 0) return ESP_FAIL;
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

esp_err_t aiot_ble_transport_notify_telemetry(const aiot_sensor_sample_t *sample,
                                              const aiot_health_estimate_t *estimate)
{
    if (sample == NULL || estimate == NULL) return ESP_ERR_INVALID_ARG;
    if (s_connection_handle == BLE_HS_CONN_HANDLE_NONE || !s_telemetry_subscribed) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t payload[TELEMETRY_PAYLOAD_MAX] = {0};
    size_t size = 0;
    ESP_RETURN_ON_ERROR(aiot_serialize_ble_telemetry(sample, estimate, payload,
                                                     sizeof(payload), &size), TAG,
                        "serialize BLE telemetry");
    const uint16_t mtu = ble_att_mtu(s_connection_handle);
    if (size > (size_t)(mtu - 3U)) return ESP_ERR_INVALID_SIZE;
    struct os_mbuf *buffer = ble_hs_mbuf_from_flat(payload, (uint16_t)size);
    if (buffer == NULL) return ESP_ERR_NO_MEM;
    return ble_gatts_notify_custom(s_connection_handle, s_telemetry_handle, buffer) == 0
               ? ESP_OK : ESP_FAIL;
}

#else

esp_err_t aiot_ble_transport_init(void)
{
    ESP_LOGI(TAG, "BLE disabled by CONFIG_AIOT_BLE_ENABLED");
    return ESP_OK;
}

esp_err_t aiot_ble_transport_notify_telemetry(const aiot_sensor_sample_t *sample,
                                              const aiot_health_estimate_t *estimate)
{
    if (sample == NULL || estimate == NULL) return ESP_ERR_INVALID_ARG;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
