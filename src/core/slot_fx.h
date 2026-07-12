#pragma once

#include <cstdint>

#define NUM_FX 4
#define FX_PITCH 0
#define FX_CRUSH 1
#define FX_LPF   2
#define FX_HPF   3

struct SlotFx {
    uint8_t value[NUM_FX];
    bool enabled[NUM_FX];
};

namespace SlotFxOps {

inline void defaults(SlotFx& fx) {
    fx.value[FX_PITCH] = 12;  // 0 semitones (range 0-24, center=12)
    fx.value[FX_CRUSH] = 0;
    fx.value[FX_LPF] = 100;
    fx.value[FX_HPF] = 0;
    fx.enabled[FX_PITCH] = true;
    fx.enabled[FX_CRUSH] = true;
    fx.enabled[FX_LPF] = true;
    fx.enabled[FX_HPF] = true;
}

inline uint8_t defaultValue(int index) {
    if (index == FX_PITCH) return 12;
    if (index == FX_LPF) return 100;
    return 0;
}

inline uint8_t maxValue(int index) {
    if (index == FX_PITCH) return 24;
    return 100;
}

inline bool anyActive(const SlotFx& fx) {
    for (int i = 0; i < NUM_FX; i++) {
        if (fx.enabled[i] && fx.value[i] != defaultValue(i)) return true;
    }
    return false;
}

}
