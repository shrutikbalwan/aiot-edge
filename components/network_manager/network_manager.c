#include "network_manager.h"

#include <string.h>

#include "credential_policy.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "nvs.h"

#define WIFI_NAMESPACE "aiot_network"
#define WIFI_SSID_KEY "wifi_ssid"
#define WIFI_PASSWORD_KEY "wifi_pass"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_INITIAL_BACKOFF_MS 1000U

static const char *TAG = "network_manager";

#if CONFIG_AIOT_WIFI_ENABLED
static EventGroupHandle_t s_events;
static TimerHandle_t s_reconnect_timer;
static uint32_t s_reconnect_backoff_ms = WIFI_INITIAL_BACKOFF_MS;
static bool s_initialized;

static esp_err_t read_string(nvs_handle_t handle, const char *key,
                             char *output, size_t output_size)
{
    size_t required = output_size;
    const esp_err_t err = nvs_get_str(handle, key, output, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK || required == 0U || required > output_size) {
        return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
    }
    return ESP_OK;
}

static void reconnect_timer_callback(TimerHandle_t timer)
{
    (void)timer;
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi reconnect start failed: %s", esp_err_to_name(err));
}

static void schedule_reconnect(void)
{
    const TickType_t delay = pdMS_TO_TICKS(s_reconnect_backoff_ms);
    if (xTimerChangePeriod(s_reconnect_timer, delay, 0) != pdPASS ||
        xTimerStart(s_reconnect_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "unable to schedule Wi-Fi reconnect");
    }
    if (s_reconnect_backoff_ms < CONFIG_AIOT_WIFI_MAX_RECONNECT_MS) {
        s_reconnect_backoff_ms *= 2U;
        if (s_reconnect_backoff_ms > CONFIG_AIOT_WIFI_MAX_RECONNECT_MS) {
            s_reconnect_backoff_ms = CONFIG_AIOT_WIFI_MAX_RECONNECT_MS;
        }
    }
}

static void network_event(void *argument, esp_event_base_t base,
                          int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_data;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        schedule_reconnect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        schedule_reconnect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_reconnect_backoff_ms = WIFI_INITIAL_BACKOFF_MS;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected and IP address assigned");
    }
}
#endif

esp_err_t network_manager_provision_wifi(const char *ssid, const char *password)
{
    if (!aiot_wifi_credentials_valid(ssid, password)) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_AIOT_WIFI_ENABLED
    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &handle), TAG,
                        "open Wi-Fi credential namespace");
    esp_err_t err = nvs_set_str(handle, WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, WIFI_PASSWORD_KEY, password);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) ESP_LOGI(TAG, "Wi-Fi credentials stored; restart to apply");
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t network_manager_init(void)
{
#if CONFIG_AIOT_WIFI_ENABLED
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Wi-Fi credentials are not provisioned; network remains offline");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open Wi-Fi credential namespace");
    char ssid[33] = {0};
    char password[65] = {0};
    err = read_string(handle, WIFI_SSID_KEY, ssid, sizeof(ssid));
    if (err == ESP_OK) err = read_string(handle, WIFI_PASSWORD_KEY, password, sizeof(password));
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi credentials are invalid or incomplete");
        return err;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "initialize TCP/IP stack");
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (esp_netif_create_default_wifi_sta() == NULL) return ESP_ERR_NO_MEM;
    s_events = xEventGroupCreate();
    s_reconnect_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(WIFI_INITIAL_BACKOFF_MS),
                                     pdFALSE, NULL, reconnect_timer_callback);
    if (s_events == NULL || s_reconnect_timer == NULL) return ESP_ERR_NO_MEM;

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "initialize Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   network_event, NULL), TAG,
                        "register Wi-Fi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   network_event, NULL), TAG,
                        "register IP event handler");
    wifi_config_t configuration = {0};
    strlcpy((char *)configuration.sta.ssid, ssid, sizeof(configuration.sta.ssid));
    strlcpy((char *)configuration.sta.password, password,
            sizeof(configuration.sta.password));
    configuration.sta.threshold.authmode = strlen(password) == 0U
        ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &configuration), TAG,
                        "configure station");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");
    memset(password, 0, sizeof(password));
    s_initialized = true;
    return ESP_OK;
#else
    ESP_LOGI(TAG, "Wi-Fi disabled by CONFIG_AIOT_WIFI_ENABLED");
    return ESP_OK;
#endif
}

bool network_manager_is_connected(void)
{
#if CONFIG_AIOT_WIFI_ENABLED
    return s_initialized &&
           (xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) != 0U;
#else
    return false;
#endif
}
