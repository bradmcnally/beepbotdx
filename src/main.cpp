#include <M5Cardputer.h>
#include <Preferences.h>
#include "config.h"
#include "core/app.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "platform/led.h"
#include "boot_image.h"
#if ENABLE_SCREENSHOTS
#include "platform/screenshot.h"
#endif

#define FIRMWARE_VERSION "v1.0.0"

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
    Display::setBrightness((displayBrightness * 255) / 100);
    brightnessDirty = true;
    brightnessChangeTime = millis();
}

static void onSaveSettings(const GlobalSettings& settings) {
    prefs.begin("beepbotdx", false);
    prefs.putBool("autoSave", settings.autoSave);
    prefs.putUChar("ledMode", (uint8_t)settings.ledMode);
    prefs.putBool("confirmDel", settings.confirmDelete);
    prefs.putBool("bootProj", settings.bootToProject);
    prefs.end();
}

static bool onScreenshot() {
#if ENABLE_SCREENSHOTS
    takeScreenshot();
    return true;
#else
    return false;
#endif
}

static void showBootScreen() {
    Canvas& canvas = Display::canvas();
    Display::beginFrame();

    // Render boot image from palette-indexed data
    uint16_t* buf = canvas.buffer();
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t idx = pgm_read_byte(&BOOT_IMAGE[i]);
        buf[i] = pgm_read_word(&BOOT_PALETTE[idx % BOOT_PALETTE_COUNT]);
    }

    // Version (lower-left)
    canvas.setTextSize(1);
    canvas.setTextColor(0x7BEF);
    canvas.setTextDatum(top_left);
    canvas.drawString(FIRMWARE_VERSION, 4, SCREEN_HEIGHT - 12);

    // "beepbot" (lower-center)
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(top_center);
    canvas.drawString("beepbot", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12);

    // "Help" (lower-right)
    canvas.setTextColor(0x7BEF);
    canvas.setTextDatum(top_right);
    canvas.drawString("Help", SCREEN_WIDTH - 4, SCREEN_HEIGHT - 12);

    Display::endFrame();

    // Wait for key press or timeout
    uint32_t startTime = millis();
    bool waiting = true;
    while (waiting) {
        M5Cardputer.update();
        if (millis() - startTime > 2500) { waiting = false; break; }
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            waiting = false;
        }
        delay(10);
    }
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();
    LED::init();

    prefs.begin("beepbotdx", false);
    displayBrightness = prefs.getUChar("brightness", 80);
    uint8_t lastSlot = prefs.getUChar("lastSlot", 0);
    bool autoSave = prefs.getBool("autoSave", false);
    uint8_t ledMode = prefs.getUChar("ledMode", 0);
    bool confirmDel = prefs.getBool("confirmDel", true);
    bool bootProj = prefs.getBool("bootProj", false);
    prefs.end();
    Display::setBrightness((displayBrightness * 255) / 100);

    showBootScreen();

    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    callbacks.setBrightness = onSetBrightness;
    callbacks.onScreenshot = onScreenshot;
    callbacks.saveSettings = onSaveSettings;
    app.init(callbacks);
    app.setBrightness(displayBrightness);
    app.getSettings().autoSave = autoSave;
    app.getSettings().ledMode = (LedMode)ledMode;
    app.getSettings().confirmDelete = confirmDel;
    app.getSettings().bootToProject = bootProj;
    if (bootProj) {
        app.openProjectList(lastSlot);
    } else {
        app.loadSlot(lastSlot);
    }
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
