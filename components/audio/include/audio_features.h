#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aiot_types.h"
#include "esp_err.h"

esp_err_t audio_features_init(void);
esp_err_t audio_features_extract(const int16_t *pcm, size_t sample_count,
                                 aiot_audio_features_t *features);
