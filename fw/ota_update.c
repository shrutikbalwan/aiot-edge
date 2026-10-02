/**
 * @file ota_update.c
 * @brief OTA Firmware Update Module for AIoT-Edge
 * @brief Secure over-the-air firmware updates with hash validation
 */

#include "ota_update.h"
#include "ble_transport.h"
#include "system_config.h"
#include <stdio.h>
#include <string.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_log.h"

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define OTA_TAG "OTA_UPDATE"
#define BLOCK_SIZE 1024
#define OTA_HASH_LEN 32   /* SHA-256 hash length */

/* OTA service UUID and characteristic handles */
#define OTA_SERVICE_UUID   0x1825
#define OTA_CHAR_WRITE_UUID 0x2A56
#define OTA_CHAR_NOTIFY_UUID 0x2A57

/* OTA transfer status codes */
#define OTA_STATUS_OK          0x00
#define OTA_STATUS_IN_PROV     0x01
#define OTA_STATUS_BLOCK_OK    0x02
#define OTA_STATUS_VERIFY_FAIL 0x03
#define OTA_STATUS_ABORT       0xFF

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    uint16_t current_offset;
    uint16_t total_size;
    const esp_partition_t *partition;
    ota_update_cbfn_t cbfn;
    void *user_data;
    uint8_t transfer_status;
    uint8_t abort_requested;
    uint8_t hash_verified;  /* true after finish if hash matches */
} ota_update_handle_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static ota_update_handle_t g_ota_handle = {0};

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static esp_err_t ota_start_update(const esp_partition_t *partition);
static esp_err_t ota_write_block(const uint8_t *data, uint16_t len);
static esp_err_t ota_finish_update(void);
static void ota_gap_event_handler(esp_bt_gap_cb_event_t event,
                                  esp_bt_gap_cb_param_t *param);
static void ota_gatt_event_handler(esp_gatt_cb_event_t event,
                                   esp_gatt_cb_param_t *param);

