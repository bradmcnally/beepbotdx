#include <M5Cardputer.h>
#include <Preferences.h>
#include "config.h"
#include "core/app.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"
#if ENABLE_SCREENSHOTS
#include "platform/screenshot.h"
#endif

static App app;
static Preferences prefs;
static uint8_t displayBrightness = 80;
static bool brightnessDirty = false;
static uint32_t brightnessChangeTime = 0;

static void onSaveSlot(uint8_t slot) {
    prefs.begin("beepbotdx", false);
    prefs.putUChar("lastSlot", slot);
    prefs.end();
}

static void onSetBrightness(uint8_t percent) {
    displayBrightness = percent;
    M5Cardputer.Display.setBrightness((displayBrightness * 255) / 100);
    brightnessDirty = true;
    brightnessChangeTime = millis();
}

static bool onScreenshot() {
#if ENABLE_SCREENSHOTS
    takeScreenshot();
    return true;
#else
    return false;
#endif
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();

    prefs.begin("beepbotdx", false);
    displayBrightness = prefs.getUChar("brightness", 80);
    uint8_t lastSlot = prefs.getUChar("lastSlot", 0);
    prefs.end();
    M5Cardputer.Display.setBrightness((displayBrightness * 255) / 100);

    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    callbacks.setBrightness = onSetBrightness;
    callbacks.onScreenshot = onScreenshot;
    app.init(callbacks);
    app.loadSlot(lastSlot);
}

void loop() {
    M5Cardputer.update();

    app.tick();

    // Persist brightness after 1s of no changes
    if (brightnessDirty && millis() - brightnessChangeTime > 1000) {
        prefs.begin("beepbotdx", false);
        prefs.putUChar("brightness", displayBrightness);
        prefs.end();
        brightnessDirty = false;
    }

    delay(10);
}
