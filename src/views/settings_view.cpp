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
                if (event == INPUT_LEFT) {
                    _settings.ledMode = (LedMode)((_settings.ledMode + 2) % 3);
                } else {
                    _settings.ledMode = (LedMode)((_settings.ledMode + 1) % 3);
                }
                if (_settings.ledMode == LED_OFF) {
                    LED::off();
                } else {
                    uint8_t r, g, b;
                    ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
                    LED::setColor(r, g, b);
                }
                const char* names[] = {"on", "beat", "off"};
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

    const char* labels[] = {"AUTO-SAVE", "LED MODE", "CONFIRM DEL", "BOOT", "SHAKE GEN"};
    const char* values[NUM_ITEMS];

    values[0] = _settings.autoSave ? "ON" : "OFF";
    static const char* ledNames[] = {"ON", "METRONOME", "OFF"};
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
