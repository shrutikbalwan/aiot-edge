#pragma once

#include "aiot_types.h"
#include "esp_err.h"

esp_err_t ota_manager_init(void);
esp_err_t ota_manager_start(const char *https_url, const char *expected_version);
esp_err_t ota_manager_get_status(aiot_ota_status_t *status);
