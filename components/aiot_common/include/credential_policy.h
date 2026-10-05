#pragma once

#include <stdbool.h>

bool aiot_wifi_credentials_valid(const char *ssid, const char *password);
bool aiot_mqtt_commands_authorized(bool tls, const char *username,
                                   const char *password);
