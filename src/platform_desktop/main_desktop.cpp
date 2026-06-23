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

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Display::init();
    Input::init();
    Audio::init();
    Storage::init();
    Memory::init();

    AppCallbacks callbacks = {};
    callbacks.saveSlot = onSaveSlot;
    app.init(callbacks);
    app.loadSlot(loadLastSlot());

    while (!Input::shouldQuit()) {
        app.tick();
        SDL_Delay(10);
    }

    Display::shutdown();
    return 0;
}
