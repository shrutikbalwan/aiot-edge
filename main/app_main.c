#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "aiot_types.h"
#include "audio_features.h"
#include "ble_transport.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "model_runtime.h"
#include "mqtt_transport.h"
#include "nvs_flash.h"
#include "ota_manager.h"
#include "sensor_driver.h"

#define SENSOR_TASK_STACK 4096U
#define PROCESS_TASK_STACK 4096U
#define SENSOR_TASK_PRIORITY 5U
#define PROCESS_TASK_PRIORITY 4U
#define SENSOR_PERIOD_MS 100U

#ifdef CONFIG_AIOT_BLE_ENABLED
#define AIOT_BLE_ENABLED_VALUE 1
#else
#define AIOT_BLE_ENABLED_VALUE 0
#endif
#ifdef CONFIG_AIOT_MQTT_ENABLED
#define AIOT_MQTT_ENABLED_VALUE 1
#else
#define AIOT_MQTT_ENABLED_VALUE 0
#endif
#ifdef CONFIG_AIOT_OTA_ENABLED
#define AIOT_OTA_ENABLED_VALUE 1
#else
#define AIOT_OTA_ENABLED_VALUE 0
#endif

static const char *TAG = "aiot_main";
static QueueHandle_t s_sensor_queue;
static EventGroupHandle_t s_system_events;
static TaskHandle_t s_sensor_task;
static TaskHandle_t s_process_task;

static void sensor_task(void *context)
{
    (void)context;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        aiot_sensor_sample_t sample = {0};
        const esp_err_t err = sensor_driver_read(&sample);
        if (err == ESP_OK) {
            if (xQueueOverwrite(s_sensor_queue, &sample) != pdPASS) {
                ESP_LOGW(TAG, "sensor queue write failed");
            }
        } else {
            ESP_LOGW(TAG, "sensor read failed: %s", esp_err_to_name(err));
        }
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

static void processing_task(void *context)
{
    (void)context;
    aiot_sensor_sample_t window[AIOT_PPG_WINDOW_SAMPLES] = {0};
    size_t used = 0;

    for (;;) {
        aiot_sensor_sample_t sample = {0};
        if (xQueueReceive(s_sensor_queue, &sample, pdMS_TO_TICKS(1000)) != pdPASS) {
            ESP_LOGW(TAG, "sensor data stale");
            continue;
        }

        window[used++] = sample;
        if (used == AIOT_PPG_WINDOW_SAMPLES) {
            aiot_health_estimate_t estimate = {0};
            sensor_analyze_window(window, used, AIOT_SENSOR_SAMPLE_RATE_HZ, &estimate);
            mqtt_transport_publish_telemetry(&sample, &estimate);
            aiot_ble_transport_notify_telemetry(&sample, &estimate);
            ESP_LOGI(TAG, "educational estimate: HR=%" PRIu16 " bpm valid=%d",
                     estimate.heart_rate_bpm, estimate.heart_rate_valid);
            used = 0;
        }
    }
}

static esp_err_t create_runtime_resources(void)
{
    s_sensor_queue = xQueueCreate(1, sizeof(aiot_sensor_sample_t));
    s_system_events = xEventGroupCreate();
    if (s_sensor_queue == NULL || s_system_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(create_runtime_resources());
    ESP_ERROR_CHECK(sensor_driver_init());
    ESP_ERROR_CHECK(audio_features_init());
    ESP_ERROR_CHECK(model_runtime_init());
    ESP_ERROR_CHECK(ota_manager_init());
    ESP_ERROR_CHECK(mqtt_transport_init());
    ESP_ERROR_CHECK(aiot_ble_transport_init());

    BaseType_t created = xTaskCreate(sensor_task, "aiot_sensor", SENSOR_TASK_STACK, NULL,
                                     SENSOR_TASK_PRIORITY, &s_sensor_task);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "unable to create sensor task");
        abort();
    }
    created = xTaskCreate(processing_task, "aiot_process", PROCESS_TASK_STACK, NULL,
                          PROCESS_TASK_PRIORITY, &s_process_task);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "unable to create processing task");
        abort();
    }

    ESP_LOGI(TAG, "started on ESP32-S3 (simulation=%d, BLE=%d, MQTT=%d, OTA=%d)",
             CONFIG_AIOT_SIMULATION, AIOT_BLE_ENABLED_VALUE,
             AIOT_MQTT_ENABLED_VALUE, AIOT_OTA_ENABLED_VALUE);
}
