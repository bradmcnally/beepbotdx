#pragma once

#include <cstdint>
#include "config.h"
#include "sound_slot.h"

struct Pattern {
    uint8_t steps[NUM_STEPS];
};

enum BitDepth : uint8_t {
    BIT_DEPTH_16 = 0,
    BIT_DEPTH_8 = 1,
};

struct Project {
    uint16_t bpm;
    uint8_t themeIndex;
    BitDepth bitDepth;
    char name[9];
    SoundSlot sounds[NUM_SOUNDS];
    Pattern patterns[NUM_PATTERNS];
    uint8_t song[NUM_SONG_POSITIONS];
    bool dirty;

    static void init(Project& p);
};
