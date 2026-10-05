#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AIOT_PROTOCOL_VERSION 1U
#define AIOT_PPG_WINDOW_SAMPLES 100U
#define AIOT_SENSOR_SAMPLE_RATE_HZ 10U
#define AIOT_AUDIO_FEATURE_COUNT 40U
#define AIOT_OTA_URL_MAX 256U
#define AIOT_VERSION_MAX 32U

typedef struct {
    uint64_t timestamp_ms;
    uint32_t red;
    uint32_t infrared;
    int32_t temperature_milli_c;
    int16_t accel_x_mg;
    int16_t accel_y_mg;
    int16_t accel_z_mg;
    bool simulated;
} aiot_sensor_sample_t;

typedef struct {
    uint16_t heart_rate_bpm;
    uint16_t spo2_permille;
    bool heart_rate_valid;
    bool spo2_valid;
    bool rhythm_irregular;
} aiot_health_estimate_t;

typedef struct {
    int8_t values[AIOT_AUDIO_FEATURE_COUNT];
    size_t count;
    float scale;
    int32_t zero_point;
} aiot_audio_features_t;

typedef struct {
    uint64_t timestamp_ms;
    uint16_t confidence_permille;
    char label[16];
} aiot_wake_event_t;

typedef struct {
    uint32_t uptime_seconds;
    uint32_t free_heap_bytes;
    int8_t wifi_rssi_dbm;
    bool ble_connected;
    bool mqtt_connected;
} aiot_device_status_t;

typedef enum {
    AIOT_OTA_IDLE = 0,
    AIOT_OTA_DOWNLOADING,
    AIOT_OTA_VERIFYING,
    AIOT_OTA_READY_TO_REBOOT,
    AIOT_OTA_FAILED,
} aiot_ota_state_t;

typedef struct {
    aiot_ota_state_t state;
    uint8_t progress_percent;
    int32_t error_code;
    char target_version[AIOT_VERSION_MAX];
} aiot_ota_status_t;

typedef enum {
    AIOT_COMMAND_NONE = 0,
    AIOT_COMMAND_STATUS,
    AIOT_COMMAND_REBOOT,
    AIOT_COMMAND_OTA,
} aiot_command_kind_t;

typedef struct {
    aiot_command_kind_t kind;
    char ota_url[AIOT_OTA_URL_MAX];
    char version[AIOT_VERSION_MAX];
} aiot_command_t;
