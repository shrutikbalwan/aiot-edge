/**
 * @file ai_accelerator.h
 * @brief Custom AI Accelerator ASIC Header (VLSI Domain)
 * @brief Hardware-accelerated neural network inference for edge devices
 */

#ifndef AI_ACCELERATOR_H
#define AI_ACCELERATOR_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
/** Handle to neural network model loaded into accelerator */
typedef void *nn_handle_t;

/** Network inference output result */
typedef struct {
    int8_t  class_ids[10];    /* Top-10 predicted class indices */
    float32_t confidences[10]; /* Confidence scores (0-255 quantized) */
    uint32_t inference_cycles;  /* CPU cycles for profiling */
    float_t  inference_ms;     /* Inference time in milliseconds */
} nn_result_t;

/* ------------------------------------------------------------ */
/*                                  Function Interface          */
/* ------------------------------------------------------------ */

/** 
 * @brief Initialize the AI accelerator hardware block
 * @details Power-on reset, clock gating, DMA setup
 * @return ESP_OK on success, ESP_ERROR_* on failure
 */
esp_err_t ai_accelerator_init(void);

/**
 * @brief Load a TensorFlow Lite model into the accelerator
 * @param model_path Path to .tflite model file (progmem or flash)
 * @nn_handle_out Output handle for subsequent inference calls
 * @return nn_handle_t on success, NULL on failure
 */
nn_handle_t ai_accelerator_load_model(const char *model_path);

/**
 * @brief Run neural network inference on pre-processed features
 * @param handle Model handle from ai_accelerator_load_model()
 * @param input_features Quantized input buffer (FFT features, size depends on model)
 * @param output_result Output structure to fill with inference results
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ai_accelerator_infer(nn_handle_t handle, 
                               const int8_t *input_features, 
                               nn_result_t *output_result);

/**
 * @brief Apply federated learning model delta from cloud
 * @param delta_json cJSON payload containing weight deltas
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ai_accelerator_apply_federated_delta(cJSON *delta_json);

/**
 * @brief Get accelerator hardware statistics
 * @param stats Output structure for power, temperature, utilization
 * @return esp_err_t
 */
esp_err_t ai_accelerator_get_stats(void *stats);

/* ------------------------------------------------------------ */
/*                          Security & Integrity              */
/* ------------------------------------------------------------ */
/**
 * @brief Verify model integrity via embedded PUF (Physical Unclonable Function)
 * @param model_hash Expected SHA-256 hash of the model
 * @return bool true if integrity verified
 */
bool ai_accelerator_verify_model_hash(const uint8_t *model_hash);

/**
 * @brief Enable/disable side-channel countermeasures
 * @param enable true to enable constant-time execution, false to disable
 */
void ai_accelerator_set_sidechannel_protection(bool enable);

/* ------------------------------------------------------------ */
/*                          Firmware Update Support           */
/* ------------------------------------------------------------ */
/**
 * @brief Secure OTA update for neural network accelerator firmware
 * @param new_fw_path Path to new accelerator firmware binary
 * @param expected_hash SHA-256 hash of expected firmware
 * @return esp_err_t
 */
esp_err_t ai_accelerator_secure_fw_update(const char *new_fw_path, 
                                          const uint8_t *expected_hash);

/* ------------------------------------------------------------ */
/*                          Deprecated / Legacy               */
/* ------------------------------------------------------------ */
/**
 * @brief Legacy: Run inference on CPU (for compatibility)
 * @deprecated Use ai_accelerator_infer() instead for hardware acceleration
 * @note Kept for backward compatibility with older models
 * @param input Input data buffer
 * @param output Output buffer (must be 10 classes)
 * @return int Number of classes evaluated
 */
__attribute__((deprecated))
int ai_accelerator_cpu_inference(const int8_t *input, int8_t *output);

#endif /* AI_ACCELERATOR_H */