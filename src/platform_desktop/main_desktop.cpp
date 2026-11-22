#include <SDL2/SDL.h>
#include <cstdio>
#include "config.h"
#include "core/app.h"
#include "core/pattern_gen.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "platform/led.h"
#include "storage_paths.h"
#include "boot_image.h"
#ifdef CARDPUTER_ZERO_DEVICE
#include "native_io.h"
#endif

#define FIRMWARE_VERSION "v0.6.0"

static App app;

static uint8_t loadLastSlot() {
    FILE* f = fopen((Storage::dataDirectory() / "lastslot").c_str(), "rb");
    if (!f) return 0;
    uint8_t slot = 0;
    fread(&slot, 1, 1, f);
    fclose(f);
    return slot < 8 ? slot : 0;
}

static void saveLastSlot(uint8_t slot) {
    FILE* f = fopen((Storage::dataDirectory() / "lastslot").c_str(), "wb");
    if (!f) return;
    fwrite(&slot, 1, 1, f);
    fclose(f);
}

static void onSaveSlot(uint8_t slot) {
    saveLastSlot(slot);
}

static void loadSettings(GlobalSettings& settings) {
    FILE* f = fopen((Storage::dataDirectory() / "settings").c_str(), "rb");
    if (!f) return;
    uint8_t buf[4];
    size_t n = fread(buf, 1, 4, f);
    if (n >= 2) {
        settings.autoSave = buf[0] != 0;
        settings.ledMode = normalizeLedMode(static_cast<LedMode>(buf[1] % 3));
    }
    if (n >= 3) {
        settings.confirmDelete = buf[2] != 0;
    }
    if (n >= 4) {
        settings.bootToProject = buf[3] != 0;
    }
    fclose(f);
}

static void onSaveSettings(const GlobalSettings& settings) {
    FILE* f = fopen((Storage::dataDirectory() / "settings").c_str(), "wb");
    if (!f) return;
    uint8_t buf[4] = { (uint8_t)settings.autoSave, (uint8_t)settings.ledMode, (uint8_t)settings.confirmDelete, (uint8_t)settings.bootToProject };
    fwrite(buf, 1, 4, f);
    fclose(f);
}

static void showBootScreen(uint16_t accentColor) {
    Canvas& canvas = Display::canvas();
    Display::beginFrame();

    uint16_t palette[BOOT_PALETTE_COUNT];
    for (int i = 0; i < BOOT_PALETTE_COUNT; i++)
        palette[i] = BOOT_PALETTE[i];
    if (BOOT_PALETTE_COUNT > 1)
        palette[1] = accentColor;

    uint16_t bg = palette[0];
    uint16_t* buf = canvas.buffer();
    int ox = (SCREEN_WIDTH - BOOT_IMAGE_W) / 2;
    int oy = (SCREEN_HEIGHT - BOOT_IMAGE_H) / 2;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int sx = x - ox;
            int sy = y - oy;
            if (sx >= 0 && sx < BOOT_IMAGE_W && sy >= 0 && sy < BOOT_IMAGE_H) {
                uint8_t idx = BOOT_IMAGE[sy * BOOT_IMAGE_W + sx];
                buf[y * SCREEN_WIDTH + x] = palette[idx % BOOT_PALETTE_COUNT];
            } else {
                buf[y * SCREEN_WIDTH + x] = bg;
            }
        }
    }

    canvas.setTextSize(1);
    canvas.setTextColor(accentColor);
    canvas.setTextDatum(top_left);
    canvas.drawString(FIRMWARE_VERSION, 4, SCREEN_HEIGHT - 12);

    canvas.setTextColor(0xFFFF);
    canvas.setTextDatum(top_center);
    canvas.drawString("beepbot", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12);

    canvas.setTextColor(accentColor);
    canvas.setTextDatum(top_right);
    canvas.drawString("HELP", SCREEN_WIDTH - 4, SCREEN_HEIGHT - 12);
    int hX = SCREEN_WIDTH - 4 - (4 * 6);
    canvas.fillRect(hX, SCREEN_HEIGHT - 12 + 8, 5, 1, accentColor);

    Display::endFrame();

    uint32_t startTime = SDL_GetTicks();
    while (!Input::shouldQuit() && SDL_GetTicks() - startTime < 1000) {
        if (Input::poll() == INPUT_ESC) break;
        SDL_Delay(10);
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (!Storage::init()) {
        SDL_Log("Could not create workspace: %s", Storage::dataDirectory().c_str());
        return 1;
    }
#ifdef CARDPUTER_ZERO_DEVICE
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return 1;
    }
    if (!Zero::nativeIo.open()) {
        SDL_Quit();
        return 1;
    }
#endif
    Display::init();
    Input::init();
    Audio::init();
    Memory::init();
    PatternGen::init();

    uint8_t lastSlot = loadLastSlot();
    uint8_t themeIdx = Storage::loadProjectTheme(lastSlot);
    Theme bootTheme = ThemeOps::getPreset(themeIdx);
    showBootScreen(bootTheme.accent);
    if (Input::shouldQuit()) {
        Display::shutdown();
        return 0;
    }

    LED::init();
    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    callbacks.saveSettings = onSaveSettings;
    app.init(callbacks);
    loadSettings(app.getSettings());
    if (app.getSettings().bootToProject) {
        app.openProjectList(lastSlot);
    } else {
        app.loadSlot(lastSlot);
    }

    while (!Input::shouldQuit()) {
        app.tick();
#ifdef CARDPUTER_ZERO
        if (app.shouldQuit()) {
            onSaveSettings(app.getSettings());
            break;
        }
#endif
        SDL_Delay(10);
    }

    LED::shutdown();
    Display::shutdown();
    return 0;
}