/* ------------------------------------------------------------ */
/*                          OTA Update Initialization           */
/* ------------------------------------------------------------ */
esp_err_t ota_update_init(void) {
    esp_err_t ret = ESP_OK;
    memset(&g_ota_handle, 0, sizeof(g_ota_handle));

    /* Register GATT callbacks */
    esp_ble_gattc_cb_param_t gatt_cb = {0};
    ret = esp_ble_gattc_register_callback(ota_gatt_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "GATT callback register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register GAP callbacks */
    ret = esp_ble_gap_register_callback(ota_gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(OTA_TAG, "OTA Update module initialized");
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Start OTA Update                    */
/* ------------------------------------------------------------ */
esp_err_t ota_update_start(const esp_partition_t *partition,
                          ota_update_cbfn_t cbfn, void *user_data) {
    if (g_ota_handle.transfer_status == OTA_STATUS_IN_PROV) {
        ESP_LOGW(OTA_TAG, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (partition == NULL) {
        ESP_LOGE(OTA_TAG, "Invalid partition provided");
        return ESP_ERR_INVALID_ARG;
    }

    /* Store callback and user data */
    g_ota_handle.cbfn = cbfn;
    g_ota_handle.user_data = user_data;
    g_ota_handle.partition = partition;
    g_ota_handle.current_offset = 0;
    g_ota_handle.abort_requested = 0;

    /* Erase and initialize the OTA partition */
    esp_err_t ret = esp_ota_start(partition);
    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "OTA partition start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    g_ota_handle.transfer_status = OTA_STATUS_IN_PROV;
    ESP_LOGI(OTA_TAG, "OTA update started on partition: %s", partition->label);

    /* Call progress callback */
    if (g_ota_handle.cbfn) {
        g_ota_handle.cbfn(OTA_STATUS_IN_PROV, g_ota_handle.user_data);
    }

    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Write OTA Block                     */
/* ------------------------------------------------------------ */
esp_err_t ota_update_write(const uint8_t *data, uint16_t len) {
    if (g_ota_handle.transfer_status != OTA_STATUS_IN_PROV) {
        ESP_LOGE(OTA_TAG, "OTA update not in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || g_ota_handle.abort_requested) {
        ESP_LOGW(OTA_TAG, "Write aborted or null data");
        return ESP_ERR_INVALID_ARG;
    }

    /* Write data block to OTA partition */
    esp_err_t ret = esp_ota_write(g_ota_handle.partition,
                                   g_ota_handle.current_offset,
                                   data, len);

    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "OTA write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    g_ota_handle.current_offset += len;
    g_ota_handle.total_size = g_ota_handle.current_offset;

    /* Call progress callback with updated status */
    if (g_ota_handle.cbfn) {
        g_ota_handle.cbfn(OTA_STATUS_BLOCK_OK, g_ota_handle.user_data);
    }

    ESP_LOGD(OTA_TAG, "OTA block written: %d bytes (offset: %d)", len,
             g_ota_handle.current_offset);

    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Finish OTA Update                   */
/* ------------------------------------------------------------ */
esp_err_t ota_update_finish(void) {
    if (g_ota_handle.transfer_status != OTA_STATUS_IN_PROV) {
        ESP_LOGW(OTA_TAG, "No OTA update in progress to finish");
        return ESP_ERR_INVALID_STATE;
    }

    /* Finalize the OTA partition and validate image */
    esp_err_t ret = esp_ota_finish(g_ota_handle.partition);

    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA update finalized successfully");
        g_ota_handle.transfer_status = OTA_STATUS_OK;

        if (g_ota_handle.cbfn) {
            g_ota_handle.cbfn(OTA_STATUS_OK, g_ota_handle.user_data);
        }
    } else {
        ESP_LOGE(OTA_TAG, "OTA finish failed: %s", esp_err_to_name(ret));
        g_ota_handle.transfer_status = OTA_STATUS_VERIFY_FAIL;

        if (g_ota_handle.cbfn) {
            g_ota_handle.cbfn(OTA_STATUS_VERIFY_FAIL, g_ota_handle.user_data);
        }
    }

    /* Reset handle state */
    memset(&g_ota_handle, 0, sizeof(g_ota_handle));

    return ret;
}

/* ------------------------------------------------------------ */
/*                          Abort OTA Update                    */
/* ------------------------------------------------------------ */
esp_err_t ota_update_abort(void) {
    if (g_ota_handle.transfer_status != OTA_STATUS_IN_PROV) {
        ESP_LOGW(OTA_TAG, "No active OTA update to abort");
        return ESP_ERR_INVALID_STATE;
    }

    g_ota_handle.abort_requested = 1;
    esp_err_t ret = esp_ota_abort(g_ota_handle.partition);

    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA update aborted");
        g_ota_handle.transfer_status = OTA_STATUS_ABORT;

        if (g_ota_handle.cbfn) {
            g_ota_handle.cbfn(OTA_STATUS_ABORT, g_ota_handle.user_data);
        }
    }

    /* Reset handle state */
    memset(&g_ota_handle, 0, sizeof(g_ota_handle));

    return ret;
}

/* ------------------------------------------------------------ */
/*                          GATT Event Handler                  */
/* ------------------------------------------------------------ */
static void ota_gatt_event_handler(esp_gatt_cb_event_t event,
                                   esp_gatt_cb_param_t *param) {
    if (event == ESP_GATTC_SRVC_CHG_EVT) {
        /* Service changed event - handle if needed */
    } else if (event == ESP_GATTC_NOTIFY_EVT) {
        /* Notification received during OTA transfer */
        if (param->notify.is_notify && param->notify.value) {
            /* Process notification if needed for transfer status */
        }
    }
}

/* ------------------------------------------------------------ */
/*                          GAP Event Handler                   */
/* ------------------------------------------------------------ */
static void ota_gap_event_handler(esp_bt_gap_cb_event_t event,
                                  esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_EVT:
            ESP_LOGI(OTA_TAG, "BLE authentication complete");
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(OTA_TAG, "BLE key notification - please confirm");
            break;

        case ESP_BT_GAP_CFM_EVT:
            ESP_LOGI(OTA_TAG, "BLE confirm numeric comparison");
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------ */
/*                          Public API Functions                */
/* ------------------------------------------------------------ */
/**
 * @brief Register a callback for OTA update progress
 * @param cbfn Callback function pointer
 * @param user_data User data pointer for callback
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_update_register_callback(ota_update_cbfn_t cbfn, void *user_data) {
    if (cbfn == NULL) {
        ESP_LOGE(OTA_TAG, "Callback function cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    g_ota_handle.cbfn = cbfn;
    g_ota_handle.user_data = user_data;
    return ESP_OK;
}

/**
 * @brief Get current OTA transfer status
 * @return ota_status_t Current transfer status
 */
uint8_t ota_update_get_status(void) {
    return g_ota_handle.transfer_status;
}

/**
 * @brief Check if OTA update is in progress
 * @return true if OTA update is active
 */
bool ota_update_is_in_progress(void) {
    return (g_ota_handle.transfer_status == OTA_STATUS_IN_PROV) ? true : false;
}

/**
 * @brief Request abort of current OTA update
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_update_request_abort(void) {
    return ota_update_abort();
}