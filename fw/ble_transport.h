/**
 * @file ble_transport.h
 * @brief BLE Transport Layer Header for AIoT-Edge
 * @brief Bluetooth Low Energy 5.3 declarations and types
 */

#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------ */
/*                                  Callback Types              */
/* ------------------------------------------------------------ */
/** @brief Callback for BLE data notifications */
typedef void (*ble_notify_cbfn_t)(const uint8_t *data, uint16_t len);

/** @brief Callback for BLE connection status */
typedef void (*ble_connect_cbfn_t)(esp_err_t status, uint16_t conn_handle);

/** @brief Callback for BLE disconnection reason */
typedef void (*ble_disconnect_cbfn_t)(uint8_t reason);

/* ------------------------------------------------------------ */
/*                                  Macro Definitions           */
/* ------------------------------------------------------------ */
#define BLE_DEVICE_NAME           "AIoT-Edge-HM"
#define BLE_MAX_NOTIFY_PAYLOAD    20
#define BLE_ADV_INTERVAL          160  /* 100ms units */

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
/** @brief Initialize BLE transport layer */
esp_err_t ble_transport_init(void);

/** @brief Start BLE advertising */
esp_err_t ble_transport_start_advertising(void);

/** @brief Send data over BLE connection */
esp_err_t ble_transport_send_data(const void *data, uint16_t len);

/** @brief Send alert over BLE */
esp_err_t ble_transport_send_alert(uint8_t alert_type);

/** @brief Check for firmware update request */
bool ble_transport_check_firmware_update(void);

/** @Register BLE callbacks */
void ble_transport_register_data_callback(ble_notify_cbfn_t cbfn);
void ble_transport_register_connect_callback(ble_connect_cbfn_t cbfn);
void ble_transport_register_disconnect_callback(ble_disconnect_cbfn_t cbfn);

/** @brief Start BLE transport (advertising + tasks) */
void ble_transport_start(void);

#endif /* BLE_TRANSPORT_H */