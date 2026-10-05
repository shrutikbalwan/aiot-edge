#include <assert.h>

#include "ota_policy.h"

int main(void)
{
    assert(ota_policy_request_valid("https://updates.example/fw.bin", "1.2.3"));
    assert(!ota_policy_request_valid("http://updates.example/fw.bin", "1.2.3"));
    assert(!ota_policy_request_valid("https://updates.example/fw.bin", ""));

    assert(ota_policy_version_is_newer("1.9.0", "1.10.0"));
    assert(ota_policy_version_is_newer("2.0.0-rc1", "2.0.0"));
    assert(!ota_policy_version_is_newer("2.0.0", "2.0.0"));
    assert(!ota_policy_version_is_newer("2.0.0", "1.99.0"));
    assert(!ota_policy_version_is_newer("development", "next"));

    assert(ota_policy_transition_allowed(AIOT_OTA_IDLE, AIOT_OTA_DOWNLOADING));
    assert(ota_policy_transition_allowed(AIOT_OTA_DOWNLOADING, AIOT_OTA_VERIFYING));
    assert(ota_policy_transition_allowed(AIOT_OTA_VERIFYING, AIOT_OTA_READY_TO_REBOOT));
    assert(ota_policy_transition_allowed(AIOT_OTA_DOWNLOADING, AIOT_OTA_FAILED));
    assert(!ota_policy_transition_allowed(AIOT_OTA_IDLE, AIOT_OTA_READY_TO_REBOOT));
    assert(!ota_policy_transition_allowed(AIOT_OTA_READY_TO_REBOOT, AIOT_OTA_FAILED));
    return 0;
}
