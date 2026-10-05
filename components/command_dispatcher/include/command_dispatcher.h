#pragma once

#include <stdbool.h>

#include "aiot_types.h"
#include "esp_err.h"

typedef enum {
    AIOT_COMMAND_SOURCE_MQTT = 0,
    AIOT_COMMAND_SOURCE_BLE,
} aiot_command_source_t;

esp_err_t command_dispatcher_init(void);
esp_err_t command_dispatcher_submit(const aiot_command_t *command,
                                    aiot_command_source_t source,
                                    bool authenticated);
