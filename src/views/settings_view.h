#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"

enum LedMode : uint8_t {
    LED_ON = 0,
    LED_METRONOME = 1,
    LED_OFF = 2,
};

inline LedMode normalizeLedMode(LedMode mode) {
#ifdef CARDPUTER_ZERO
    return mode == LED_OFF ? LED_OFF : LED_METRONOME;
#else
    return mode;
#endif
}

inline LedMode nextLedMode(LedMode mode, bool backwards = false) {
#ifdef CARDPUTER_ZERO
    return mode == LED_OFF ? LED_METRONOME : LED_OFF;
#else
    return static_cast<LedMode>((mode + (backwards ? 2 : 1)) % 3);
#endif
}

inline bool ledShowsIdleColor(LedMode mode) {
#ifdef CARDPUTER_ZERO
    return false;
#else
    return mode != LED_OFF;
#endif
}

struct GlobalSettings {
    bool autoSave = false;
    LedMode ledMode = normalizeLedMode(LED_ON);
    bool confirmDelete = true;
    bool bootToProject = false;
    bool shakeGen = true;

    static GlobalSettings* instance;
};

class SettingsView : public View {
public:
    SettingsView(Project& project, Character& character, GlobalSettings& settings);
    void enter() override;
    void update(InputEvent event) override;
    void draw(Canvas& canvas) override;
    void exit() override;

    bool shouldClose() const { return _closeRequested; }
    void clearClose() { _closeRequested = false; }

private:
    Project& _project;
    Character& _character;
    GlobalSettings& _settings;
    uint8_t _cursor = 0;
    bool _closeRequested = false;

    static const uint8_t NUM_ITEMS = 5;
};
