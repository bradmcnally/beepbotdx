#include <SDL2/SDL.h>
#include <cstdio>
#include "config.h"
#include "core/app.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/storage.h"
#include "platform/memory.h"

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
    uint8_t buf[3];
    size_t n = fread(buf, 1, 3, f);
    if (n >= 2) {
        settings.autoSave = buf[0] != 0;
        settings.ledMode = (LedMode)(buf[1] % 3);
    }
    if (n >= 3) {
        settings.confirmDelete = buf[2] != 0;
    }
    fclose(f);
}

static void onSaveSettings(const GlobalSettings& settings) {
    FILE* f = fopen("beepbotdx_data/settings", "wb");
    if (!f) return;
    uint8_t buf[3] = { (uint8_t)settings.autoSave, (uint8_t)settings.ledMode, (uint8_t)settings.confirmDelete };
    fwrite(buf, 1, 3, f);
    fclose(f);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();

    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    callbacks.saveSettings = onSaveSettings;
    app.init(callbacks);
    loadSettings(app.getSettings());
    app.loadSlot(loadLastSlot());

    while (!Input::shouldQuit()) {
        app.tick();
        SDL_Delay(10);
    }

    Display::shutdown();
    return 0;
}
