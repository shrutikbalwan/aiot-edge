/**
 * @file power_mgmt.c
 * @brief Power Management Module for AIoT-Edge
 * @brief Dynamic voltage/frequency scaling, sleep modes, and power optimization
 */

#include "power_mgmt.h"
#include "system_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define PM_TAG "POWER_MGMT"
#define DEFAULT_MAX_FREQ       MHZ(48)
#define DEFAULT_MIN_FREQ       MHZ(24)

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    esp_pm_config_t pm_config;
    bool light_sleep_enabled;
    bool deep_sleep_enabled;
    uint32_t last_wake_time;
    uint32_t sleep_count;
    uint32_t active_count;
    pm_wakeup_source_t wake_source;
} power_mgmt_handle_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static power_mgmt_handle_t g_pm_handle = {0};

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static esp_err_t pm_configure_light_sleep(void);
static esp_err_t pm_configure_deep_sleep(void);
static void pm_gpio_wakeup_init(void);

/* ------------------------------------------------------------ */
/*                          Power Management Initialization     */
/* ------------------------------------------------------------ */
esp_err_t power_mgmt_init(void) {
    memset(&g_pm_handle, 0, sizeof(g_pm_handle));

    /* Initialize power management */
    g_pm_handle.pm_config.max_freq_mhz = DEFAULT_MAX_FREQ;
    g_pm_handle.pm_config.min_freq_mhz = DEFAULT_MIN_FREQ;
    g_pm_handle.pm_config.light_sleep_enable = true;

    esp_err_t ret = esp_pm_configure(&g_pm_handle.pm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(PM_TAG, "Power management configure failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize GPIO wakeup sources */
    pm_gpio_wakeup_init();

    g_pm_handle.light_sleep_enabled = true;
    g_pm_handle.sleep_count = 0;
    g_pm_handle.active_count = 0;

    ESP_LOGI(PM_TAG, "Power management initialized");
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Enter Light Sleep                 */
/* ------------------------------------------------------------ */
esp_err_t power_mgmt_enter_light_sleep(void) {
    if (!g_pm_handle.light_sleep_enabled) {
        ESP_LOGW(PM_TAG, "Light sleep disabled");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_light_sleep_start();
    if (ret == ESP_OK) {
        g_pm_handle.sleep_count++;
        g_pm_handle.active_count = 0;
        ESP_LOGI(PM_TAG, "Entered light sleep (wake count: %d)", g_pm_handle.sleep_count);
    }
    return ret;
}

/* ------------------------------------------------------------ */
/*                          Enter Deep Sleep                  */
/* ------------------------------------------------------------ */
esp_err_t power_mgmt_enter_deep_sleep(void) {
    if (!g_pm_handle.deep_sleep_enabled) {
        ESP_LOGW(PM_TAG, "Deep sleep disabled");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_deep_sleep_start();
    if (ret == ESP_OK) {
        g_pm_handle.sleep_count++;
        g_pm_handle.active_count = 0;
        ESP_LOGI(PM_TAG, "Entered deep sleep (wake count: %d)", g_pm_handle.sleep_count);
    }
    return ret;
}

/* ------------------------------------------------------------ */
/*                          Wakeup Source Configuration         */
/* ------------------------------------------------------------ */
static void pm_gpio_wakeup_init(void) {
    /* Initialize GPIO wakeup pins for light/deep sleep */
    /* Timer wakeup can be configured separately */
    esp_sleep_enable_timer_wakeup(1000000); /* 1 second */
    ESP_LOGD(PM_TAG, "GPIO wakeup sources initialized");
}

/* ------------------------------------------------------------ */
/*                          Set Wakeup Sources                */
/* ------------------------------------------------------------ */
esp_err_t power_mgmt_set_wakeup_source(pm_wakeup_source_t source) {
    g_pm_handle.wake_source = source;

    switch (source) {
        case PM_WAKEUP_TIMER:
            esp_sleep_enable_timer_wakeup(1000000);
            break;
        case PM_WAKEUP_GPIO:
            /* Configure specific GPIO for wakeup */
            esp_sleep_enable_gpio_wakeup();
            break;
        case PM_WAKEUP_UART:
            esp_sleep_enable_uart_wakeup(UART_NUM_0);
            break;
        default:
            ESP_LOGW(PM_TAG, "Unsupported wakeup source: %d", source);
            return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Get Power Statistics              */
/* ------------------------------------------------------------ */
void power_mgmt_get_stats(uint32_t *sleep_cycles, uint32_t *active_cycles) {
    if (sleep_cycles) {
        *sleep_cycles = g_pm_handle.sleep_count;
    }
    if (active_cycles) {
        *active_cycles = g_pm_handle.active_count;
    }
}

/* ------------------------------------------------------------ */
/*                          Public API Functions                */
/* ------------------------------------------------------------ */
/**
 * @brief Enable dynamic frequency scaling
 * @param enable true to enable, false to disable
 * @return esp_err_t ESP_OK on success
 */
esp_err_t power_mgmt_enable_dfs(bool enable) {
    if (enable) {
        ESP_LOGI(PM_TAG, "Dynamic frequency scaling enabled");
        /* DFS is handled by ESP-PM auto-tuning */
        return ESP_OK;
    }
    ESP_LOGI(PM_TAG, "Dynamic frequency scaling disabled");
    return esp_pm_disable();
}

/**
 * @brief Get current power mode status
 * @return power_mode_t Current power mode
 */
power_mode_t power_mgmt_get_mode(void) {
    power_mode_t mode = {0};
    mode.light_sleep = g_pm_handle.light_sleep_enabled;
    mode.deep_sleep = g_pm_handle.deep_sleep_enabled;
    mode.dfs_enabled = /* dfs status */ false;
    return mode;
}