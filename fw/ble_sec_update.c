/**
 * @file ble_sec_update.c
 * @brief BLE Security & LE Secure Connections for AIoT-Edge
 * @brief Enhanced BLE pairing, bonding, and privacy features
 */

#include "ble_sec_update.h"
#include "ble_transport.h"
#include "system_config.h"
#include <stdio.h>
#include <string.h>
#include "esp_bt.h"
#include "esp_gap_bt_defs.h"
#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define BLE_SEC_TAG "BLE_SEC"
#define BONDING_TIMEOUT_SECONDS 300
#define MAX_BONDING_KEYS 4

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    uint8_t bond_keys[MAX_BONDING_KEYS][16]; /* LTK, CSRK, IR */
    uint8_t num_keys;
    uint8_t bonding_done;
    uint8_t privacy_mode; /* 0 = disabled, 1 = enabled */
    uint8_t random_addr_enabled;
    uint32_t last_bond_time;
} ble_sec_handle_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static ble_sec_handle_t g_sec_handle = {0};

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static void ble_sec_gap_event_handler(esp_bt_gap_cb_event_t event,
                                      esp_bt_gap_cb_param_t *param);
static void ble_sec_gatt_event_handler(esp_gatt_cb_event_t event,
                                        esp_gatt_cb_param_t *param);
esp_err_t ble_sec_init(void);
esp_err_t ble_sec_start_bonding(void);
esp_err_t ble_sec_abort_bonding(void);
esp_err_t ble_sec_set_privacy_mode(uint8_t mode);
uint8_t ble_sec_get_status(void);

/* ------------------------------------------------------------ */
/*                          GAP Event Handler                   */
/* ------------------------------------------------------------ */
static void ble_sec_gap_event_handler(esp_bt_gap_cb_event_t event,
                                      esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_EVT: {
            esp_bt_gap_auth_status_t *auth = &param->auth_status;
            ESP_LOGI(BLE_SEC, "Authentication status: %s",
                     auth->success ? "SUCCESS" : "FAIL);
            if (auth->success) {
                g_sec_handle.bonding_done = 1;
                g_sec_handle.last_bond_time = xTaskGetTickCount();
                ESP_LOGI(BLE_SEC, "Bonding completed successfully");
            }
            break;
        }

        case ESP_BT_GAP_KEY_NOTIF_EVT: {
            esp_bt_gap_key_notif_t *key = &param->key_notif;
            ESP_LOGI(BLE_SEC, "Key notification - passkey: %06d", key->passkey);
            /* User would confirm the passkey on the other device */
            break;
        }

        case ESP_BT_GAP_CFM_EVT: {
            esp_bt_gap_cfm_t *cfm = &param->cfm;
            ESP_LOGI(BLE_SEC, "Confirm numeric comparison: %06d", cfm->num_val);
            /* User confirms the numeric comparison on the other device */
            break;
        }

        case ESP_BT_GAP_RESOLVE_EVT: {
            esp_bt_gap_resolve_t *resolve = &param->resolve;
            ESP_LOGI(BLE_SEC, "Resolving address info for handle %d", resolve->handle);
            break;
        }

        default:
            break;
    }
}

/* ------------------------------------------------------------ */
/*                          GATT Event Handler                  */
/* ------------------------------------------------------------ */
static void ble_sec_gatt_event_handler(esp_gatt_cb_event_t event,
                                        esp_gatt_cb_param_t *param) {
    if (event == ESP_GATTC_INDICATE_EVT) {
        ESP_LOGD(BLE_SEC, "Indication received from peer");
    }
    /* Handle other GATT events as needed for security */
}

/* ------------------------------------------------------------ */
/*                          BLE Security Initialization         */
/* ------------------------------------------------------------ */
esp_err_t ble_sec_init(void) {
    memset(&g_sec_handle, 0, sizeof(g_sec_handle));

    /* Register GAP callbacks for security events */
    esp_err_t ret = esp_ble_gap_register_callback(ble_sec_gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_SEC, "GAP callback register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register GATT callbacks */
    ret = esp_ble_gattc_register_callback(ble_sec_gatt_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_SEC, "GATT callback register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(BLE_SEC, "BLE Security module initialized");
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Start Bonding                     */
/* ------------------------------------------------------------ */
esp_err_t ble_sec_start_bonding(void) {
    if (g_sec_handle.bonding_done) {
        ESP_LOGW(BLE_SEC, "Bonding already completed");
        return ESP_ERR_INVALID_STATE;
    }

    /* Set BLE to use LE Secure Connections */
    esp_ble_gap_set_cipher_type(ESP_BLE_CIPHER_TYPE_LE_SC);

    /* Enable privacy mode if configured */
    if (g_sec_handle.privacy_mode) {
        esp_ble_gap_set_rand_addr(RAND_ADDR_ENABLE);
        ESP_LOGI(BLE_SEC, "Privacy mode enabled - random address rotation");
    }

    /* Start bonding procedure */
    esp_err_t ret = esp_ble_gap_start_bonding(ESP_BLE_GAP_BONDING_BS0);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_SEC, "Failed to start bonding: %s", esp_err_to_name(ret));
        return ret;
    }

    g_sec_handle.bonding_done = 0;
    ESP_LOGI(BLE_SEC, "BLE bonding started - waiting for peer connection");
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Abort Bonding                     */
/* ------------------------------------------------------------ */
esp_err_t ble_sec_abort_bonding(void) {
    if (!g_sec_handle.bonding_done) {
        ESP_LOGW(BLE_SEC, "No bonding in progress");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ble_gap_stop_bonding();
    if (ret == ESP_OK) {
        memset(&g_sec_handle, 0, sizeof(g_sec_handle));
        ESP_LOGI(BLE_SEC, "Bonding aborted");
    }
    return ret;
}

/* ------------------------------------------------------------ */
/*                          Set Privacy Mode                  */
/* ------------------------------------------------------------ */
esp_err_t ble_sec_set_privacy_mode(uint8_t mode) {
    if (mode > 1) {
        ESP_LOGE(BLE_SEC, "Invalid privacy mode: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }

    g_sec_handle.privacy_mode = mode;

    if (mode) {
        esp_err_t ret = esp_ble_gap_set_rand_addr(RAND_ADDR_ENABLE);
        if (ret == ESP_OK) {
            ESP_LOGI(BLE_SEC, "Privacy mode enabled - random address enabled");
        }
    } else {
        ESP_LOGI(BLE_SEC, "Privacy mode disabled - static address");
    }

    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Get Security Status               */
/* ------------------------------------------------------------ */
uint8_t ble_sec_get_status(void) {
    return g_sec_handle.bonding_done | (g_sec_handle.privacy_mode << 1) |
           (g_sec_handle.random_addr_enabled << 2);
}

/* ------------------------------------------------------------ */
/*                          Public API Functions                */
/* ------------------------------------------------------------ */
/**
 * @brief Get current BLE security status
 * @return ble_sec_status_t Status bits
 */
ble_sec_status_t ble_sec_get_status_extended(void) {
    ble_sec_status_t status = {0};
    status.bonding_done = g_sec_handle.bonding_done;
    status.privacy_mode = g_sec_handle.privacy_mode;
    status.random_addr = g_sec_handle.random_addr_enabled;
    return status;
}

/**
 * @brief Refresh random address for privacy
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_sec_refresh_address(void) {
    g_sec_handle.random_addr_enabled = 1;
    return esp_ble_gap_set_rand_addr(RAND_ADDR_ENABLE);
}