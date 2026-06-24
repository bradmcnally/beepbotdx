#include "project_view.h"
#include "platform/storage.h"
#include "platform/led.h"
#include "core/theme.h"
#include "core/timing.h"
#include "core/grid_layout.h"
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
        if (i == _currentSlot) {
            _slotTheme[i] = _project.themeIndex;
            _slotBpm[i] = _project.bpm;
        } else {
            _slotTheme[i] = _slotExists[i] ? Storage::loadProjectTheme(i) : 0;
            _slotBpm[i] = _slotExists[i] ? Storage::loadProjectBpm(i) : DEFAULT_BPM;
        }
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
        case INPUT_UP:
            if (_cursor >= 4) _cursor -= 4;
            break;
        case INPUT_DOWN:
            if (_cursor < 4) _cursor += 4;
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
                _character.setState(CHAR_FLIP);
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
            { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
            break;
        case INPUT_MINUS:
            _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
            { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
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

    GridLayout grid = GridLayout::make(4, 2, 22);

    for (uint8_t i = 0; i < 8; i++) {
        int x, y;
        grid.cellXY(i, x, y);

        uint16_t bgColor;
        uint16_t textColor;
        if (i == _cursor) {
            bgColor = TFT_WHITE;
            textColor = TFT_BLACK;
        } else if (_slotExists[i]) {
            Theme slotTheme = ThemeOps::getPreset(_slotTheme[i]);
            bgColor = theme.dim;
            textColor = slotTheme.accent;
        } else {
            bgColor = theme.dark;
            textColor = 0x7BEF;
        }
        canvas.fillRect(x, y, grid.cellW, grid.cellH, bgColor);

        canvas.setTextDatum(top_left);
        canvas.setTextColor(textColor);
        char numStr[4];
        snprintf(numStr, sizeof(numStr), "%02d", i + 1);
        canvas.drawString(numStr, x + 4, y + 4);

        if (_slotExists[i]) {
            char bpmStr[8];
            snprintf(bpmStr, sizeof(bpmStr), "%dbpm", _slotBpm[i]);
            canvas.drawString(bpmStr, x + 4, y + 14);
        } else {
            canvas.drawString("empty", x + 4, y + 14);
        }
    }

    if (_statusMsg[0] && (millis() - _statusTime < 2000)) {
        canvas.setTextColor(theme.highlight);
        canvas.setTextDatum(top_right);
        canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
    }
}

void ProjectView::exit() {}
