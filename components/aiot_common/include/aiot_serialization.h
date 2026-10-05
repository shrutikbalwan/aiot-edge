#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aiot_types.h"
#include "esp_err.h"

esp_err_t aiot_serialize_telemetry_json(const aiot_sensor_sample_t *sample,
                                        const aiot_health_estimate_t *estimate,
                                        char *output, size_t output_size);
esp_err_t aiot_serialize_ble_telemetry(const aiot_sensor_sample_t *sample,
                                       const aiot_health_estimate_t *estimate,
                                       uint8_t *output, size_t output_size,
                                       size_t *written);
esp_err_t aiot_parse_command_json(const char *payload, size_t payload_size,
                                  aiot_command_t *command);
