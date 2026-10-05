#pragma once

#include "aiot_types.h"
#include "esp_err.h"

/* CPU reference abstraction. A production TFLM/ESP-NN backend is not bundled. */
esp_err_t model_runtime_init(void);
esp_err_t model_runtime_infer(const aiot_audio_features_t *features,
                              aiot_wake_event_t *event, bool *detected);
