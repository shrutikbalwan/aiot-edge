#pragma once

#include <stddef.h>

#include "aiot_types.h"
#include "esp_err.h"

#define MAX30102_I2C_ADDRESS 0x57U
#define MAX30205_I2C_ADDRESS 0x48U
#define BMI160_I2C_ADDRESS_MIN 0x68U
#define BMI160_I2C_ADDRESS_MAX 0x69U

esp_err_t sensor_driver_init(void);
esp_err_t sensor_driver_read(aiot_sensor_sample_t *sample);
esp_err_t sensor_analyze_window(const aiot_sensor_sample_t *samples, size_t count,
                                uint32_t sample_rate_hz,
                                aiot_health_estimate_t *estimate);
