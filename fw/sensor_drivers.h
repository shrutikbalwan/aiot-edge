/**
 * @file sensor_drivers.h
 * @brief Sensor Driver Header for AIoT-Edge Health Monitoring
 * @brief PPG, Temperature, and Environmental Sensors
 */

#ifndef SENSOR_DRIVERS_H
#define SENSOR_DRIVERS_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------ */
/*                                  Macro Definitions           */
/* ------------------------------------------------------------ */
#define SENSOR_I2C_ADDR_PPG    (0x57 >> 1)   /* Maxim MAX30102 */
#define SENSOR_I2C_ADDR_TEMP   (0x48 >> 1)   /* Maxim MAX30205 */
#define SENSOR_I2C_ADDR_ACCEL  (0x53 >> 1)   /* BMI160 */

/* Sensor sample rates */
#define SENSOR_RATE_PPG        100    /* 100 Hz */
#define SENSOR_RATE_TEMP       10     /* 10 Hz */
#define SENSOR_RATE_ACCEL      1000   /* 1 kHz */

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */

/* PPG (Heart Rate & SpO2) */
esp_err_t sensor_init_ppg(void);
uint16_t sensor_read_ppg(void);
esp_err_t sensor_set_ppg_rate(uint16_t rate_hz);
void sensor_estimate_heart_rate(uint16_t *samples, uint16_t num_samples);
bool sensor_detect_irregular_rhythm(uint16_t *samples, uint16_t num_samples);

/* Temperature */
esp_err_t sensor_init_temp(void);
int16_t sensor_read_temp(void);
esp_err_t sensor_set_temp_rate(uint16_t rate_hz);

/* Accelerometer (activity detection, fall detection) */
esp_err_t sensor_init_accel(void);
void sensor_read_accel(int16_t *x, int16_t *y, int16_t *z);
esp_err_t sensor_detect_activity(void);

/* ------------------------------------------------------------ */
/*                          Calibration & Utilities           */
/* ------------------------------------------------------------ */
esp_err_t sensor_calibrate_temp(void);
esp_err_t sensor_self_test(void);

#endif /* SENSOR_DRIVERS_H */