/**
 * @file health_enhancements.c
 * @brief Health Monitoring Enhancements for AIoT-Edge
 * @brief SpO2 estimation, Heart Rate Variability, and advanced vital sign analysis
 */

#include "health_enhancements.h"
#include "sensor_drivers.h"
#include "ble_transport.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define HR_TAG "HR_ENHANCE"
#define SPO2_TAG "SPO2_ENH"
#define BUFFER_SIZE 256

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    uint32_t bpm;                          /* Current heart rate */
    uint8_t spo2;                          /* Blood oxygen saturation */
    bool spo2_valid;
    float_t hrv_time_domain;               /* RMSSD-based HRV */
    uint32_t last_rr_interval;             /* Last R-R interval (ms) */
    uint32_t rr_intervals[8];              /* Recent R-R intervals */
    uint8_t rr_count;
    uint8_t quality_flag;                  /* Signal quality indicator */
} health_metrics_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static health_metrics_t g_health_metrics = {0};
static uint32_t g_hr_sample_index = 0;
static uint16_t g_ir_buffer[SAMPLE_RATE];  /* IR LED sensor values */
static uint16_t g_red_buffer[SAMPLE_RATE]; /* Red LED sensor values (for SpO2) */

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static uint32_t hr_calculate_bpm(uint16_t *buffer, uint32_t count);
static uint8_t spo2_estimate(uint16_t *ir, uint16_t *red, uint32_t count);
static float_t hrv_calculate_rmssd(uint32_t *rr_intervals, uint8_t count);
static uint8_t quality_assess(uint16_t *ir, uint16_t *red, uint32_t count);

/* ------------------------------------------------------------ */
/*                          Health Metrics Update               */
/* ------------------------------------------------------------ */
void health_enhancements_update_ppg(uint16_t ppg_value) {
    /* Update rolling buffers for SpO2 calculation */
    if (g_hr_sample_index < SAMPLE_SIZE) {
        g_red_buffer[g_hr_sample_index] = ppg_value;
        g_ir_buffer[g_hr_sample_index] = ppg_value; /* Simplified: assuming PPG is IR */
        g_hr_sample_index++;
    } else {
        /* Shift buffers and add new sample */
        memmove(g_red_buffer, g_red_buffer + 1, 
                (SAMPLE_SIZE - 1) * sizeof(uint16_t));
        memmove(g_ir_buffer, g_ir_buffer + 1, 
                (SAMPLE_SIZE - 1) * sizeof(uint16_t));
        g_red_buffer[SAMPLE_SIZE - 1] = ppg_value;
        g_ir_buffer[SAMPLE_SIZE - 1] = ppg_value;
    }

    /* Calculate metrics when buffer is full */
    if (g_hr_sample_index >= SAMPLE_SIZE || g_hr_sample_index >= 2 * SAMPLE_SIZE) {
        /* Update SpO2 estimation */
        g_health_metrics.spo2 = spo2_estimate(g_red_buffer, g_ir_buffer, SAMPLE_SIZE);
        g_health_metrics.spo2_valid = (g_health_metrics.spo2 > 80 && g_health_metrics.spo2 <= 100);

        /* Calculate BPM */
        g_health_metrics.bpm = hr_calculate_bpm(g_ir_buffer, SAMPLE_SIZE);

        /* Calculate HRV (RMSSD) */
        g_health_metrics.hrv_time_domain = hrv_calculate_rmssd(
            g_health_metrics.rr_intervals, g_health_metrics.rr_count);

        /* Assess signal quality */
        g_health_metrics.quality_flag = quality_assess(g_red_buffer, g_ir_buffer, SAMPLE_SIZE);

        /* Reset counter */
        g_hr_sample_index = 0;
    }
}

/* ------------------------------------------------------------ */
/*                          Heart Rate Calculation              */
/* ------------------------------------------------------------ */
static uint32_t hr_calculate_bpm(uint16_t *buffer, uint32_t count) {
    /* Peak detection algorithm for heart rate calculation */
    if (count < 10) {
        return 0;
    }

    /* Find peaks above threshold */
    uint32_t peak_count = 0;
    uint32_t last_peak_time = 0;
    int32_t max_val = 0;
    int32_t min_val = 0xFFFF;

    for (uint32_t i = 1; i < count - 1; i++) {
        if (buffer[i] > buffer[i-1] && buffer[i] > buffer[i+1]) {
            peak_count++;
            /* Calculate interval from last peak */
            if (last_peak_time > 0) {
                /* Store R-R interval */
                if (g_health_metrics.rr_count < 8) {
                    g_health_metrics.rr_intervals[g_health_metrics.rr_count] = 
                        i - last_peak_time;
                    g_health_metrics.rr_count++;
                }
            }
            last_peak_time = i;
        }
    }

    /* Calculate BPM from average R-R interval */
    if (peak_count < 2 || g_health_metrics.rr_count < 2) {
        return 0;
    }

    /* Average R-R interval in samples */
    uint32_t avg_rr = 0;
    for (uint8_t i = 0; i < g_health_metrics.rr_count; i++) {
        avg_rr += g_health_metrics.rr_intervals[i];
    }
    avg_rr /= g_health_metrics.rr_count;

    /* Convert to BPM: (60 seconds / avg_rr_samples) * SAMPLE_RATE */
    if (avg_rr == 0) return 0;
    uint32_t bpm = (60 * SAMPLE_RATE) / avg_rr;

    /* Cap realistic BPM range */
    if (bpm < 40 || bpm > 200) return 0;

    g_health_metrics.bpm = bpm;
    return bpm;
}

