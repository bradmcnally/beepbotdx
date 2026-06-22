#pragma once

#include <cstdint>

#define BLOOM_COLS 39
#define BLOOM_ROWS 12

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
char glyphForEnergy(uint8_t energy);

}
