#ifndef BLE_SEC_UPDATE_H
#define BLE_SEC_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include "ble_transport.h"

/* ------------------------------------------------------------ */
/*                          Security Status Types               */
/* ------------------------------------------------------------ */
typedef struct {
    bool bonding_done;
    bool privacy_mode;
    bool random_addr;
} ble_sec_status_t;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
esp_err_t ble_sec_init(void);
esp_err_t ble_sec_start_bonding(void);
esp_err_t ble_sec_abort_bonding(void);
esp_err_t ble_sec_set_privacy_mode(uint8_t mode);
uint8_t ble_sec_get_status(void);
ble_sec_status_t ble_sec_get_status_extended(void);
esp_err_t ble_sec_refresh_address(void);

/* ------------------------------------------------------------ */
/*                          Security Configuration              */
/* ------------------------------------------------------------ */
#define BLE_SEC_BONDING_TIMEOUT_SEC  300
#define BLE_SEC_MAX_BOND_KEYS        4
#define BLE_SEC_PRIVACY_ENABLED      1
#define BLE_SEC_PRIVACY_DISABLED     0

/* ------------------------------------------------------------ */
/*                          BLE Security Tags                   */
/* ------------------------------------------------------------ */
#define BLE_SEC_TAG "BLE_SEC"

/* ------------------------------------------------------------ */
/*                          Maximum Key Size                    */
/* ------------------------------------------------------------ */
#define MAX_KEY_SIZE 16

/* ------------------------------------------------------------ */
/*                          End of Header                         */
/* ------------------------------------------------------------ */
#endif /* BLE_SEC_UPDATE_H */