#include "audio_features.h"

#include <limits.h>
#include <string.h>

esp_err_t audio_features_init(void)
{
    return ESP_OK;
}

esp_err_t audio_features_extract(const int16_t *pcm, size_t sample_count,
                                 aiot_audio_features_t *features)
{
    if (pcm == NULL || features == NULL || sample_count < AIOT_AUDIO_FEATURE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(features, 0, sizeof(*features));
    const size_t bin_width = sample_count / AIOT_AUDIO_FEATURE_COUNT;
    if (bin_width == 0U) return ESP_ERR_INVALID_SIZE;
    for (size_t bin = 0; bin < AIOT_AUDIO_FEATURE_COUNT; ++bin) {
        int64_t magnitude_sum = 0;
        for (size_t i = 0; i < bin_width; ++i) {
            const int32_t value = pcm[bin * bin_width + i];
            magnitude_sum += value < 0 ? -(int64_t)value : value;
        }
        int64_t quantized = magnitude_sum / (int64_t)bin_width / 128;
        if (quantized > INT8_MAX) quantized = INT8_MAX;
        features->values[bin] = (int8_t)quantized;
    }
    features->count = AIOT_AUDIO_FEATURE_COUNT;
    features->scale = 128.0F;
    features->zero_point = 0;
    return ESP_OK;
}
