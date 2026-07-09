#include "theme.h"

uint16_t ThemeOps::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct PresetDef {
    uint8_t r, g, b;
    const char* name;
};

static const PresetDef presets[ThemeOps::NUM_PRESETS] = {
    {0xA4, 0xE3, 0x52, "LIME"},
    {0x52, 0xE3, 0xE3, "CYAN"},
    {0xE3, 0x52, 0xA4, "PINK"},
    {0xE3, 0xA4, 0x52, "AMBER"},
    {0x52, 0xA4, 0xE3, "SKY"},
    {0xE3, 0xE3, 0x52, "YELLOW"},
    {0xA4, 0x52, 0xE3, "PURPLE"},
    {0xE3, 0x52, 0x52, "RED"},
};

Theme ThemeOps::getPreset(uint8_t index) {
    if (index >= NUM_PRESETS) index = 0;
    const PresetDef& p = presets[index];

    Theme t;
    t.accent = rgb565(p.r, p.g, p.b);
    t.dim = rgb565(p.r * 36 / 100, p.g * 36 / 100, p.b * 36 / 100);
    t.dark = rgb565(p.r * 8 / 100, p.g * 8 / 100, p.b * 8 / 100);
    t.highlight = rgb565(
        p.r + (255 - p.r) / 3,
        p.g + (255 - p.g) / 3,
        p.b + (255 - p.b) / 3
    );
    t.bg = 0x0000;
    int luma = (p.r * 299 + p.g * 587 + p.b * 114) / 1000;
    t.textOnAccent = luma > 150 ? 0x0000 : 0xFFFF;
    return t;
}

const char* ThemeOps::getPresetName(uint8_t index) {
    if (index >= ThemeOps::NUM_PRESETS) index = 0;
    return presets[index].name;
}

void ThemeOps::getPresetRGB(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (index >= NUM_PRESETS) index = 0;
    r = presets[index].r;
    g = presets[index].g;
    b = presets[index].b;
}
