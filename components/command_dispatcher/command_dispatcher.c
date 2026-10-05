#include "command_dispatcher.h"

#include <stdlib.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ota_manager.h"

#define COMMAND_QUEUE_DEPTH 4U
#define COMMAND_TASK_STACK 4096U
#define COMMAND_TASK_PRIORITY 3U
#define REBOOT_DELAY_MS 1000U

typedef struct {
    aiot_command_t command;
    aiot_command_source_t source;
} queued_command_t;

static const char *TAG = "command_dispatcher";
static QueueHandle_t s_queue;
static TaskHandle_t s_task;

static void command_task(void *argument)
{
    (void)argument;
    for (;;) {
        queued_command_t queued = {0};
        if (xQueueReceive(s_queue, &queued, portMAX_DELAY) != pdPASS) continue;
        switch (queued.command.kind) {
        case AIOT_COMMAND_STATUS:
            ESP_LOGI(TAG, "authenticated status request from source=%d", queued.source);
            break;
        case AIOT_COMMAND_OTA: {
            const esp_err_t err = ota_manager_start(queued.command.ota_url,
                                                    queued.command.version);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "unable to start authenticated OTA: %s",
                         esp_err_to_name(err));
            }
            break;
        }
        case AIOT_COMMAND_REBOOT:
            ESP_LOGW(TAG, "authenticated reboot requested; rebooting in %u ms",
                     REBOOT_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(REBOOT_DELAY_MS));
            esp_restart();
            break;
        default:
            ESP_LOGW(TAG, "unsupported queued command kind=%d", queued.command.kind);
            break;
        }
    }
}

esp_err_t command_dispatcher_init(void)
{
    s_queue = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(queued_command_t));
    if (s_queue == NULL) return ESP_ERR_NO_MEM;
    if (xTaskCreate(command_task, "aiot_command", COMMAND_TASK_STACK, NULL,
                    COMMAND_TASK_PRIORITY, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t command_dispatcher_submit(const aiot_command_t *command,
                                    aiot_command_source_t source,
                                    bool authenticated)
{
    if (command == NULL || command->kind == AIOT_COMMAND_NONE) return ESP_ERR_INVALID_ARG;
    if (!authenticated) {
        ESP_LOGW(TAG, "rejected unauthenticated command from source=%d", source);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_queue == NULL) return ESP_ERR_INVALID_STATE;
    const queued_command_t queued = {.command = *command, .source = source};
    return xQueueSend(s_queue, &queued, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
