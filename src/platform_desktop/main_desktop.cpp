#include <SDL2/SDL.h>
#include <cstdio>
#include "config.h"
#include "core/app.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "boot_image.h"

#define FIRMWARE_VERSION "v1.0.0"

static App app;

static uint8_t loadLastSlot() {
    FILE* f = fopen("beepbotdx_data/lastslot", "rb");
    if (!f) return 0;
    uint8_t slot = 0;
    fread(&slot, 1, 1, f);
    fclose(f);
    return slot;
}

static void saveLastSlot(uint8_t slot) {
    FILE* f = fopen("beepbotdx_data/lastslot", "wb");
    if (!f) return;
    fwrite(&slot, 1, 1, f);
    fclose(f);
}

static void onSaveSlot(uint8_t slot) {
    saveLastSlot(slot);
}

static void loadSettings(GlobalSettings& settings) {
    FILE* f = fopen("beepbotdx_data/settings", "rb");
    if (!f) return;
    uint8_t buf[4];
    size_t n = fread(buf, 1, 4, f);
    if (n >= 2) {
        settings.autoSave = buf[0] != 0;
        settings.ledMode = (LedMode)(buf[1] % 3);
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
    FILE* f = fopen("beepbotdx_data/settings", "wb");
    if (!f) return;
    uint8_t buf[4] = { (uint8_t)settings.autoSave, (uint8_t)settings.ledMode, (uint8_t)settings.confirmDelete, (uint8_t)settings.bootToProject };
    fwrite(buf, 1, 4, f);
    fclose(f);
}

static void showBootScreen() {
    Canvas& canvas = Display::canvas();
    Display::beginFrame();

    uint16_t* buf = canvas.buffer();
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t idx = BOOT_IMAGE[i];
        buf[i] = BOOT_PALETTE[idx % BOOT_PALETTE_COUNT];
    }

    canvas.setTextSize(1);
    canvas.setTextColor(0x7BEF);
    canvas.setTextDatum(top_left);
    canvas.drawString(FIRMWARE_VERSION, 4, SCREEN_HEIGHT - 12);

    canvas.setTextColor(0xFFFF);
    canvas.setTextDatum(top_center);
    canvas.drawString("beepbot", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12);

    canvas.setTextColor(0x7BEF);
    canvas.setTextDatum(top_right);
    canvas.drawString("Help", SCREEN_WIDTH - 4, SCREEN_HEIGHT - 12);

    Display::endFrame();

    uint32_t startTime = SDL_GetTicks();
    bool waiting = true;
    while (waiting && !Input::shouldQuit()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return;
            if (e.type == SDL_KEYDOWN) { waiting = false; break; }
        }
        if (SDL_GetTicks() - startTime > 2500) waiting = false;
        SDL_Delay(10);
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();

    showBootScreen();

    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    callbacks.saveSettings = onSaveSettings;
    app.init(callbacks);
    loadSettings(app.getSettings());
    uint8_t lastSlot = loadLastSlot();
    if (app.getSettings().bootToProject) {
        app.openProjectList(lastSlot);
    } else {
        app.loadSlot(lastSlot);
    }

    while (!Input::shouldQuit()) {
        app.tick();
        SDL_Delay(10);
    }

    Display::shutdown();
    return 0;
}
