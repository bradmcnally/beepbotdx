#include "settings_view.h"
#include "core/theme.h"
#include "platform/led.h"
#include "config.h"
#include <cstdio>

GlobalSettings* GlobalSettings::instance = nullptr;

SettingsView::SettingsView(Project& project, Character& character, GlobalSettings& settings)
    : _project(project), _character(character), _settings(settings),
      _cursor(0), _closeRequested(false) {}

void SettingsView::enter() {
    _cursor = 0;
    _closeRequested = false;
    _character.setState(CHAR_IDLE);
}

void SettingsView::update(InputEvent event) {
    switch (event) {
        case INPUT_UP:
            if (_cursor > 0) _cursor--;
            break;
        case INPUT_DOWN:
            if (_cursor < NUM_ITEMS - 1) _cursor++;
            break;
        case INPUT_LEFT:
        case INPUT_RIGHT:
        case INPUT_ENTER:
            if (_cursor == 0) {
                _settings.autoSave = !_settings.autoSave;
                _character.say(_settings.autoSave ? "on" : "off");
            } else if (_cursor == 1) {
                _settings.ledMode = nextLedMode(_settings.ledMode, event == INPUT_LEFT);
                if (!ledShowsIdleColor(_settings.ledMode)) {
                    LED::off();
                } else {
                    uint8_t r, g, b;
                    ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
                    LED::setColor(r, g, b);
                }
#ifdef CARDPUTER_ZERO
                const char* names[] = {"on", "on", "off"};
#else
                const char* names[] = {"on", "beat", "off"};
#endif
                _character.say(names[_settings.ledMode]);
            } else if (_cursor == 2) {
                _settings.confirmDelete = !_settings.confirmDelete;
                _character.say(_settings.confirmDelete ? "on" : "off");
            } else if (_cursor == 3) {
                _settings.bootToProject = !_settings.bootToProject;
                _character.say(_settings.bootToProject ? "projects" : "last");
            } else if (_cursor == 4) {
                _settings.shakeGen = !_settings.shakeGen;
                _character.say(_settings.shakeGen ? "on" : "off");
            }
            break;
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default:
            break;
    }
}

void SettingsView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);

    canvas.setTextSize(1);

    const int itemH = 16;
    const int startY = 24;
    const int labelX = 7;
    const int valueX = 140;

    const char* labels[] = {"AUTO-SAVE",
#ifdef CARDPUTER_ZERO
        "METRONOME",
#else
        "LED MODE",
#endif
        "CONFIRM DEL", "BOOT", "SHAKE GEN"};
    const char* values[NUM_ITEMS];

    values[0] = _settings.autoSave ? "ON" : "OFF";
#ifdef CARDPUTER_ZERO
    static const char* ledNames[] = {"ON", "ON", "OFF"};
#else
    static const char* ledNames[] = {"ON", "METRONOME", "OFF"};
#endif
    values[1] = ledNames[_settings.ledMode];
    values[2] = _settings.confirmDelete ? "ON" : "OFF";
    values[3] = _settings.bootToProject ? "PROJ LIST" : "LAST PROJ";
    values[4] = _settings.shakeGen ? "ON" : "OFF";

    for (uint8_t i = 0; i < NUM_ITEMS; i++) {
        int y = startY + i * itemH;

        canvas.setTextColor(i == _cursor ? TFT_WHITE : theme.accent);
        canvas.setTextDatum(top_left);
        canvas.drawString(labels[i], labelX, y);

        canvas.setTextColor(i == _cursor ? TFT_WHITE : theme.dim);
        canvas.setTextDatum(top_left);
        if (i == _cursor) {
            char buf[20];
            snprintf(buf, sizeof(buf), "< %s >", values[i]);
            canvas.drawString(buf, valueX - 12, y);
        } else {
            canvas.drawString(values[i], valueX, y);
        }
    }
}

void SettingsView::exit() {}
