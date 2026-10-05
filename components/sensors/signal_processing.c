#include "signal_processing.h"

#include <limits.h>

bool signal_estimate_heart_rate(const uint32_t *infrared, size_t count,
                                uint32_t sample_rate_hz, uint16_t *bpm)
{
    if (infrared == NULL || bpm == NULL || count < 5U || sample_rate_hz == 0U) {
        return false;
    }

    uint64_t sum = 0;
    uint32_t minimum = UINT32_MAX;
    uint32_t maximum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += infrared[i];
        if (infrared[i] < minimum) minimum = infrared[i];
        if (infrared[i] > maximum) maximum = infrared[i];
    }
    const uint32_t mean = (uint32_t)(sum / count);
    const uint32_t span = maximum - minimum;
    if (span < 100U) {
        return false;
    }
    const uint32_t threshold = mean + span / 8U;
    const size_t minimum_peak_distance = (size_t)((sample_rate_hz * 60U) / 220U);
    size_t previous_peak = 0;
    uint64_t interval_sum = 0;
    size_t intervals = 0;

    for (size_t i = 1; i + 1U < count; ++i) {
        const bool peak = infrared[i] > threshold && infrared[i] >= infrared[i - 1U] &&
                          infrared[i] > infrared[i + 1U];
        if (!peak) continue;
        if (previous_peak != 0U && i - previous_peak >= minimum_peak_distance) {
            interval_sum += i - previous_peak;
            ++intervals;
            previous_peak = i;
        } else if (previous_peak == 0U) {
            previous_peak = i;
        }
    }
    if (intervals == 0U || interval_sum == 0U) {
        return false;
    }
    const uint64_t calculated = (60ULL * sample_rate_hz * intervals) / interval_sum;
    if (calculated < 30U || calculated > 220U) {
        return false;
    }
    *bpm = (uint16_t)calculated;
    return true;
}
