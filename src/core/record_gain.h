#pragma once

#include <cstdint>

namespace RecordGain {

inline int16_t apply(int16_t sample, uint8_t gain) {
    int32_t amplified = static_cast<int32_t>(sample) * gain;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    return static_cast<int16_t>(amplified);
}

}
