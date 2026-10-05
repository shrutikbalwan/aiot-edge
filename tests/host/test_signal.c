#include <assert.h>
#include <stdint.h>

#include "signal_processing.h"

int main(void)
{
    uint32_t samples[100] = {0};
    const uint32_t pulse[8] = {0, 100, 400, 1000, 1800, 1000, 400, 100};
    for (size_t i = 0; i < 100; ++i) samples[i] = 52000U + pulse[i % 8U];
    uint16_t bpm = 0;
    assert(signal_estimate_heart_rate(samples, 100, 10, &bpm));
    assert(bpm >= 74 && bpm <= 76);
    assert(!signal_estimate_heart_rate(samples, 4, 10, &bpm));
    assert(!signal_estimate_heart_rate(NULL, 100, 10, &bpm));
    for (size_t i = 0; i < 100; ++i) samples[i] = 42;
    assert(!signal_estimate_heart_rate(samples, 100, 10, &bpm));
    return 0;
}
