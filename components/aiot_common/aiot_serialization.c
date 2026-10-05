#include "aiot_serialization.h"

#include <string.h>

#include "cJSON.h"

#define BLE_TELEMETRY_SIZE 25U
#define COMMAND_PAYLOAD_MAX 512U

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i) {
        dst[i] = (uint8_t)(value >> (i * 8U));
    }
}

esp_err_t aiot_serialize_telemetry_json(const aiot_sensor_sample_t *sample,
                                        const aiot_health_estimate_t *estimate,
                                        char *output, size_t output_size)
{
    if (sample == NULL || estimate == NULL || output == NULL || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "schema", AIOT_PROTOCOL_VERSION);
    cJSON_AddNumberToObject(root, "timestamp_ms", (double)sample->timestamp_ms);
    cJSON_AddNumberToObject(root, "red", sample->red);
    cJSON_AddNumberToObject(root, "infrared", sample->infrared);
    cJSON_AddNumberToObject(root, "temperature_milli_c", sample->temperature_milli_c);
    cJSON_AddNumberToObject(root, "accel_x_mg", sample->accel_x_mg);
    cJSON_AddNumberToObject(root, "accel_y_mg", sample->accel_y_mg);
    cJSON_AddNumberToObject(root, "accel_z_mg", sample->accel_z_mg);
    cJSON_AddBoolToObject(root, "simulated", sample->simulated);
    if (estimate->heart_rate_valid) {
        cJSON_AddNumberToObject(root, "heart_rate_bpm", estimate->heart_rate_bpm);
    }
    if (estimate->spo2_valid) {
        cJSON_AddNumberToObject(root, "spo2_permille", estimate->spo2_permille);
    }
    const bool ok = cJSON_PrintPreallocated(root, output, (int)output_size, false);
    cJSON_Delete(root);
    return ok ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t aiot_serialize_ble_telemetry(const aiot_sensor_sample_t *sample,
                                       const aiot_health_estimate_t *estimate,
                                       uint8_t *output, size_t output_size,
                                       size_t *written)
{
    if (sample == NULL || estimate == NULL || output == NULL || written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (output_size < BLE_TELEMETRY_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }
    output[0] = AIOT_PROTOCOL_VERSION;
    output[1] = (uint8_t)((sample->simulated ? 1U : 0U) |
                          (estimate->heart_rate_valid ? 2U : 0U) |
                          (estimate->spo2_valid ? 4U : 0U));
    put_u32_le(&output[2], (uint32_t)sample->timestamp_ms);
    put_u32_le(&output[6], sample->red);
    put_u32_le(&output[10], sample->infrared);
    put_u32_le(&output[14], (uint32_t)sample->temperature_milli_c);
    put_u16_le(&output[18], (uint16_t)sample->accel_x_mg);
    put_u16_le(&output[20], (uint16_t)sample->accel_y_mg);
    put_u16_le(&output[22], (uint16_t)sample->accel_z_mg);
    output[24] = estimate->heart_rate_valid ? (uint8_t)estimate->heart_rate_bpm : 0U;
    *written = BLE_TELEMETRY_SIZE;
    return ESP_OK;
}

esp_err_t aiot_parse_command_json(const char *payload, size_t payload_size,
                                  aiot_command_t *command)
{
    if (payload == NULL || command == NULL || payload_size == 0 ||
        payload_size > COMMAND_PAYLOAD_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(command, 0, sizeof(*command));
    cJSON *root = cJSON_ParseWithLength(payload, payload_size);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (!cJSON_IsNumber(schema) || schema->valueint != AIOT_PROTOCOL_VERSION ||
        !cJSON_IsString(name) || name->valuestring == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(name->valuestring, "status") == 0) {
        command->kind = AIOT_COMMAND_STATUS;
    } else if (strcmp(name->valuestring, "reboot") == 0) {
        command->kind = AIOT_COMMAND_REBOOT;
    } else if (strcmp(name->valuestring, "ota") == 0) {
        const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        if (!cJSON_IsString(url) || !cJSON_IsString(version) || url->valuestring == NULL ||
            version->valuestring == NULL || strncmp(url->valuestring, "https://", 8) != 0 ||
            strlen(url->valuestring) >= sizeof(command->ota_url) ||
            strlen(version->valuestring) >= sizeof(command->version)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        command->kind = AIOT_COMMAND_OTA;
        memcpy(command->ota_url, url->valuestring, strlen(url->valuestring) + 1U);
        memcpy(command->version, version->valuestring, strlen(version->valuestring) + 1U);
    } else {
        cJSON_Delete(root);
        return ESP_ERR_NOT_SUPPORTED;
    }
    cJSON_Delete(root);
    return ESP_OK;
}
