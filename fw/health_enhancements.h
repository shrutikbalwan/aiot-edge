#ifndef HEALTH_ENHANCEMENTS_H
#define HEALTH_ENHANCEMENTS_H

#include <stdint.h>
#include <stddef.h>
#include "sensor_drivers.h"

/* ------------------------------------------------------------ */
/*                          Health Metrics Structure            */
/* ------------------------------------------------------------ */
typedef struct {
    uint32_t bpm;                          /* Heart rate in BPM */
    uint8_t spo2;                          /* SpO2 percentage */
    bool spo2_valid;
    float_t hrv_rmssd;                     /* Heart Rate Variability RMSSD (ms) */
    uint32_t last_rr_interval;             /* Last R-R interval (ms) */
    uint32_t rr_intervals[8];              /* Recent R-R intervals (ms) */
    uint8_t rr_count;
    uint8_t quality_flag;                  /* Signal quality: 1=Poor, 2=Fair, 3=Good */
} health_metrics_t;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
void health_enhancements_update_ppg(uint16_t ppg_value);
health_metrics_t health_enhancements_get_metrics(void);
void health_enhancements_reset_metrics(void);

/* ------------------------------------------------------------ */
/*                          Default Constants                   */
/* ------------------------------------------------------------ */
#define SAMPLE_SIZE      256    /* Buffer size for metric calculations */
#define MIN_SPO2_VALID   80     /* Minimum valid SpO2 threshold */
#define MAX_SPO2_VALID   100    /* Maximum valid SpO2 threshold */
#define MIN_BPM          40     /* Minimum valid BPM */
#define MAX_BPM          200    /* Maximum valid BPM */
#define MIN_HRV_RMSSD    5.0f   /* Minimum valid HRV RMSSD */

/* ------------------------------------------------------------ */
/*                          End of Header                         */
/* ------------------------------------------------------------ */
#endif /* HEALTH_ENHANCEMENTS_H */