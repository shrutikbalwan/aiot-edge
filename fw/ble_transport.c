/**
 * @file ble_transport.c
 * @brief BLE Transport Layer for AIoT-Edge
 * @brief Bluetooth Low Energy 5.3 implementation for sensor data and OTA
 */

#include "ble_transport.h"
#include "system_config.h"
#include <stdio.h>
#include <string.h>
#include "esp_bt.h"
#include "esp_gatt_defs.h"
#include "esp_gap_bt_defs.h"
#include "esp_spp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------ */
/*                                  Configuration                 */
/* ------------------------------------------------------------ */
#define BLE_DEVICE_NAME "AIoT-Edge-HM"
#define BLE_ADVERTISING_INTERVAL  160  /* 100ms units ~ 160ms */
#define BLE_CONNECTION_INTERVAL   32   /* 20ms units ~ 20ms */
#define BLE_SUPERVISION_TIMEOUT 1000   /* 10s units ~ 10s */
#define MAX_NOTIFICATION_PAYLOAD 20

/* ------------------------------------------------------------ */
/*                                  Global Variables              */
/* ------------------------------------------------------------ */
static uint8_t g_bonding_done = 0;
static uint16_t g_conn_handle = 0;
static uint8_t g_notify_index = 0;
static uint8_t g_tx_power = 0;

/* Callback function pointers */
static ble_notify_cbfn_t g_data_cbfn = NULL;
static ble_connect_cbfn_t g_connect_cbfn = NULL;
static ble_disconnect_cbfn_t g_disconnect_cbfn = NULL;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
static void ble_gap_event_handler(esp_bt_gap_cb_event_t event, 
                                  esp_bt_gap_cb_param_t *param);
static void ble_gatt_event_handler(esp_gatt_cb_event_t event, 
                                   esp_gatt_cb_param_t *param);

/* ------------------------------------------------------------ */
/*                          BLE Initialization & Configuration    */
/* ------------------------------------------------------------ */
esp_err_t ble_transport_init(void) {
    esp_err_t ret;
    
    /* Initialize Bluetooth controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("BLE", "Controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Set controller mode to BLE */
    esp_bt_controller_mem_dump(ESP_BT_MODE_BLE);
    
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    
    /* Register GAP callbacks */
    esp_bt_gap_register_callback(ble_gap_event_handler);
    
    /* Register GATT callbacks */
    esp_gattc_register_callback(ble_gatt_event_handler);
    
    /* Set device name */
    esp_bt_gap_set_device_name(BLE_DEVICE_NAME);
    
    /* Enable BLE discoverability */
    esp_bt_gap_set_scan_mode(ESP_BT_GAP_CONNECTABLE_DISCOVERABLE, 
                            BLE_ADVERTISING_INTERVAL, 
                            BLE_ADVERTISING_INTERVAL);
    
    ESP_LOGI("BLE", "BLE Transport initialized, mode: BLE only");
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          GAP Event Handler                   */
/* ------------------------------------------------------------ */
static void ble_gap_event_handler(esp_bt_gap_cb_event_t event, 
                                  esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_CALLBACK_STATUS_EVT:
            ESP_LOGI("BLE", "Gap callback status: %d", param->status.status);
            break;
            
        case ESP_BT_GAP_SET_LOCAL_NAME_COMPLETE_EVT:
            ESP_LOGI("BLE", "Local name set complete");
            break;
            
        case ESP_BT_GAP_ADV_DATA_RAW_SET_COMPLETE_EVT:
            ESP_LOGI("BLE", "Advertising data set complete");
            break;
            
        case ESP_BT_GAP_START_COMPLETE_EVT: {
            /* Advertising complete or timed out */
            bool disc_mode = (param->start_compplete.status == ESP_BT_STATUS_SUCCESS);
            if (disc_mode) {
                ESP_LOGI("BLE", "Advertising started successfully");
            } else {
                ESP_LOGW("BLE", "Advertising timed out, restarting...");
                esp_bt_gap_start_advertising(ESP_BT_ADV_PARAM_TYPE_UND_iCON);
            }
            break;
        }
            
        case ESP_BT_GAP_CONNECT_EVT: {
            /* New connection established */
            g_conn_handle = param->connect.conn_id;
            g_bonding_done = 1;
            
            ESP_LOGI("BLE", "Connected! Handle: %d, Address: [%02x:%02x:%02x:%02x:%02x:%02x]",
                    g_conn_handle,
                    param->connect.bda.address[0],
                    param->connect.bda.address[1],
                    param->connect.bda.address[2],
                    param->connect.bda.address[3],
                    param->connect.bda.address[4],
                    param->connect.bda.address[5]);
            
            /* Call registered callback */
            if (g_connect_cbfn) {
                g_connect_cbfn(param->connect.status, g_conn_handle);
            }
            
            /* Start notifications for heart rate service */
            start_ble_notifications(g_conn_handle);
            break;
        }
            
        case ESP_BT_GAP_DISCONNECT_EVT: {
            /* Connection lost */
            g_conn_handle = 0;
            g_bonding_done = 0;
            
            ESP_LOGW("BLE", "Disconnected! Reason: %d", param->disconnect.reason);
            
            /* Restart advertising */
            esp_bt_gap_start_advertising(ESP_BT_ADV_PARAM_TYPE_UND_iCON);
            
            /* Call registered callback */
            if (g_disconnect_cbfn) {
                g_disconnect_cbfn(param->disconnect.reason);
            }
            break;
        }
            
        default:
            break;
    }
}

