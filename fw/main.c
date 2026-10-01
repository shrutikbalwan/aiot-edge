/**
 * @file main.c
 * @brief AIoT-Edge Intelligent Voice-Health Monitoring Firmware
 * @target ARM Cortex-M4 with FPU, FreeRTOS
 */

#include <stdint.h>
#include <stddef.h>
#include "system_config.h"
#include "sensor_drivers.h"
#include "audio_codec.h"
#include "ble_transport.h"
#include "ai_accelerator.h"
#include "fft_lib.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define SAMPLE_RATE             (16000U)
#define FRAME_SIZE              (256U)
#define FFT_SIZE                (512U)
#define NUM_SENSOR_SAMPLES      (128U)

/* ------------------------------------------------------------ */
  /*                                  Types                         */
/* ------------------------------------------------------------ */
typedef struct {
    uint16_t audio_frame[FRAME_SIZE];
    uint16_t ppg_sample[NUM_SENSOR_SAMPLES];
    int16_t temp_sample;   /* Temperature raw value (°C * 256, MAX30205) */
    uint8_t  command_id;
    uint8_t  vital_signs_valid;
} edge_data_t;

/* ------------------------------------------------------------ */
  /*                          FreeRTOS Primitives                 */
/* ------------------------------------------------------------ */
#define SENSOR_DATA_QUEUE_LENGTH  4
static QueueHandle_t g_sensor_data_queue;
static SemaphoreHandle_t g_sensor_semaphore;
static TimerHandle_t g_sensor_timeout_timer;

/* Mutex for shared BLE data transmission */
static MutexHandle_t g_ble_data_mutex;

/* Wake word task notification index */
#define TASK_NOTIFY_WAKE_WORD     0x01

/* Wake word shared flags */
uint8_t g_wake_word_detected = 0;
uint32_t last_wake_word_detected = 0;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
static void vAudioProcessingTask(void *pvParameters);
static void vHealthMonitorTask(void *pvParameters);
static void vBleConnectivityTask(void *pvParameters);
static void vNNInferenceTask(void *pvParameters);

/* ------------------------------------------------------------ */
/*                          FreeRTOS Tasks                      */
/* ------------------------------------------------------------ */
void main(void) {
    /* Initialize hardware abstractions */
    system_init();
    sensor_init();
    audio_codec_init();
    ble_transport_init();
    ai_accelerator_init();

    /* Create FreeRTOS tasks */
    xTaskCreate(vAudioProcessingTask, "AudioProc", 512, NULL, 2, NULL);
    xTaskCreate(vHealthMonitorTask, "HealthMon", 256, NULL, 1, NULL);
    xTaskCreate(vBleConnectivityTask, "BLEConn", 512, NULL, 3, NULL);
    xTaskCreate(vNNInferenceTask, "NNInfer", 1024, NULL, 2, NULL);

    /* Initialize FreeRTOS primitives */
    g_sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_LENGTH, sizeof(edge_data_t));
    g_sensor_semaphore = xSemaphoreCreateBinary();
    g_ble_data_mutex = xMutexCreateRecursive();
    g_sensor_timeout_timer = xTimerCreate("sensor_timeout", pdMS_TO_TICKS(5000), pdFALSE, 0, vSensorTimeoutCallback);

    /* Start scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;);
}

/* ------------------------------------------------------------ */
/*          Audio Processing Task - Feature Extraction          */
/* ------------------------------------------------------------ */
static void vAudioProcessingTask(void *pvParameters) {
    uint16_t raw_samples[FRAME_SIZE];
    float_t normalized[FRAME_SIZE];
    float_t fft_input[FFT_SIZE];
    fft_cfg_t fft_cfg;

    fft_cfg = fft_init(FFT_SIZE, FFT_RADIX_2);

    for (;;) {
        /* Capture audio frame */
        audio_codec_capture(raw_samples, FRAME_SIZE);

        /* DC removal and normalization */
        for (int i = 0; i < FRAME_SIZE; i++) {
            normalized[i] = (float_t)raw_samples[i] / 32768.0f;
        }

        /* Windowing (Hann) */
        for (int i = 0; i < FFT_SIZE; i++) {
            fft_input[i] = normalized[i % FRAME_SIZE] * 
                          (0.5f * (1.0f - cosf(2.0f * PI * i / FFT_SIZE)));
        }

        /* Compute FFT */
        fft_execute(fft_cfg, fft_input);

        /* Extract MFCC-like features for NN input */
        extract_features(fft_input, g_edge_data.audio_frame);

        vTaskDelay(pdMS_TO_TICKS(20)); /* 50Hz frame rate */
    }
}