/* ------------------------------------------------------------ */
/*                          SpO2 Estimation                     */
/* ------------------------------------------------------------ */
static uint8_t spo2_estimate(uint16_t *red, uint16_t *ir, uint32_t count) {
    /* Simplified SpO2 calculation using ratio of ratios method */
    /* In a full implementation, would use MAX30205/PPG algorithm */

    if (count < 20) return 95; /* Default fallback */

    /* Calculate AC and DC components */
    uint32_t red_dc = 0, ir_dc = 0;
    uint32_t red_ac = 0, ir_ac = 0;

    /* Mean values */
    uint32_t red_mean = 0, ir_mean = 0;
    for (uint32_t i = 0; i < count; i++) {
        red_mean += red[i];
        ir_mean += ir[i];
    }
    red_mean /= count;
    ir_mean /= count;

    /* AC component (peak-to-peak variation) */
    for (uint32_t i = 0; i < count; i++) {
        red_ac += abs(red[i] - red_mean);
        ir_ac += abs(ir[i] - ir_mean);
    }
    red_ac /= count;
    ir_ac /= count;

    /* Avoid division by zero */
    if (ir_ac == 0) return 95;

    /* Ratio of ratios */
    float_t ratio = ((float_t)red_ac / red_mean) / ((float_t)ir_ac / ir_mean);

    /* Empirical SpO2 calculation (for demonstration) */
    /* SpO2 = 110 - 25 * ratio (approximate for finger-type sensors) */
    int32_t spo2_val = (int32_t)(110 - 25 * ratio);

    /* Clamp to valid range */
    if (spo2_val < 80) spo2_val = 80;
    if (spo2_val > 100) spo2_val = 100;

    return (uint8_t)spo2_val;
}

/* ------------------------------------------------------------ */
/*                          HRV Calculation (RMSSD)             */
/* ------------------------------------------------------------ */
static float_t hrv_calculate_rmssd(uint32_t *rr_intervals, uint8_t count) {
    if (count < 2) return 0;

    float_t sum_sq_diff = 0;
    for (uint8_t i = 1; i < count; i++) {
        int32_t diff = (int32_t)rr_intervals[i] - (int32_t)rr_intervals[i-1];
        sum_sq_diff += (float_t)diff * (float_t)diff;
    }

    return sqrtf(sum_sq_diff / (float_t)(count - 1));
}

/* ------------------------------------------------------------ */
/*                          Signal Quality Assessment           */
/* ------------------------------------------------------------ */
static uint8_t quality_assess(uint16_t *ir, uint16_t *red, uint32_t count) {
    if (count < 10) return 0;

    /* Calculate signal variability */
    uint32_t ir_var = 0, red_var = 0;
    uint32_t ir_mean = 0, red_mean = 0;

    for (uint32_t i = 0; i < count; i++) {
        ir_mean += ir[i];
        red_mean += red[i];
    }
    ir_mean /= count;
    red_mean /= count;

    for (uint32_t i = 0; i < count; i++) {
        ir_var += (ir[i] - ir_mean) * (ir[i] - ir_mean);
        red_var += (red[i] - red_mean) * (red[i] - red_mean);
    }
    ir_var /= count;
    red_var /= count;

    /* Quality threshold: sufficient signal variance */
    if (ir_var > 1000 && red_var > 1000) return 3; /* Good */
    if (ir_var > 500 || red_var > 500) return 2; /* Fair */
    return 1; /* Poor */
}

/* ------------------------------------------------------------ */
/*                          Get Health Metrics                  */
/* ------------------------------------------------------------ */
health_metrics_t health_enhancements_get_metrics(void) {
    /* Copy current metrics and reset quality */
    health_metrics_t metrics;
    memcpy(&metrics, &g_health_metrics, sizeof(health_metrics_t));
    metrics.quality_flag = 0; /* Quality is snapshot-specific */
    return metrics;
}

/* ------------------------------------------------------------ */
/*                          Reset Metrics                       */
/* ------------------------------------------------------------ */
void health_enhancements_reset_metrics(void) {
    memset(&g_health_metrics, 0, sizeof(health_metrics_t));
    g_hr_sample_index = 0;
}