/**
 * @file sensor_drivers.c
 * @brief Sensor Driver Implementation for AIoT-Edge
 * @brief PPG, Temperature, and Accelerometer drivers using I2C
 */

#include "sensor_drivers.h"
#include "system_config.h"
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/*                          PPG Driver (MAX30102)               */
/* ------------------------------------------------------------ */
esp_err_t sensor_init_ppg(void) {
    /* Initialize I2C master */
    i2c_master_init();

    /* MAX30102 reset */
    uint8_t reset_cmd = 0x40;
    i2c_master_write(SENSOR_I2C_ADDR_PPG, &reset_cmd, 1, 100);

    /* Set up pulse amplitude and LED current */
    uint8_t config_reg = 0x27; /* Mode: SpO2 + HR, LED current: 50mA */
    i2c_master_write(SENSOR_I2C_ADDR_PPG, &config_reg, 1, 100);

    /* Set sample rate to 100 SPS */
    uint8_t sr_reg = 0x25;
    i2c_master_write(SENSOR_I2C_ADDR_PPG, &sr_reg, 1, 100);

    ESP_LOGI("SENSOR", "PPG (MAX30102) initialized");
    return ESP_OK;
}

uint16_t sensor_read_ppg(void) {
    /* Read from PPG data register (0x07 for infrared, 0x06 for red) */
    uint8_t reg_addr = 0x07;
    uint8_t data[2];

    i2c_master_read(SENSOR_I2C_ADDR_PPG, &reg_addr, 1, data, 2, 100);

    /* Combine MSB and LSB (MAX30102 big-endian) */
    uint16_t value = ((uint16_t)data[0] << 8) | data[1];

    return value;
}

