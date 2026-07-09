#pragma once

#include <cstdint>

#ifdef ESP_PLATFORM
#include <pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#endif

static const uint16_t BOOT_PALETTE[] PROGMEM = {
    0x0000, // 0: black
    0xFFFF, // 1: white
};

static const uint8_t BOOT_PALETTE_COUNT = 2;

// Placeholder: 240x135 image data, one byte per pixel (palette index)
// Replace with actual image data exported from your tool
static const uint8_t BOOT_IMAGE[240 * 135] PROGMEM = {0};
