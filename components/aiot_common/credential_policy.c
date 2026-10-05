#include "credential_policy.h"

#include <string.h>

bool aiot_wifi_credentials_valid(const char *ssid, const char *password)
{
    return ssid != NULL && password != NULL && strlen(ssid) > 0U &&
           strlen(ssid) <= 32U && strlen(password) <= 64U;
}

bool aiot_mqtt_commands_authorized(bool tls, const char *username,
                                   const char *password)
{
    return tls && username != NULL && password != NULL &&
           username[0] != '\0' && password[0] != '\0';
}
