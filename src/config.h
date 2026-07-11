#pragma once

// Audio
#define SAMPLE_RATE 16000
#define MAX_SAMPLE_LENGTH 32000  // 2.0s at 16kHz
#define NUM_VOICES 8
#define SPEAKER_VOLUME 200

// Project limits
#define NUM_SOUNDS 8
#define NUM_PATTERNS 16
#define NUM_STEPS 16
#define NUM_SONG_POSITIONS 16

// BPM
#define DEFAULT_BPM 120
#define MIN_BPM 60
#define MAX_BPM 240

// Display
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 240
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 135
#endif

// SD Card SPI pins (Cardputer ADV)
#define SD_SPI_CLK 40
#define SD_SPI_MISO 39
#define SD_SPI_MOSI 14
#define SD_SPI_CS 12

// Features
#define ENABLE_SCREENSHOTS 1
