#include "model_runtime.h"

#include <string.h>

esp_err_t model_runtime_init(void)
{
    return ESP_OK;
}

esp_err_t model_runtime_infer(const aiot_audio_features_t *features,
                              aiot_wake_event_t *event, bool *detected)
{
    if (features == NULL || event == NULL || detected == NULL ||
        features->count != AIOT_AUDIO_FEATURE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(event, 0, sizeof(*event));
    *detected = false;
    return ESP_ERR_NOT_SUPPORTED;
}