/* ------------------------------------------------------------ */
/*          Health Monitoring Task - Sensor Fusion              */
/* ------------------------------------------------------------ */
static void vHealthMonitorTask(void *pvParameters) {
    edge_data_t local_data;
    uint32_t notified;
    
    for (;;) {
        /* Read PPG and temperature sensors */
        local_data.ppg_sample[0] = sensor_read_ppg();
        local_data.temp_sample = sensor_read_temp();

        /* Heart rate estimation using peak detection */
        if (local_data.vital_signs_valid == 0) {
            estimate_heart_rate(local_data.ppg_sample, NUM_SENSOR_SAMPLES);
            local_data.vital_signs_valid = 1;
        }

        /* Irregular rhythm detection (ML-aided) */
        if (detect_irregular_rhythm(local_data.ppg_sample, NUM_SENSOR_SAMPLES)) {
            /* Trigger alert via BLE */
            ble_transport_send_alert(ALERT_ARRHYTHMIA);
        }

        /* Send sensor data to BLE task via queue (non-blocking) */
        if (g_sensor_data_queue) {
            xQueueSend(g_sensor_data_queue, &local_data, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); /* 10Hz sensor fusion */
    }
}

/* ------------------------------------------------------------ */
  /*          BLE Connectivity Task                               */
/* ------------------------------------------------------------ */
static void vBleConnectivityTask(void *pvParameters) {
    edge_data_t recv_data;
    
    for (;;) {
        /* Receive sensor data from health task via queue */
        if (g_sensor_data_queue && xQueueReceive(g_sensor_data_queue, &recv_data, pdMS_TO_TICKS(10))) {
            /* Send sensor data and command status to phone app */
            if (recv_data.vital_signs_valid) {
                /* Acquire mutex for thread-safe BLE transmission */
                if (xMutexRecursiveTake(g_ble_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    ble_transport_send_data(
                        &recv_data,
                        sizeof(edge_data_t)
                    );
                    xMutexRecursiveGive(g_ble_data_mutex);
                }
            }
        }

        /* Check for OTA update request */
        if (ble_transport_check_firmware_update()) {
            /* Trigger secure bootloader update */
            system_perform_ota_update();
        }

        vTaskDelay(pdMS_TO_TICKS(50)); /* 20Hz BLE update rate */
    }
}

/* ------------------------------------------------------------ */
/*          Neural Network Inference Task                       */
/* ------------------------------------------------------------ */
static void vNNInferenceTask(void *pvParameters) {
    nn_handle_t nn_handle;
    int8_t  acc_output[10]; /* 10 command classes */
    uint32_t start_tick, end_tick;
    uint32_t ulNotifiedValue;
    TickType_t last_wake_word_time = 0;
    const TickType_t wake_word_debounce_ms = 200; /* Debounce wake word detection */

    /* Load quantized neural network model */
    nn_handle = nn_accelerator_load_model("wake_word.tflite");

    for (;;) {
        /* Run inference on audio features */
        start_tick = xTaskGetTickCount();
        nn_accelerator_infer(nn_handle, g_edge_data.audio_frame, acc_output);
        end_tick = xTaskGetTickCount();

        /* Check for wake word / command */
        if (acc_output[0] > 0.7f) { /* Threshold for wake word */
            /* Debounce wake word detection (minimum 200ms between detections) */
            if (xTaskGetTickCount() - last_wake_word_time > pdMS_TO_TICKS(wake_word_debounce_ms)) {
                /* Give task notification to BLE task */
                ulNotifiedValue = xTaskNotifyGive(xTaskGetHandle("BLEConn"));
                /* Wake word flag for telemetry publish */
                g_wake_word_detected = 1;
                last_wake_word_time = xTaskGetTickCount();
            }
        }

        /* Print inference time for profiling */
        #ifdef DEBUG_PROFILING
        printf("NN Inference: %lu ticks\r\n", end_tick - start_tick);
        #endif

        vTaskDelay(pdMS_TO_TICKS(10)); /* 100Hz inference rate */
    }
}

/* ------------------------------------------------------------ */
  /*                          System Initialization               */
/* ------------------------------------------------------------ */
void system_init(void) {
    /* Configure system clock */
    sysclk_set_freq(SYS_CLK_48MHz);

    /* Enable FPU if available */
    #ifdef FPU_PRESENT
    core_enable_fpu();
    #endif

    /* Initialize hardware abstraction layer */
    hal_init();

    /* Initialize power management */
    pmgr_init();

    /* Start sensor timeout timer */
    if (g_sensor_timeout_timer) {
        xTimerStart(g_sensor_timeout_timer, 0);
    }
}

/* ------------------------------------------------------------ */
  /*                    Sensor Timeout Timer Callback             */
/* ------------------------------------------------------------ */
void vSensorTimeoutCallback(TimerHandle_t xTimer) {
    /* Reset sensor data flag on timeout */
    g_sensor_data_queue = NULL;
    ESP_LOGW("SYS", "Sensor data timeout - resetting queue");
}