esp_err_t sensor_set_ppg_rate(uint16_t rate_hz) {
    uint8_t sr_reg;

    if (rate_hz == 50) {
        sr_reg = 0x25; /* 50 SPS */
    } else if (rate_hz == 100) {
        sr_reg = 0x26; /* 100 SPS */
    } else if (rate_hz == 200) {
        sr_reg = 0x27; /* 200 SPS */
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_write(SENSOR_I2C_ADDR_PPG, &sr_reg, 1, 100);
    return ESP_OK;
}

void sensor_estimate_heart_rate(uint16_t *samples, uint16_t num_samples) {
    /* Peak detection algorithm for heart rate estimation */
    uint16_t max_val = 0;
    uint16_t peak_count = 0;
    uint32_t total_peaks = 0;

    for (int i = 1; i < num_samples - 1; i++) {
        if (samples[i] > samples[i - 1] && samples[i] > samples[i + 1]) {
            peak_count++;
            if (samples[i] > max_val) {
                max_val = samples[i];
            }
            total_peaks += samples[i];
        }
    }

    if (peak_count > 0) {
        /* Approximate BPM: (peaks / num_samples) * 60 * sample_rate */
        /* With 100 SPS: BPM = peaks * 6000 / num_samples */
        float bpm = (float)peak_count * 6000.0f / (float)num_samples;
        ESP_LOGI("HR", "Estimated Heart Rate: %.1f BPM", bpm);
    }
}

bool sensor_detect_irregular_rhythm(uint16_t *samples, uint16_t num_samples) {
    /* Simple CVRR (Cardio-Variable Respiratory Rate) approximation */
    uint16_t intervals[50];
    uint16_t interval_count = 0;

    for (int i = 1; i < num_samples; i++) {
        if (samples[i] > samples[i - 1] && samples[i] > samples[i + 1]) {
            if (interval_count < 50) {
                intervals[interval_count] = i - (interval_count > 0 ? intervals[interval_count - 1] : 0);
                interval_count++;
            }
        }
    }

    if (interval_count < 3) {
        return false;
    }

    /* Calculate mean and standard deviation of intervals */
    uint32_t sum = 0;
    for (int i = 0; i < interval_count; i++) {
        sum += intervals[i];
    }
    float mean = (float)sum / (float)interval_count;

    uint32_t sq_sum = 0;
    for (int i = 0; i < interval_count; i++) {
        sq_sum += (intervals[i] - mean) * (intervals[i] - mean);
    }
    float std_dev = (float)sqrt((double)sq_sum / (float)interval_count);

    /* Coefficient of variation > 15% indicates irregular rhythm */
    float cv = (mean > 0) ? (std_dev / mean) * 100.0f : 0.0f;

    if (cv > 15.0f) {
        ESP_LOGW("ARRHYTH", "Irregular rhythm detected (CV: %.1f%%)", cv);
        return true;
    }

    return false;
}

/* ------------------------------------------------------------ */
/*                          Temperature Driver (MAX30205)       */
/* ------------------------------------------------------------ */
esp_err_t sensor_init_temp(void) {
    /* MAX30205 default I2C address 0x49, shift right by 1 */
    uint8_t config = 0x00; /* Normal mode, 4Hz conversion rate */
    i2c_master_write(SENSOR_I2C_ADDR_TEMP, &config, 1, 100);

    ESP_LOGI("SENSOR", "Temperature (MAX30205) initialized");
    return ESP_OK;
}

int16_t sensor_read_temp(void) {
    uint8_t reg_addr = 0x00; /* Temperature MSB register */
    uint8_t data[2];

    i2c_master_read(SENSOR_I2C_ADDR_TEMP, &reg_addr, 1, data, 2, 100);

    /* Two's complement 16-bit value */
    int16_t msb = (int16_t)data[0];
    int16_t lsb = data[1];
    int16_t temp = (msb << 8) | lsb;

    /* MAX30205: LSB = 0.00390625°C */
    /* Raw value is in °C * 256, so divide by 256 */
    /* Or: temp_c = raw * 0.00390625 */

    return temp; /* Return raw value, caller converts if needed */
}

esp_err_t sensor_set_temp_rate(uint16_t rate_hz) {
    /* MAX30205 has fixed rates: 1, 2, 4, 8, 16 SPS */
    uint8_t config;

    switch (rate_hz) {
        case 1:  config = 0x00; break;
        case 2:  config = 0x01; break;
        case 4:  config = 0x02; break;
        case 8:  config = 0x03; break;
        case 16: config = 0x04; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    i2c_master_write(SENSOR_I2C_ADDR_TEMP, &config, 1, 100);
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Accelerometer Driver (BMI160)      */
/* ------------------------------------------------------------ */
esp_err_t sensor_init_accel(void) {
    /* BMI160 default I2C address 0x53 (shifted) or 0xBB (primary) */
    uint8_t soft_res = 0xB6; /* Soft reset */
    i2c_master_write(SENSOR_I2C_ADDR_ACCEL, &soft_res, 1, 100);

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Write to power control register (0x1C) - normal mode */
    uint8_t power_ctl = 0x00; /* Normal mode, 16g range */
    i2c_master_write(SENSOR_I2C_ADDR_ACCEL, &power_ctl, 1, 100);

    /* Write to data rate register (0x1A) - 1kHz */
    uint8_t data_ctl = 0x0F; /* 1kHz ODR */
    i2c_master_write(SENSOR_I2C_ADDR_ACCEL, &data_ctl, 1, 100);

    ESP_LOGI("SENSOR", "Accelerometer (BMI160) initialized");
    return ESP_OK;
}

void sensor_read_accel(int16_t *x, int16_t *y, int16_t *z) {
    uint8_t reg_addr = 0x2C; /* X-AXIS MSB */
    uint8_t data[6];

    i2c_master_read(SENSOR_I2C_ADDR_ACCEL, &reg_addr, 1, data, 6, 100);

    *x = ((int16_t)data[0] << 8) | data[1];
    *y = ((int16_t)data[2] << 8) | data[3];
    *z = ((int16_t)data[4] << 8) | data[5];
}

esp_err_t sensor_detect_activity(void) {
    /* Activity detection using accelerometer threshold monitoring */
    /* Detects significant acceleration changes indicating activity/fall */
    int16_t x, y, z;
    sensor_read_accel(&x, &y, &z);

    /* Calculate resultant acceleration magnitude */
    int32_t magnitude = (int32_t)x * x + (int32_t)y * y + (int32_t)z * z;
    int32_t magnitude_norm = (int32_t)sqrt((double)magnitude);

    /* Threshold: significant change > 8000 (approx 1g = 256 LSB, calibrated) */
    static int32_t last_magnitude = 0;
    int32_t delta = magnitude_norm - last_magnitude;

    if (abs(delta) > 8000) {
        ESP_LOGI("SENSOR", "Activity detected! delta=%d, magnitude=%d", delta, magnitude_norm);
        last_magnitude = magnitude_norm;
        return ESP_OK;
    }

    last_magnitude = magnitude_norm;
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Calibration & Utilities             */
/* ------------------------------------------------------------ */
esp_err_t sensor_calibrate_temp(void) {
    /* MAX30205 auto-calibration on every reading */
    /* No external calibration required */
    ESP_LOGI("SENSOR", "Temperature calibration: auto (MAX30205)");
    return ESP_OK;
}

esp_err_t sensor_self_test(void) {
    /* Test all sensors by reading back IDs */
    ESP_LOGI("SELF_TEST", "Running sensor self-test...");

    /* PPG ID check (should be 0x15 for MAX30102) */
    uint8_t ppg_id = 0x00;
    i2c_master_read(SENSOR_I2C_ADDR_PPG, (uint8_t[]){0xFE}, 1, &ppg_id, 1, 100);
    ESP_LOGI("SELF_TEST", "PPG Chip ID: 0x%02X", ppg_id);

    /* Temperature ID check (should be 0x20 for MAX30205) */
    uint8_t temp_id = 0x00;
    i2c_master_read(SENSOR_I2C_ADDR_TEMP, (uint8_t[]){0xBF}, 1, &temp_id, 1, 100);
    ESP_LOGI("SELF_TEST", "Temp Chip ID: 0x%02X", temp_id);

    return ESP_OK;
}