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
/*                          BLE Communication Simulation        */
/* ------------------------------------------------------------ */
#define BLE_DEVICE_NAME "AIoT-Edge-HM"
#define BLE_ADVERTISING_INTERVAL  160  /* 100ms units ~ 160ms */
#define BLE_CONNECTION_INTERVAL   32   /* 20ms units ~ 20ms */
#define BLE_SUPERVISION_TIMEOUT 1000   /* 10s units ~ 10s */
#define MAX_NOTIFICATION_PAYLOAD 20

/* BLE connection state */
static uint8_t g_sim_ble_connected = 0;
static uint16_t g_sim_conn_handle = 0;
static uint8_t g_sim_ble_notify_index = 0;
static uint8_t g_sim_tx_power = 0;

/* Callback functions */
static ble_notify_cbfn_t g_sim_ble_data_cbfn = NULL;
static ble_connect_cbfn_t g_sim_ble_connect_cbfn = NULL;
static ble_disconnect_cbfn_t g_sim_ble_disconnect_cbfn = NULL;

/* Simulated BLE data to transmit */
static uint8_t g_sim_ble_data[20];
static uint16_t g_sim_ble_data_len = 0;

/* ------------------------------------------------------------ */
/*                          BLE Task                            */
/* ------------------------------------------------------------ */
static void vSimBleTask(void *pvParameters) {
    for (;;) {
        /* Simulate BLE stack - process events */
        /* Check for connection events */
        if (g_sim_ble_connected) {
            /* Simulate periodic connection maintenance */
            /* In real: BLE stack handles this */
        }
        
        /* Simulate advertising if not connected */
        if (!g_sim_ble_connected) {
            /* Simulate advertising every 100ms */
            static uint32_t last_adv = 0;
            /* Would use xTaskGetTickCount() in real FreeRTOS */
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------ */
/*                          BLE Initialization                   */
/* ------------------------------------------------------------ */
void ble_simulation_init(void) {
    printf("[SIM BLE] BLE Simulation initialized\n");
    g_sim_ble_connected = 0;
    g_sim_conn_handle = 0;
    g_sim_ble_notify_index = 0;
    
    /* Register simulated callbacks */
    g_sim_ble_data_cbfn = NULL;
    g_sim_ble_connect_cbfn = NULL;
    g_sim_ble_disconnect_cbfn = NULL;
}

/* ------------------------------------------------------------ */
/*                          BLE API Functions                    */
/* ------------------------------------------------------------ */
esp_err_t ble_simulation_advertise(void) {
    printf("[SIM BLE] Advertising as %s\n", BLE_DEVICE_NAME);
    return ESP_OK;
}

esp_err_t ble_simulation_connect(uint16_t conn_handle) {
    g_sim_ble_connected = 1;
    g_sim_conn_handle = conn_handle;
    printf("[SIM BLE] Connected! Handle: %d\n", conn_handle);
    
    /* Simulate connection callback */
    if (g_sim_ble_connect_cbfn) {
        g_sim_ble_connect_cbfn(ESP_OK, conn_handle);
    }
    
    return ESP_OK;
}

esp_err_t ble_simulation_disconnect(uint16_t conn_handle, uint8_t reason) {
    g_sim_ble_connected = 0;
    g_sim_conn_handle = 0;
    printf("[SIM BLE] Disconnected! Reason: %d\n", reason);
    
    /* Simulate connection callback */
    if (g_sim_ble_disconnect_cbfn) {
        g_sim_ble_disconnect_cbfn(reason);
    }
    
    return ESP_OK;
}

esp_err_t ble_simulation_send_data(uint8_t *data, uint16_t len) {
    if (!g_sim_ble_connected) {
        printf("[SIM BLE] Not connected, cannot send data\n");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Simulate data transmission */
    g_sim_ble_data_len = len > 20 ? 20 : len;
    memcpy(g_sim_ble_data, data, g_sim_ble_data_len);
    printf("[SIM BLE] Sent %d bytes: ", g_sim_ble_data_len);
    for (int i = 0; i < g_sim_ble_data_len; i++) {
        printf("0x%02x ", g_sim_ble_data[i]);
    }
    printf("\n");
    
    /* Simulate callback */
    if (g_sim_ble_data_cbfn) {
        g_sim_ble_data_cbfn(g_sim_ble_data, g_sim_ble_data_len);
    }
    
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          MQTT Simulation                      */
/* ------------------------------------------------------------ */
#define MQTT_BROKER "mqtt.googleapis.com"
#define MQTT_PORT 8883
#define MQTT_CLIENT_ID "aiot-edge-sim-"

static uint8_t g_sim_mqtt_connected = 0;
static char g_sim_client_id[64];

/* MQTT topic callbacks */
typedef void (*mqtt_topic_cb_t)(const char *topic, const char *payload);

static mqtt_topic_cb_t g_sim_mqtt_status_cb = NULL;
static mqtt_topic_cb_t g_sim_mqtt_wake_word_cb = NULL;
static mqtt_topic_cb_t g_sim_mqtt_ota_cb = NULL;

/* ------------------------------------------------------------ */
/*                          MQTT Simulation                      */
/* ------------------------------------------------------------ */
void mqtt_simulation_init(const char *client_id) {
    strncpy(g_sim_client_id, client_id, sizeof(g_sim_client_id) - 1);
    g_sim_mqtt_connected = 0;
    printf("[SIM MQTT] MQTT Simulation initialized: %s\n", g_sim_client_id);
}

esp_err_t mqtt_simulation_connect(void) {
    g_sim_mqtt_connected = 1;
    printf("[SIM MQTT] Connected to %s:%d\n", MQTT_BROKER, MQTT_PORT);
    return ESP_OK;
}

esp_err_t mqtt_simulation_disconnect(void) {
    g_sim_mqtt_connected = 0;
    printf("[SIM MQTT] Disconnected\n");
    return ESP_OK;
}

esp_err_t mqtt_simulation_publish(const char *topic, const char *payload) {
    if (!g_sim_mqtt_connected) {
        printf("[SIM MQTT] Not connected, cannot publish\n");
        return ESP_ERR_INVALID_STATE;
    }
    
    printf("[SIM MQTT] Published to %s: %s\n", topic, payload);
    
    /* Simulate receiving a response */
    /* Could trigger topic callbacks */
    return ESP_OK;
}

esp_err_t mqtt_simulation_subscribe(const char *topic, mqtt_topic_cb_t cb) {
    if (strcmp(topic, "aiot/edge/status") == 0) {
        g_sim_mqtt_status_cb = cb;
    } else if (strcmp(topic, "aiot/edge/wake_word") == 0) {
        g_sim_mqtt_wake_word_cb = cb;
    } else if (strcmp(topic, "aiot/edge/ota") == 0) {
        g_sim_mqtt_ota_cb = cb;
    }
    printf("[SIM MQTT] Subscribed to %s\n", topic);
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Power Modeling                      */
/* ------------------------------------------------------------ */
typedef enum {
    POWER_STATE_ACTIVE,
    POWER_STATE_SLEEP,
    POWER_STATE_DEEP_SLEEP,
    POWER_STATE_HIBERNATE
} power_sim_state_t;

static power_sim_state_t g_sim_power_state = POWER_STATE_ACTIVE;
static uint32_t g_sim_active_time_ms = 0;
static uint32_t g_sim_sleep_time_ms = 0;
static uint32_t g_sim_total_uah = 0; /* microamps-hours */

void power_simulation_init(void) {
    g_sim_power_state = POWER_STATE_ACTIVE;
    g_sim_active_time_ms = 0;
    g_sim_sleep_time_ms = 0;
    g_sim_total_uah = 0;
    printf("[SIM POWER] Power simulation initialized\n");
}

void power_simulation_set_state(power_sim_state_t state) {
    g_sim_power_state = state;
    printf("[SIM POWER] State changed to: %s\n", 
           state == POWER_STATE_ACTIVE ? "ACTIVE" :
           state == POWER_STATE_SLEEP ? "SLEEP" :
           state == POWER_STATE_DEEP_SLEEP ? "DEEP_SLEEP" : "HIBERNATE");
}

void power_simulation_tick_ms(uint32_t ms) {
    /* Update power consumption tracking */
    if (g_sim_power_state == POWER_STATE_ACTIVE) {
        g_sim_active_time_ms += ms;
        /* Approximate: ~5mW active, ~10uA at 3.3V */
        g_sim_total_uah += (ms * 5 / (3.3 * 3600000)) * 1000000; /* rough */
    } else if (g_sim_power_state == POWER_STATE_SLEEP) {
        g_sim_sleep_time_ms += ms;
    }
}

void power_simulation_get_stats(uint32_t *active_ms, uint32_t *sleep_ms, uint32_t *uah) {
    if (active_ms) *active_ms = g_sim_active_time_ms;
    if (sleep_ms) *sleep_ms = g_sim_sleep_time_ms;
    if (uah) *uah = g_sim_total_uah;
}

/* ------------------------------------------------------------ */
/*                          Fault Injection                      */
/* ------------------------------------------------------------ */
typedef enum {
    FAULT_NONE,
    FAULT_SENSOR_I2C_ERROR,
    FAULT_SENSOR_VALUE_OUT_OF_RANGE,
    FAULT_MQTT_CONNECTION_LOST,
    FAULT_OTA_FAILURE,
    FAULT_WIFI_DISCONNECT
} fault_sim_type_t;

static fault_sim_state_t g_sim_fault_state = FAULT_NONE;
static uint32_t g_sim_fault_start_time = 0;
static uint32_t g_sim_fault_duration_ms = 0;

void fault_simulation_init(void) {
    g_sim_fault_state = FAULT_NONE;
    printf("[SIM FAULT] Fault simulation initialized\n");
}

void fault_simulation_inject_fault(fault_sim_type_t type, uint32_t duration_ms) {
    g_sim_fault_state = type;
    g_sim_fault_start_time = 0; /* 0 = immediate */
    g_sim_fault_duration_ms = duration_ms;
    printf("[SIM FAULT] Injecting fault: %s for %dms\n", 
           type == FAULT_SENSOR_I2C_ERROR ? "I2C error" :
           type == FAULT_SENSOR_VALUE_OUT_OF_RANGE ? "value out of range" :
           type == FAULT_MQTT_CONNECTION_LOST ? "MQTT lost" :
           type == FAULT_OTA_FAILURE ? "OTA failure" : "WiFi disconnect",
           duration_ms);
}

bool fault_simulation_is_fault_active(void) {
    if (g_sim_fault_state == FAULT_NONE) return false;
    
    /* Check if fault duration has elapsed */
    if (g_sim_fault_duration_ms == 0) return true; /* Permanent fault */
    
    /* In real implementation: check elapsed time */
    return true; /* For simulation: always active once injected */
}

fault_sim_type_t fault_simulation_get_active_fault(void) {
    return g_sim_fault_state;
}

void fault_simulation_clear_fault(void) {
    g_sim_fault_state = FAULT_NONE;
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