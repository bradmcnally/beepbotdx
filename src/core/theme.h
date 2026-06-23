#pragma once

#include <cstdint>

struct Theme {
    uint16_t accent;     // full brightness accent
    uint16_t dim;        // ~20% opacity on dark bg
    uint16_t dark;       // ~15% for secondary elements
    uint16_t highlight;  // bright variant for playheads/active
    uint16_t bg;         // ~5% tint for background
    uint16_t measure;    // ~35% for beat markers (columns 1,5,9,13)
};

namespace ThemeOps {

// 8 preset accent colors
static const uint8_t NUM_PRESETS = 8;

// Get theme for a preset index (0-7)
Theme getPreset(uint8_t index);

// Get preset name
const char* getPresetName(uint8_t index);

// Convert 8-bit RGB to RGB565
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

}
