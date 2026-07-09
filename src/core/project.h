#pragma once

#include <cstdint>
#include "config.h"
#include "sound_slot.h"

struct Pattern {
    uint8_t steps[NUM_STEPS];
};

struct Project {
    uint16_t bpm;
    uint8_t themeIndex;
    char name[9];
    SoundSlot sounds[NUM_SOUNDS];
    Pattern patterns[NUM_PATTERNS];
    uint8_t song[NUM_SONG_POSITIONS];
    bool dirty;

    static void init(Project& p);
};
