#pragma once

#include "aiot_types.h"
#include "esp_err.h"

esp_err_t mqtt_transport_init(void);
esp_err_t mqtt_transport_publish_telemetry(const aiot_sensor_sample_t *sample,
                                            const aiot_health_estimate_t *estimate);
