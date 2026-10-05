#include "ota_policy.h"

#include <stdint.h>
#include <string.h>

static bool parse_component(const char **cursor, uint32_t *value)
{
    const char *p = *cursor;
    if (*p < '0' || *p > '9') return false;
    uint32_t result = 0;
    do {
        const uint32_t digit = (uint32_t)(*p - '0');
        if (result > (UINT32_MAX - digit) / 10U) return false;
        result = result * 10U + digit;
        ++p;
    } while (*p >= '0' && *p <= '9');
    *cursor = p;
    *value = result;
    return true;
}

static bool parse_version(const char *text, uint32_t parts[3], const char **suffix)
{
    if (text == NULL || *text == '\0') return false;
    const char *cursor = text;
    for (size_t i = 0; i < 3U; ++i) {
        if (!parse_component(&cursor, &parts[i])) return false;
        if (i < 2U && *cursor == '.') {
            ++cursor;
        } else {
            for (++i; i < 3U; ++i) parts[i] = 0U;
            break;
        }
    }
    if (*cursor != '\0' && *cursor != '-') return false;
    *suffix = cursor;
    return true;
}

bool ota_policy_request_valid(const char *url, const char *version)
{
    return url != NULL && version != NULL && strncmp(url, "https://", 8U) == 0 &&
           strlen(url) < AIOT_OTA_URL_MAX && strlen(version) > 0U &&
           strlen(version) < AIOT_VERSION_MAX;
}

bool ota_policy_version_is_newer(const char *current, const char *incoming)
{
    uint32_t current_parts[3] = {0};
    uint32_t incoming_parts[3] = {0};
    const char *current_suffix = NULL;
    const char *incoming_suffix = NULL;
    if (!parse_version(current, current_parts, &current_suffix) ||
        !parse_version(incoming, incoming_parts, &incoming_suffix)) {
        return false;
    }
    for (size_t i = 0; i < 3U; ++i) {
        if (incoming_parts[i] != current_parts[i]) {
            return incoming_parts[i] > current_parts[i];
        }
    }
    /* A release is newer than a prerelease with the same numeric core. */
    return *current_suffix == '-' && *incoming_suffix == '\0';
}

bool ota_policy_transition_allowed(aiot_ota_state_t from, aiot_ota_state_t to)
{
    if (to == AIOT_OTA_FAILED) return from != AIOT_OTA_READY_TO_REBOOT;
    switch (from) {
    case AIOT_OTA_IDLE:
    case AIOT_OTA_FAILED:
        return to == AIOT_OTA_DOWNLOADING;
    case AIOT_OTA_DOWNLOADING:
        return to == AIOT_OTA_DOWNLOADING || to == AIOT_OTA_VERIFYING;
    case AIOT_OTA_VERIFYING:
        return to == AIOT_OTA_READY_TO_REBOOT;
    case AIOT_OTA_READY_TO_REBOOT:
    default:
        return false;
    }
}
