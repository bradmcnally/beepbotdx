#pragma once

#include <cstdint>

struct Theme {
    uint16_t accent;        // full brightness accent
    uint16_t dim;           // secondary text, beat markers
    uint16_t dark;          // cell backgrounds
    uint16_t highlight;     // bright variant for status messages
    uint16_t bg;            // screen background
    uint16_t textOnAccent;  // black or white depending on accent luminance
};

namespace ThemeOps {

// 8 preset accent colors
static const uint8_t NUM_PRESETS = 8;

// Get theme for a preset index (0-7)
Theme getPreset(uint8_t index);

// Get preset name
const char* getPresetName(uint8_t index);

// Get preset accent RGB (8-bit)
void getPresetRGB(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b);

// Convert 8-bit RGB to RGB565
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

}
