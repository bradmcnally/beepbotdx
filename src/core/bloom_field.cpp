#include "bloom_field.h"
#include <cstring>

void BloomFieldOps::reset(BloomField& field) {
    memset(field.cells, 0, sizeof(field.cells));
    field.lastSampleIndex = 0;
    // Initialize at resting level (in '.' band)
    for (int r = 0; r < BLOOM_ROWS; r++) {
        for (int c = 0; c < BLOOM_COLS; c++) {
            field.cells[r][c].energy = 12;
            field.cells[r][c].age = 128;
        }
    }
}

void BloomFieldOps::seed(BloomField& field) {
    // Not used in ripple mode — kept for recording
    (void)field;
}

void BloomFieldOps::injectAt(BloomField& field, uint8_t level, int cx, int cy) {
    if (level < 8) return;
    int radius = 3 + (level / 40);

    for (int r = cy - radius; r <= cy + radius; r++) {
        for (int c = cx - radius; c <= cx + radius; c++) {
            if (r < 0 || r >= BLOOM_ROWS || c < 0 || c >= BLOOM_COLS) continue;
            int dist = (r > cy ? r - cy : cy - r) + (c > cx ? c - cx : cx - c);
            if (dist > radius) continue;

            uint8_t add = level * (radius - dist + 1) / (radius + 1);
            uint16_t newVal = field.cells[r][c].energy + add;
            field.cells[r][c].energy = newVal > 255 ? 255 : (uint8_t)newVal;
        }
    }
}

void BloomFieldOps::inject(BloomField& field, uint8_t level, float progress) {
    int cx = BLOOM_COLS / 2;
    int cy = BLOOM_ROWS / 2;
    if (level < 8) return;
    int radius = 1 + (int)(progress * 3);

    for (int r = cy - radius; r <= cy + radius; r++) {
        for (int c = cx - radius; c <= cx + radius; c++) {
            if (r < 0 || r >= BLOOM_ROWS || c < 0 || c >= BLOOM_COLS) continue;
            int dist = (r > cy ? r - cy : cy - r) + (c > cx ? c - cx : cx - c);
            if (dist > radius) continue;

            uint8_t add = level * (radius - dist + 1) / (radius + 1);
            uint16_t newVal = field.cells[r][c].energy + add;
            field.cells[r][c].energy = newVal > 255 ? 255 : (uint8_t)newVal;
        }
    }
}

void BloomFieldOps::step(BloomField& field) {
    // Ripple simulation: new = average of cardinal neighbors + (self - previous)
    // We use age as "velocity" (stored as offset from 128 neutral)
    uint8_t next[BLOOM_ROWS][BLOOM_COLS];
    uint8_t nextVel[BLOOM_ROWS][BLOOM_COLS];

    for (int r = 0; r < BLOOM_ROWS; r++) {
        for (int c = 0; c < BLOOM_COLS; c++) {
            int16_t self = field.cells[r][c].energy;
            int16_t vel = (int16_t)field.cells[r][c].age - 128;

            // Average of cardinal neighbors
            int16_t sum = 0;
            uint8_t n = 0;
            if (r > 0) { sum += field.cells[r-1][c].energy; n++; }
            if (r < BLOOM_ROWS-1) { sum += field.cells[r+1][c].energy; n++; }
            if (c > 0) { sum += field.cells[r][c-1].energy; n++; }
            if (c < BLOOM_COLS-1) { sum += field.cells[r][c+1].energy; n++; }
            int16_t avg = sum / n;

            // Acceleration toward neighbor average
            vel += (avg - self) / 2;

            // Damping — waves die out naturally
            vel = vel * 11 / 16;

            // Apply velocity
            int16_t newE = self + vel;

            // Pull toward resting level (stronger when further away)
            int16_t diff = newE - 12;
            if (diff > 0) newE -= 1 + diff / 32;
            else if (diff < 0) newE++;

            if (newE < 10) newE = 10;
            if (newE > 255) newE = 255;

            next[r][c] = (uint8_t)newE;
            nextVel[r][c] = (uint8_t)(vel + 128);
        }
    }

    for (int r = 0; r < BLOOM_ROWS; r++) {
        for (int c = 0; c < BLOOM_COLS; c++) {
            field.cells[r][c].energy = next[r][c];
            field.cells[r][c].age = nextVel[r][c];
        }
    }
}

void BloomFieldOps::tick(BloomField& field, uint32_t now) {
    uint32_t last = field.lastSampleIndex;
    if (last == 0) last = now;
    while (now - last >= 16) {
        step(field);
        last += 16;
    }
    field.lastSampleIndex = last;
}

static const char GLYPHS[] = ".:;+*xo%#@";
static const int GLYPH_COUNT = 10;

char BloomFieldOps::glyphForEnergy(uint8_t energy) {
    if (energy == 0) return ' ';
    int idx = (energy * GLYPH_COUNT) / 256;
    if (idx >= GLYPH_COUNT) idx = GLYPH_COUNT - 1;
    return GLYPHS[idx];
}
