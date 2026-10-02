/**
 * @file main_hardware_simulation.c
 * @brief AIoT-Edge Hardware Simulation Main Entry
 * @brief Simulated firmware main for development without physical hardware
 * 
 * This file provides a complete simulated firmware main that initializes
 * and exercises all hardware components using simulated I2C and hardware
 * abstraction layers. Useful for development, unit testing, and demonstration
 * without requiring actual ESP32-S3 or sensor hardware.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sensor_drivers.h"
#include "ble_transport.h"
#include "ai_accelerator.h"
#include "system_config.h"

/* ------------------------------------------------------------ */
/*                          Simulation Configuration          */
/* ------------------------------------------------------------ */

/* Simulation tick rate (FreeRTOS tick per second) */
#define SIMULATION_TICK_RATE_HZ 1000

/* Sensor reading simulation states */
typedef enum {
    SENSOR_STATE_OFF,
    SENSOR_STATE_INIT,
    SENSOR_STATE_READY,
    SENSOR_STATE_ERROR
} sensor_sim_state_t;

/* PPG simulation data */
static uint16_t sim_ppg_data = 512; /* Baseline PPG value */
static uint32_t sim_hr_bpm = 72;   /* Simulated heart rate */
static uint8_t sim_spo2 = 98;      /* Simulated SpO2 */

/* Temperature simulation data */
static float sim_temp_c = 36.8;    /* Baseline temperature */

/* Accelerometer simulation data */
static int16_t sim_accel_x = 0;
static int16_t sim_accel_y = 0;
static int16_t sim_accel_z = 0;

/* Wake word simulation */
static uint8_t sim_wake_word_detected = 0;
static uint32_t sim_last_wake_time = 0;

/* ------------------------------------------------------------ */
/*                          Simulated I2C                      */
/* ------------------------------------------------------------ */

/* Simulated I2C master functions */
static int i2c_master_init_count = 0;

void i2c_master_init(void) {
    i2c_master_init_count++;
    printf("[SIM I2C] I2C Master initialized (call count: %d)\n", i2c_master_init_count);
}

esp_err_t i2c_master_write(uint8_t addr, uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    /* Simulate I2C write - always succeed in simulation */
    (void)timeout_ms;
    if (addr == SENSOR_I2C_ADDR_PPG) {
        /* Simulate PPG register write */
        if (len > 0) {
            printf("[SIM I2C] PPG write: reg=0x%02X, val=0x%02X\n", data[0], len > 1 ? data[1] : 0);
        }
    } else if (addr == SENSOR_I2C_ADDR_TEMP) {
        /* Simulate Temperature register write */
        printf("[SIM I2C] Temp write: reg=0x%02X, val=0x%02X\n", data[0], len > 1 ? data[1] : 0);
    } else if (addr == SENSOR_I2C_ADDR_ACCEL) {
        /* Simulate Accelerometer register write */
        printf("[SIM I2C] Accel write: reg=0x%02X, val=0x%02X\n", data[0], len > 1 ? data[1] : 0);
    }
    return ESP_OK;
}

