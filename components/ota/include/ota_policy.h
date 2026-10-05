#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "aiot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ota_policy_request_valid(const char *url, const char *version);
bool ota_policy_version_is_newer(const char *current, const char *incoming);
bool ota_policy_transition_allowed(aiot_ota_state_t from, aiot_ota_state_t to);

#ifdef __cplusplus
}
#endif