/* ------------------------------------------------------------ */
/*                          GATT Event Handler                  */
/* ------------------------------------------------------------ */
static void ble_gatt_event_handler(esp_gatt_cb_event_t event, 
                                   esp_gatt_cb_param_t *param) {
    if (event == ESP_GATTC_REG_EVT) {
        ESP_LOGI("GATT", "GATT registered");
        
        /* After registration, start discovering services */
        esp_bt_gap_search_ble_devices(80);  /* 80s scan */
        
    } else if (event == ESP_GATTC_OPEN_EVT) {
        ESP_LOGI("GATT", "Service opened, handle: %d", param->open.attr_handle);
        
    } else if (event == ESP_GATTC_NOTIFY_EVT) {
        /* Notification received from peripheral */
        if (param->notify.is_notify) {
            ESP_LOGD("GATT", "Notify: handle=%d, len=%d", 
                    param->notify.attr_handle, param->notify.length);
            
            /* Forward to application callback */
            if (g_data_cbfn && param->notify.length > 0) {
                g_data_cbfn(param->notify.value, param->notify.length);
            }
        }
        
    } else if (event == ESP_GATTC_WRITE_DESCR_EVT) {
        ESP_LOGI("GATT", "Descriptor write %s", 
                param->write_descr.status == ESP_GATT_OK ? "success" : "failed");
    }
}

/* ------------------------------------------------------------ */
/*                          BLE Data Transmission               */
/* ------------------------------------------------------------ */
esp_err_t ble_transport_send_data(const void *data, uint16_t len) {
    if (g_conn_handle == 0) {
        ESP_LOGW("BLE", "Not connected, cannot send data");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (len > MAX_NOTIFICATION_PAYLOAD) {
        ESP_LOGW("BLE", "Payload too large (%d > %d), truncating", len, MAX_NOTIFICATION_PAYLOAD);
        len = MAX_NOTIFICATION_PAYLOAD;
    }
    
    /* Send notification via GATT */
    esp_err_t ret = esp_gattc_send_blob(g_attc_if, g_conn_handle,
                                       g_notify_index, (uint8_t *)data, len);
    
    if (ret != ESP_OK) {
        ESP_LOGE("BLE", "Failed to send notification: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* ------------------------------------------------------------ */
/*                          BLE Alert Transmission              */
/* ------------------------------------------------------------ */
esp_err_t ble_transport_send_alert(uint8_t alert_type) {
    /* Alert format: 1-byte type + optional payload */
    uint8_t alert_payload[2];
    alert_payload[0] = alert_type;
    alert_payload[1] = 0x00; /* No additional data */
    
    return ble_transport_send_data(alert_payload, 2);
}

/* ------------------------------------------------------------ */
 /*                          BLE OTA Support                     */
/* ------------------------------------------------------------ */
esp_err_t ble_transport_check_firmware_update(void) {
    /* In a full implementation, this would:
     * 1. Query cloud for latest firmware version
     * 2. Compare with current firmware hash
     * 3. Request delta from OTA server
     * 4. Validate signature
     * 5. Trigger secure bootloader update
     */

    /* Check if connected to BLE - OTA requires active connection */
    if (g_conn_handle == 0) {
        ESP_LOGW("BLE", "Not connected, cannot check firmware update");
        return ESP_ERR_INVALID_STATE;
    }

    /* Placeholder: query cloud for latest version via existing cloud channel
     * In production, would use MQTT or HTTP to fetch latest firmware info
     * esp_mqtt_client_publish(client, ..., "firmware/check", ...);
     */
    ESP_LOGI("BLE", "Firmware check: infrastructure not yet integrated");
    return ESP_OK; /* Report success so host can decide; or ESP_FAIL to disable */
}

/* ------------------------------------------------------------ */
/*                          BLE Connection Management           */
/* ------------------------------------------------------------ */
esp_err_t ble_transport_start_advertising(void) {
    esp_err_t ret;
    esp_bt_gap_adv_params_t adv_params = {
        .adv_int_min = BLE_ADVERTISING_INTERVAL,
        .adv_int_max = BLE_ADVERTISING_INTERVAL,
        .adv_type = ESP_BT_ADV_TYPE_ADV_IND,
        .own_addr_type = ESP_BD_ADDR_TYPE_PUBLIC,
        .channel_map = ESP_BT_CHNL_MAP_ALL,
        .adv_filter_policy = ESP_BT_ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    
    ret = esp_bt_gap_start_advertising(&adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE("BLE", "Failed to start advertising: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* ------------------------------------------------------------ */
/*                          BLE Registration callbacks          */
/* ------------------------------------------------------------ */
void ble_transport_register_data_callback(ble_notify_cbfn_t cbfn) {
    g_data_cbfn = cbfn;
}

void ble_transport_register_connect_callback(ble_connect_cbfn_t cbfn) {
    g_connect_cbfn = cbfn;
}

void ble_transport_register_disconnect_callback(ble_disconnect_cbfn_t cbfn) {
    g_disconnect_cbfn = cbfn;
}

/* ------------------------------------------------------------ */
/*                          BLE Task                            */
/* ------------------------------------------------------------ */
static void vBleTask(void *pvParameters) {
    for (;;) {
        /* Maintain BLE stack - handled by event callbacks */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------ */
/*                          Main Entry                          */
/* ------------------------------------------------------------ */
void ble_transport_start(void) {
    /* Initialize BLE */
    esp_err_t ret = ble_transport_init();
    if (ret != ESP_OK) {
        ESP_LOGE("BLE", "Initialization failed");
        return;
    }
    
    /* Start advertising */
    ret = ble_transport_start_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE("BLE", "Advertising start failed");
        return;
    }
    
    /* Create BLE maintenance task */
    xTaskCreate(vBleTask, "BLETask", 512, NULL, 1, NULL);
    
    ESP_LOGI("BLE", "BLE transport started - advertising as %s", BLE_DEVICE_NAME);
}