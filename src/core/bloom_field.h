#pragma once

#include <cstdint>
#include "config.h"

#define BLOOM_COLS ((SCREEN_WIDTH - 6) / 6)
#define BLOOM_ROWS ((SCREEN_HEIGHT - 34) / 8)

struct BloomCell {
    uint8_t energy;
    uint8_t age;
};

struct BloomField {
    BloomCell cells[BLOOM_ROWS][BLOOM_COLS];
    uint32_t lastSampleIndex;
};

namespace BloomFieldOps {

void reset(BloomField& field);
void seed(BloomField& field);
void inject(BloomField& field, uint8_t level, float progress);
void injectAt(BloomField& field, uint8_t level, int cx, int cy);
void step(BloomField& field);
void tick(BloomField& field, uint32_t now);
char glyphForEnergy(uint8_t energy);

}
