#pragma once

#include <cstdint>
#include <cmath>
#include "slot_fx.h"

struct FxFilterState {
    float lpfPrev;
    float hpfPrev;
    float hpfInput;
};

namespace FxDsp {

inline void initFilterState(FxFilterState& state) {
    state.lpfPrev = 0;
    state.hpfPrev = 0;
    state.hpfInput = 0;
}

inline float pitchRate(uint8_t value) {
    int semitones = (int)value - 12;
    return powf(2.0f, semitones / 12.0f);
}

inline int16_t bitcrush(int16_t sample, uint8_t value) {
    if (value == 0) return sample;
    int bits = 16 - (value * 15 / 100);
    if (bits < 1) bits = 1;
    int shift = 16 - bits;
    return (sample >> shift) << shift;
}

inline float lpfAlpha(uint8_t value, float sampleRate) {
    if (value >= 100) return 1.0f;
    float minFreq = 100.0f;
    float maxFreq = sampleRate * 0.49f;
    float freq = minFreq * powf(maxFreq / minFreq, value / 100.0f);
    float rc = 1.0f / (2.0f * 3.14159f * freq);
    float dt = 1.0f / sampleRate;
    return dt / (rc + dt);
}

inline float hpfAlpha(uint8_t value, float sampleRate) {
    if (value == 0) return 1.0f;
    float minFreq = 20.0f;
    float maxFreq = sampleRate * 0.4f;
    float freq = minFreq * powf(maxFreq / minFreq, value / 100.0f);
    float rc = 1.0f / (2.0f * 3.14159f * freq);
    float dt = 1.0f / sampleRate;
    return rc / (rc + dt);
}

inline int16_t processSample(int16_t raw, const uint8_t values[NUM_FX],
                             const bool enabled[NUM_FX], FxFilterState& state,
                             float sampleRate) {
    int16_t s = raw;

    if (enabled[FX_CRUSH] && values[FX_CRUSH] > 0) {
        s = bitcrush(s, values[FX_CRUSH]);
    }

    if (enabled[FX_LPF] && values[FX_LPF] < 100) {
        float alpha = lpfAlpha(values[FX_LPF], sampleRate);
        float out = state.lpfPrev + alpha * ((float)s - state.lpfPrev);
        state.lpfPrev = out;
        s = (int16_t)out;
    }

    if (enabled[FX_HPF] && values[FX_HPF] > 0) {
        float alpha = hpfAlpha(values[FX_HPF], sampleRate);
        float out = alpha * (state.hpfPrev + (float)s - state.hpfInput);
        state.hpfInput = (float)s;
        state.hpfPrev = out;
        s = (int16_t)out;
    }

    return s;
}

inline bool hasActiveFx(const SlotFx& fx) {
    if (fx.enabled[FX_PITCH] && fx.value[FX_PITCH] != 12) return true;
    if (fx.enabled[FX_CRUSH] && fx.value[FX_CRUSH] != 0) return true;
    if (fx.enabled[FX_LPF] && fx.value[FX_LPF] != 100) return true;
    if (fx.enabled[FX_HPF] && fx.value[FX_HPF] != 0) return true;
    return false;
}

}
