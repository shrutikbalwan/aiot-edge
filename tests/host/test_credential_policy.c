#include <assert.h>
#include <stddef.h>

#include "credential_policy.h"

int main(void)
{
    assert(aiot_wifi_credentials_valid("network", ""));
    assert(aiot_wifi_credentials_valid("network", "correct horse battery staple"));
    assert(!aiot_wifi_credentials_valid("", "password"));
    assert(!aiot_wifi_credentials_valid(NULL, "password"));

    assert(aiot_mqtt_commands_authorized(true, "device", "runtime-secret"));
    assert(!aiot_mqtt_commands_authorized(false, "device", "runtime-secret"));
    assert(!aiot_mqtt_commands_authorized(true, "", "runtime-secret"));
    assert(!aiot_mqtt_commands_authorized(true, "device", ""));
    return 0;
}
