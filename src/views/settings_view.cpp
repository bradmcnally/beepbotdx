#include "settings_view.h"
#include "core/theme.h"
#include "config.h"

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
                const char* names[] = {"on", "beat", "off"};
                _character.say(names[_settings.ledMode]);
            } else if (_cursor == 2) {
                _settings.confirmDelete = !_settings.confirmDelete;
                _character.say(_settings.confirmDelete ? "on" : "off");
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
    const int labelX = 20;
    const int valueX = 140;

    const char* labels[] = {"AUTO-SAVE", "LED MODE", "WARNINGS"};
    const char* values[NUM_ITEMS];

    values[0] = _settings.autoSave ? "ON" : "OFF";
    static const char* ledNames[] = {"ON", "METRONOME", "OFF"};
    values[1] = ledNames[_settings.ledMode];
    values[2] = _settings.confirmDelete ? "ON" : "OFF";

    for (uint8_t i = 0; i < NUM_ITEMS; i++) {
        int y = startY + i * itemH;

        if (i == _cursor) {
            canvas.setTextColor(TFT_WHITE);
            canvas.setTextDatum(top_left);
            canvas.drawString(">", labelX - 10, y);
        }

        canvas.setTextColor(i == _cursor ? TFT_WHITE : theme.accent);
        canvas.setTextDatum(top_left);
        canvas.drawString(labels[i], labelX, y);

        canvas.setTextColor(i == _cursor ? theme.accent : theme.dim);
        canvas.setTextDatum(top_left);
        canvas.drawString(values[i], valueX, y);
    }
}

void SettingsView::exit() {}
