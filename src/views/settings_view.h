#pragma once

#include "view.h"
#include "core/project.h"
#include "core/character.h"

enum LedMode : uint8_t {
    LED_ON = 0,
    LED_BEAT = 1,
    LED_OFF = 2,
};

struct GlobalSettings {
    bool autoSave = false;
    LedMode ledMode = LED_ON;
    bool confirmDelete = true;

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

    static const uint8_t NUM_ITEMS = 3;
};
