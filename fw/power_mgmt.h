#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <stdint.h>
#include <stddef.h>
#include "system_config.h"

/* ------------------------------------------------------------ */
/*                          Power Mode Types                    */
/* ------------------------------------------------------------ */
typedef struct {
    bool light_sleep_enabled;
    bool deep_sleep_enabled;
    bool dfs_enabled;
    uint32_t max_freq_mhz;
    uint32_t min_freq_mhz;
} power_mode_t;

/* ------------------------------------------------------------ */
/*                          Wakeup Source Types                 */
/* ------------------------------------------------------------ */
typedef enum {
    PM_WAKEUP_TIMER,
    PM_WAKEUP_GPIO,
    PM_WAKEUP_UART,
    PM_WAKEUP_EXT,
} pm_wakeup_source_t;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
esp_err_t power_mgmt_init(void);
esp_err_t power_mgmt_enter_light_sleep(void);
esp_err_t power_mgmt_enter_deep_sleep(void);
esp_err_t power_mgmt_set_wakeup_source(pm_wakeup_source_t source);
void power_mgmt_get_stats(uint32_t *sleep_cycles, uint32_t *active_cycles);
esp_err_t power_mgmt_enable_dfs(bool enable);
power_mode_t power_mgmt_get_mode(void);

/* ------------------------------------------------------------ */
/*                          Power Management Config             */
/* ------------------------------------------------------------ */
#define DEFAULT_MAX_FREQ_MHZ      48
#define DEFAULT_MIN_FREQ_MHZ      24
#define LIGHT_SLEEP_ENABLED       true
#define DEEP_SLEEP_ENABLED        false

/* ------------------------------------------------------------ */
/*                          Power Settings                      */
/* ------------------------------------------------------------ */
#define PM_TIMER_WAKEUP_US        1000000 /* 1 second default */
#define PM_GPIO_WAKEUP_ENABLE     GPIO_WAKEUP_ENABLE

/* ------------------------------------------------------------ */
/*                          End of Header                         */
/* ------------------------------------------------------------ */
#endif /* POWER_MGMT_H */