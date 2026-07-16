#include <M5Cardputer.h>
#include "display.h"
#include "config.h"

static Canvas _canvas;
static M5Canvas _hwCanvas(&M5Cardputer.Display);
static bool _ready = false;

void Display::init() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    _canvas.create(SCREEN_WIDTH, SCREEN_HEIGHT);

    if (ESP.getFreeHeap() > 80000) {
        _hwCanvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
        _ready = true;
    }
}

void Display::beginFrame() {
    _canvas.fillScreen(TFT_BLACK);
}

void Display::endFrame() {
    if (!_ready) return;
    uint16_t* src = _canvas.buffer();
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            _hwCanvas.drawPixel(x, y, src[y * SCREEN_WIDTH + x]);
        }
    }
    _hwCanvas.pushSprite(0, 0);
}

Canvas& Display::canvas() {
    return _canvas;
}

void Display::shutdown() {}

#define TFT_BL 38
#define MIN_BRIGHT 160

void Display::setBrightness(uint8_t value) {
    uint8_t mapped = MIN_BRIGHT + (uint16_t)value * (255 - MIN_BRIGHT) / 255;
    analogWrite(TFT_BL, mapped);
}

void Display::toggleFullscreen() {}

