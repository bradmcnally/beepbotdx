#include "project_view.h"
#include "platform/storage.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstring>

ProjectView::ProjectView(Project& project, Character& character, uint8_t& currentSlot)
    : _project(project), _character(character), _currentSlot(currentSlot),
      _cursor(0), _closeRequested(false), _loaded(false), _statusTime(0) {
    _statusMsg[0] = '\0';
    memset(_slotExists, 0, sizeof(_slotExists));
}

void ProjectView::enter() {
    _cursor = _currentSlot;
    _closeRequested = false;
    _loaded = false;
    _statusMsg[0] = '\0';
    _character.setState(CHAR_IDLE);

    for (uint8_t i = 0; i < 8; i++) {
        _slotExists[i] = Storage::projectExists(i);
    }
}

void ProjectView::update(InputEvent event) {
    switch (event) {
        case INPUT_LEFT:
            if (_cursor > 0) _cursor--;
            break;
        case INPUT_RIGHT:
            if (_cursor < 7) _cursor++;
            break;
        case INPUT_ENTER:
            if (_cursor == _currentSlot) {
                // Already on this slot — just close
                _closeRequested = true;
            } else if (_slotExists[_cursor]) {
                if (Storage::loadProject(_project, _cursor)) {
                    _currentSlot = _cursor;
                    snprintf(_statusMsg, sizeof(_statusMsg), "LOADED %d", _cursor + 1);
                    _character.setState(CHAR_SUCCESS);
                    _character.say("loaded!");
                    _loaded = true;
                } else {
                    snprintf(_statusMsg, sizeof(_statusMsg), "LOAD FAIL");
                    _character.setState(CHAR_ERROR);
                    _character.say("oh no");
                }
                _statusTime = millis();
            } else {
                // Switch to empty slot (new project)
                Project::init(_project);
                _currentSlot = _cursor;
                snprintf(_statusMsg, sizeof(_statusMsg), "NEW %d", _cursor + 1);
                _character.setState(CHAR_SUCCESS);
                _character.say("fresh!");
                _loaded = true;
                _statusTime = millis();
            }
            break;
        case INPUT_SPACE: {
            _character.setState(CHAR_SAVING);
            _character.say("rendering...");
            char path[64];
            snprintf(path, sizeof(path), "/beepbotdx/export_%d.wav", _currentSlot + 1);
            if (Storage::renderSongToWav(_project, path)) {
                snprintf(_statusMsg, sizeof(_statusMsg), "EXPORTED");
                _character.setState(CHAR_SUCCESS);
                _character.say("exported!");
            } else {
                snprintf(_statusMsg, sizeof(_statusMsg), "EXPORT FAIL");
                _character.setState(CHAR_ERROR);
                _character.say("oh no");
            }
            _statusTime = millis();
            break;
        }
        case INPUT_PLUS:
            _project.themeIndex = (_project.themeIndex + 1) % ThemeOps::NUM_PRESETS;
            break;
        case INPUT_MINUS:
            _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
            break;
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default:
            break;
    }
}

void ProjectView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);

    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    // Current slot indicator
    char slotStr[16];
    snprintf(slotStr, sizeof(slotStr), "SLOT %d", _currentSlot + 1);
    canvas.setTextColor(theme.dark);
    canvas.drawString(slotStr, 4, 22);

    const int gridX = 12;
    const int gridY = 50;
    const int cellW = 28;

    for (uint8_t i = 0; i < 8; i++) {
        int x = gridX + i * cellW;

        if (i == _cursor) {
            canvas.fillRect(x - 1, gridY - 1, 22, 18, theme.accent);
            canvas.setTextColor(TFT_BLACK);
        } else if (i == _currentSlot) {
            canvas.setTextColor(theme.highlight);
        } else if (_slotExists[i]) {
            canvas.setTextColor(theme.accent);
        } else {
            canvas.setTextColor(theme.dim);
        }

        char label[2];
        label[0] = '1' + i;
        label[1] = '\0';
        canvas.drawString(label, x + 5, gridY + 3);
    }

    // Theme selector
    int themeY = gridY + 26;
    canvas.setTextColor(theme.dark);
    canvas.drawString("COLOR", 12, themeY);
    canvas.fillRect(54, themeY - 1, 14, 10, theme.accent);
    canvas.setTextColor(theme.accent);
    canvas.drawString(ThemeOps::getPresetName(_project.themeIndex), 72, themeY);
    canvas.setTextColor(theme.dark);
    canvas.drawString("+/-", 120, themeY);

    // Help
    canvas.setTextColor(theme.dim);
    canvas.drawString("ENTER=load  SPACE=export", 12, themeY + 16);

    if (_statusMsg[0] && (millis() - _statusTime < 2000)) {
        canvas.setTextColor(theme.highlight);
        canvas.drawString(_statusMsg, 12, themeY + 30);
    }
}

void ProjectView::exit() {}
