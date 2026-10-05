#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool signal_estimate_heart_rate(const uint32_t *infrared, size_t count,
                                uint32_t sample_rate_hz, uint16_t *bpm);
