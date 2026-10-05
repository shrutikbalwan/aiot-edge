#include "mqtt_transport.h"

#include <stdio.h>
#include <string.h>

#include "aiot_serialization.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"

#define MQTT_PAYLOAD_MAX 512U
#define MQTT_TOPIC_MAX 160U
#define MQTT_RECONNECT_TIMEOUT_MS 5000U

static const char *TAG = "mqtt_transport";
#if CONFIG_AIOT_MQTT_ENABLED
static esp_mqtt_client_handle_t s_client;
static bool s_connected;
#endif
static char s_telemetry_topic[MQTT_TOPIC_MAX];
static char s_command_topic[MQTT_TOPIC_MAX];

static esp_err_t make_topic(char *output, size_t output_size, const char *suffix)
{
    const int count = snprintf(output, output_size, "%s/devices/%s/%s",
                               CONFIG_AIOT_TOPIC_PREFIX, CONFIG_AIOT_DEVICE_ID, suffix);
    return count > 0 && (size_t)count < output_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

#if CONFIG_AIOT_MQTT_ENABLED
static void handle_command(const esp_mqtt_event_handle_t event)
{
    if (event->topic_len != (int)strlen(s_command_topic) ||
        memcmp(event->topic, s_command_topic, (size_t)event->topic_len) != 0) {
        return;
    }
    if (event->data_len <= 0 || event->data_len > (int)MQTT_PAYLOAD_MAX) {
        ESP_LOGW(TAG, "rejected MQTT command length %d", event->data_len);
        return;
    }
    aiot_command_t command = {0};
    const esp_err_t err = aiot_parse_command_json(event->data, (size_t)event->data_len, &command);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rejected malformed MQTT command: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "validated MQTT command kind=%d", command.kind);
    /* Dispatch is deliberately explicit; reboot and OTA are not executed in the MQTT callback. */
}

static void mqtt_event(void *argument, esp_event_base_t base, int32_t event_id,
                       void *event_data)
{
    (void)argument;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        if (esp_mqtt_client_subscribe(s_client, s_command_topic, 1) < 0) {
            ESP_LOGE(TAG, "command subscription failed");
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        break;
    case MQTT_EVENT_DATA:
        handle_command(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT transport error");
        break;
    default:
        break;
    }
}
#endif

esp_err_t mqtt_transport_init(void)
{
    ESP_RETURN_ON_ERROR(make_topic(s_telemetry_topic, sizeof(s_telemetry_topic), "telemetry"),
                        TAG, "telemetry topic");
    ESP_RETURN_ON_ERROR(make_topic(s_command_topic, sizeof(s_command_topic), "commands"),
                        TAG, "command topic");
#if CONFIG_AIOT_MQTT_ENABLED
    const bool tls = strncmp(CONFIG_AIOT_MQTT_BROKER_URI, "mqtts://", 8) == 0;
    const bool development_plaintext = CONFIG_AIOT_SIMULATION &&
        strncmp(CONFIG_AIOT_MQTT_BROKER_URI, "mqtt://", 7) == 0;
    if (!tls && !development_plaintext) {
        ESP_LOGE(TAG, "production MQTT requires mqtts:// with hostname verification");
        return ESP_ERR_INVALID_ARG;
    }
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_AIOT_MQTT_BROKER_URI,
        .broker.verification.crt_bundle_attach = tls ? esp_crt_bundle_attach : NULL,
        .credentials.client_id = CONFIG_AIOT_DEVICE_ID,
        .credentials.username = CONFIG_AIOT_MQTT_USERNAME[0] != '\0' ? CONFIG_AIOT_MQTT_USERNAME : NULL,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
        .buffer.size = MQTT_PAYLOAD_MAX,
    };
    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                       mqtt_event, NULL), TAG,
                        "MQTT event handler");
    return esp_mqtt_client_start(s_client);
#else
    ESP_LOGI(TAG, "MQTT disabled by CONFIG_AIOT_MQTT_ENABLED");
    return ESP_OK;
#endif
}

esp_err_t mqtt_transport_publish_telemetry(const aiot_sensor_sample_t *sample,
                                            const aiot_health_estimate_t *estimate)
{
    if (sample == NULL || estimate == NULL) return ESP_ERR_INVALID_ARG;
#if CONFIG_AIOT_MQTT_ENABLED
    if (!s_connected || s_client == NULL) return ESP_ERR_INVALID_STATE;
    char payload[MQTT_PAYLOAD_MAX] = {0};
    ESP_RETURN_ON_ERROR(aiot_serialize_telemetry_json(sample, estimate, payload,
                                                      sizeof(payload)), TAG,
                        "serialize telemetry");
    return esp_mqtt_client_publish(s_client, s_telemetry_topic, payload, 0, 1, 0) >= 0
               ? ESP_OK : ESP_FAIL;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
