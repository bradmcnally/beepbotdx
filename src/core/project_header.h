#pragma once

#include <cstdint>
#include "project.h"
#include "slot_fx.h"

static const uint32_t PROJECT_MAGIC = 0x42505844; // "BPXD"
static const uint8_t PROJECT_VERSION = 2;

struct ProjectHeader {
    uint32_t magic;
    uint8_t version;
    uint16_t bpm;
    uint8_t themeIndex;
    char name[9];
    uint8_t soundOccupied[NUM_SOUNDS];
    char soundNames[NUM_SOUNDS][9];
    uint8_t soundLevels[NUM_SOUNDS];
    uint8_t fxValues[NUM_SOUNDS][NUM_FX];
    uint8_t fxEnabled[NUM_SOUNDS];
    Pattern patterns[NUM_PATTERNS];
    uint8_t song[NUM_SONG_POSITIONS];
};
