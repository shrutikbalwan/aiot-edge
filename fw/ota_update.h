/**
 * @file ota_update.h
 * @brief OTA Firmware Update Header for AIoT-Edge
 * @brief Secure over-the-air firmware updates with hash validation
 */

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include "ble_transport.h"

/* ------------------------------------------------------------ */
/*                          Callback Function Type              */
/* ------------------------------------------------------------ */
typedef void (*ota_update_cbfn_t)(uint8_t status, void *user_data);

/* ------------------------------------------------------------ */
/*                          OTA Transfer Status                 */
/* ------------------------------------------------------------ */
#define OTA_STATUS_OK              0x00
#define OTA_STATUS_IN_PROGRESS     0x01
#define OTA_STATUS_BLOCK_OK        0x02
#define OTA_STATUS_VERIFY_FAIL     0x03
#define OTA_STATUS_ABORT           0xFF

/* ------------------------------------------------------------ */
/*                          Function Prototypes                 */
/* ------------------------------------------------------------ */
esp_err_t ota_update_init(void);
esp_err_t ota_update_start(const esp_partition_t *partition,
                          ota_update_cbfn_t cbfn, void *user_data);
esp_err_t ota_update_write(const uint8_t *data, uint16_t len);
esp_err_t ota_update_finish(void);
esp_err_t ota_update_abort(void);
uint8_t ota_update_get_status(void);
bool ota_update_is_in_progress(void);
esp_err_t ota_update_request_abort(void);

/* ------------------------------------------------------------ */
/*                          OTA Service UUIDs                   */
/* ------------------------------------------------------------ */
#define OTA_SERVICE_UUID   0x1825
#define OTA_CHAR_WRITE_UUID 0x2A56
#define OTA_CHAR_NOTIFY_UUID 0x2A57

/* ------------------------------------------------------------ */
/*                          Maximum OTA Packet Size             */
/* ------------------------------------------------------------ */
#define MAX_OTA_PAYLOAD_SIZE 512

/* ------------------------------------------------------------ */
/*                          OTA Configuration                   */
/* ------------------------------------------------------------ */
#define OTA_ERASE_TIMEOUT_MS  1000
#define OTA_WRITE_TIMEOUT_MS  500
#define OTA_VERIFY_TIMEOUT_MS 2000

#endif /* OTA_UPDATE_H */