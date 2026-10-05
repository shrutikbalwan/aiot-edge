#include "ota_manager.h"
#include "ota_policy.h"

#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define OTA_TASK_STACK 8192U
#define OTA_TASK_PRIORITY 3U
#define OTA_HTTP_TIMEOUT_MS 15000U

typedef struct {
    char url[AIOT_OTA_URL_MAX];
    char version[AIOT_VERSION_MAX];
} ota_request_t;

static SemaphoreHandle_t s_status_mutex;
static aiot_ota_status_t s_status;
#if CONFIG_AIOT_OTA_ENABLED
static const char *TAG = "ota_manager";
static TaskHandle_t s_ota_task;

static void set_status(aiot_ota_state_t state, uint8_t progress, esp_err_t error,
                       const char *version)
{
    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!ota_policy_transition_allowed(s_status.state, state)) {
            ESP_LOGE(TAG, "invalid OTA state transition %d -> %d",
                     (int)s_status.state, (int)state);
            xSemaphoreGive(s_status_mutex);
            return;
        }
        s_status.state = state;
        s_status.progress_percent = progress;
        s_status.error_code = error;
        if (version != NULL) {
            strlcpy(s_status.target_version, version, sizeof(s_status.target_version));
        }
        xSemaphoreGive(s_status_mutex);
    }
}

static void ota_task(void *argument)
{
    ota_request_t request = *(ota_request_t *)argument;
    free(argument);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (running == NULL || next == NULL || running == next) {
        set_status(AIOT_OTA_FAILED, 0, ESP_ERR_NOT_FOUND, request.version);
        s_ota_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    const esp_http_client_config_t http_config = {
        .url = request.url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
    };
    const esp_https_ota_config_t ota_config = {.http_config = &http_config};
    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) goto failure;

    esp_app_desc_t incoming = {0};
    err = esp_https_ota_get_img_desc(handle, &incoming);
    if (err != ESP_OK) goto abort_update;
    const esp_app_desc_t *current = esp_app_get_description();
    if (strncmp(incoming.version, request.version, sizeof(incoming.version)) != 0) {
        err = ESP_ERR_OTA_VALIDATE_FAILED;
        goto abort_update;
    }
#if !CONFIG_AIOT_ALLOW_OTA_DOWNGRADE
    if (!ota_policy_version_is_newer(current->version, incoming.version)) {
        err = ESP_ERR_OTA_VALIDATE_FAILED;
        goto abort_update;
    }
#endif

    set_status(AIOT_OTA_DOWNLOADING, 0, ESP_OK, request.version);
    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        const int total = esp_https_ota_get_image_size(handle);
        const int received = esp_https_ota_get_image_len_read(handle);
        const uint8_t progress = total > 0
            ? (uint8_t)((received * 100LL) / total) : 0U;
        set_status(AIOT_OTA_DOWNLOADING, progress, ESP_OK, request.version);
        vTaskDelay(1);
    }
    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) goto abort_update;
    set_status(AIOT_OTA_VERIFYING, 100, ESP_OK, request.version);
    err = esp_https_ota_finish(handle);
    handle = NULL;
    if (err != ESP_OK) goto failure;
    set_status(AIOT_OTA_READY_TO_REBOOT, 100, ESP_OK, request.version);
    ESP_LOGI(TAG, "verified image selected for next boot; reboot remains explicit");
    s_ota_task = NULL;
    vTaskDelete(NULL);
    return;

abort_update:
    if (handle != NULL) esp_https_ota_abort(handle);
failure:
    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
    set_status(AIOT_OTA_FAILED, 0, err, request.version);
    s_ota_task = NULL;
    vTaskDelete(NULL);
}
#endif

esp_err_t ota_manager_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    s_status_mutex = xSemaphoreCreateMutex();
    if (s_status_mutex == NULL) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

esp_err_t ota_manager_start(const char *https_url, const char *expected_version)
{
    if (!ota_policy_request_valid(https_url, expected_version)) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_AIOT_OTA_ENABLED
    if (s_ota_task != NULL) return ESP_ERR_INVALID_STATE;
    ota_request_t *request = calloc(1, sizeof(*request));
    if (request == NULL) return ESP_ERR_NO_MEM;
    strlcpy(request->url, https_url, sizeof(request->url));
    strlcpy(request->version, expected_version, sizeof(request->version));
    if (xTaskCreate(ota_task, "aiot_ota", OTA_TASK_STACK, request, OTA_TASK_PRIORITY,
                    &s_ota_task) != pdPASS) {
        free(request);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t ota_manager_get_status(aiot_ota_status_t *status)
{
    if (status == NULL || s_status_mutex == NULL) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    *status = s_status;
    xSemaphoreGive(s_status_mutex);
    return ESP_OK;
}