esp_err_t i2c_master_read(uint8_t addr, uint8_t *reg_addr, uint8_t reg_len, uint8_t *data, uint16_t data_len, uint32_t timeout_ms) {
    (void)timeout_ms;
    
    /* Simulate I2C read with realistic sensor data */
    if (addr == SENSOR_I2C_ADDR_PPG) {
        /* Simulate PPG read - return varying values around baseline */
        sim_ppg_data = sim_ppg_data + (rand() % 20 - 10); /* Small variation */
        if (sim_ppg_data < 200) sim_ppg_data = 200;
        if (sim_ppg_data > 800) sim_ppg_data = 800;
        data[0] = (sim_ppg_data >> 8) & 0xFF;
        data[1] = sim_ppg_data & 0xFF;
        printf("[SIM I2C] PPG read: value=%d\n", sim_ppg_data);
    } else if (addr == SENSOR_I2C_ADDR_TEMP) {
        /* Simulate Temperature read - slowly varying */
        sim_temp_c += (rand() % 3 - 1) * 0.05; /* Small drift */
        if (sim_temp_c < 35.0f) sim_temp_c = 35.0f;
        if (sim_temp_c > 40.0f) sim_temp_c = 40.0f;
        int16_t temp_raw = (int16_t)(sim_temp_c * 256.0f); /* MAX30205 format */
        data[0] = (temp_raw >> 8) & 0xFF;
        data[1] = temp_raw & 0xFF;
        printf("[SIM I2C] Temp read: raw=%d (%.2f°C)\n", temp_raw, sim_temp_c);
    } else if (addr == SENSOR_I2C_ADDR_ACCEL) {
        /* Simulate Accelerometer read - slight gravity + noise */
        sim_accel_x += (rand() % 3 - 1);
        sim_accel_y += (rand() % 3 - 1);
        sim_accel_z = 256 + (rand() % 20 - 10); /* ~1g + noise */
        data[0] = sim_accel_x & 0xFF;
        data[1] = (sim_accel_x >> 8) & 0xFF;
        data[2] = sim_accel_y & 0xFF;
        data[3] = (sim_accel_y >> 8) & 0xFF;
        data[4] = sim_accel_z & 0xFF;
        data[5] = (sim_accel_z >> 8) & 0xFF;
        printf("[SIM I2C] Accel read: x=%d, y=%d, z=%d\n", sim_accel_x, sim_accel_y, sim_accel_z);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Task Functions                      */
/* ------------------------------------------------------------ */

/* PPG Processing Task */
static void vSimulationPPGTask(void *pvParameters) {
    uint16_t samples[128];
    uint32_t sample_count = 0;
    
    printf("[TASK] PPG Simulation Task started\n");
    
    for (;;) {
        /* Read simulated PPG sensor */
        uint16_t ppg_value = sensor_read_ppg();
        samples[sample_count % 128] = ppg_value;
        sample_count++;
        
        /* Estimate heart rate every 10 samples */
        if (sample_count % 10 == 0) {
            sensor_estimate_heart_rate(samples, 10);
        }
        
        /* Detect irregular rhythm every 50 samples */
        if (sample_count % 50 == 0) {
            bool irregular = sensor_detect_irrhythm(samples, 50);
            if (irregular) {
                printf("[ALERT] Irregular rhythm detected!\n");
                ble_transport_send_alert(ALERT_ARRHYTHMIA);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); /* 100 Hz update rate */
    }
}

/* Temperature Task */
static void vSimulationTempTask(void *pvParameters) {
    printf("[TASK] Temperature Simulation Task started\n");
    
    for (;;) {
        /* Read simulated temperature */
        int16_t temp_raw = sensor_read_temp();
        float temp_c = temp_raw / 256.0f; /* Convert to °C */
        
        printf("[SENSOR] Temperature: %.2f°C (raw: %d)\n", temp_c, temp_raw);
        
        vTaskDelay(pdMS_TO_TICKS(100)); /* 10 Hz update rate */
    }
}

/* Accelerometer Task */
static void vSimulationAccelTask(void *pvParameters) {
    printf("[TASK] Accelerometer Simulation Task started\n");
    
    for (;;) {
        /* Read simulated accelerometer */
        int16_t x, y, z;
        sensor_read_accel(&x, &y, &z);
        
        /* Detect activity */
        sensor_detect_activity();
        
        /* Print acceleration every second */
        static uint32_t last_print = 0;
        /* Note: would use xTaskGetTickCount() in real FreeRTOS */
        printf("[SENSOR] Accel: x=%d, y=%d, z=%d (g-force: %.2f)\n", 
               x, y, z, sqrt((int32_t)x*x + (int32_t)y*y + (int32_t)z*z) / 256.0f);
        
        vTaskDelay(pdMS_TO_TICKS(10)); /* 100 Hz update rate */
    }
}

/* Wake Word / NN Inference Task */
static void vSimulationNNTask(void *pvParameters) {
    printf("[TASK] Neural Network Simulation Task started\n");
    
    for (;;) {
        /* Simulate wake word detection - periodic pattern */
        uint32_t current_ticks = sim_last_wake_time + 200; /* 200ms debounce */
        
        /* Simple pattern: detect "wake word" every 5 seconds of simulation */
        if (sim_last_wake_time == 0 || (current_ticks - sim_last_wake_time) > 5000) {
            sim_wake_word_detected = 1;
            sim_last_wake_time = current_ticks;
            printf("[NN] Wake word detected!\n");
            /* Give notification to BLE task */
            /* In real: xTaskNotifyGive(xTaskGetHandle("BLEConn")); */
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); /* 10 Hz inference rate */
    }
}

/* ------------------------------------------------------------ */
/*                          System Initialization               */
/* ------------------------------------------------------------ */

void app_simulation_start(void) {
    printf("\n");
    printf("========================================\n");
    printf("  AIoT-Edge: Hardware Simulation Mode\n");
    printf("========================================\n");
    printf("\n");
    
    /* Initialize system */
    system_init();
    
    /* Initialize sensors */
    printf("\n[INIT] Initializing sensors...\n");
    sensor_init_ppg();
    sensor_init_temp();
    sensor_init_accel();
    
    /* Print sensor IDs (simulated) */
    printf("[SIM] Chip IDs: PPG=0x15, Temp=0x20, Accel=0x53\n");
    
    /* Start FreeRTOS tasks */
    printf("\n[FREERTOS] Starting tasks...\n");
    
    xTaskCreate(vSimulationPPGTask, "PPGTask", 512, NULL, 2, NULL);
    xTaskCreate(vSimulationTempTask, "TempTask", 256, NULL, 1, NULL);
    xTaskCreate(vSimulationAccelTask, "AccelTask", 512, NULL, 2, NULL);
    xTaskCreate(vSimulationNNTask, "NNTask", 1024, NULL, 2, NULL);
    
    /* Create synchronization primitives */
    printf("[FREERTOS] Creating synchronization primitives...\n");
    
    /* Start software timers would go here */
    
    printf("\n[STATUS] Simulation running - press Ctrl+C to exit\n\n");
}

/* ------------------------------------------------------------ */
/*                          Main Entry                          */
/* ------------------------------------------------------------ */

int main(void) {
    /* Enable FPU if available (simulated) */
    #ifdef FPU_PRESENT
    printf("[CPU] FPU enabled (simulation)\n");
    #endif
    
    /* Set system clock (simulated) */
    printf("[CPU] System clock: 48 MHz (simulation)\n");
    
    /* Initialize hardware abstraction */
    printf("[HW] Initializing hardware abstractions...\n");
    
    /* Begin simulation */
    app_simulation_start();
    
    /* FreeRTOS scheduler would start here in real implementation */
    /* In simulation, just run tasks */
    vTaskStartScheduler();
    
    /* Should never reach here */
    for (;;);
    
    return 0;
}