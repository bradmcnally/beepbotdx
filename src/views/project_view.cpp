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
      _cursor(0), _closeRequested(false), _loaded(false), _confirming(false), _deleting(false), _statusTime(0) {
    _statusMsg[0] = '\0';
    memset(_slotExists, 0, sizeof(_slotExists));
}

void ProjectView::enter() {
    _cursor = _currentSlot;
    _closeRequested = false;
    _loaded = false;
    _confirming = false;
    _deleting = false;
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
    if (_confirming) {
        switch (event) {
            case INPUT_ENTER:
                Storage::saveProject(_project, _currentSlot);
                _confirming = false;
                doSwitch();
                break;
            case INPUT_BACK:
                _confirming = false;
                doSwitch();
                break;
            case INPUT_ESC:
                _confirming = false;
                _character.setState(CHAR_IDLE);
                break;
            default:
                break;
        }
        return;
    }

    if (_deleting) {
        switch (event) {
            case INPUT_ENTER:
                Storage::deleteProject(_cursor);
                _slotExists[_cursor] = false;
                _deleting = false;
                if (_cursor == _currentSlot) {
                    Project::init(_project);
                }
                _character.setState(CHAR_SUCCESS);
                _character.say("deleted");
                break;
            case INPUT_ESC:
            case INPUT_BACK:
                _deleting = false;
                _character.setState(CHAR_IDLE);
                break;
            default:
                break;
        }
        return;
    }

    switch (event) {
        case INPUT_LEFT:
            if (_cursor % 2 > 0) _cursor--;
            else if (_cursor >= 2) _cursor -= 1;
            break;
        case INPUT_RIGHT:
            if (_cursor % 2 < 1) _cursor++;
            else if (_cursor < 6) _cursor += 1;
            break;
        case INPUT_UP:
            if (_cursor >= 2) _cursor -= 2;
            break;
        case INPUT_DOWN:
            if (_cursor < 6) _cursor += 2;
            break;
        case INPUT_ENTER:
            if (_cursor == _currentSlot) {
                _closeRequested = true;
            } else if (_project.dirty) {
                _confirming = true;
                _character.say("unsaved!");
            } else {
                doSwitch();
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
            _slotTheme[_currentSlot] = _project.themeIndex;
            _project.dirty = true;
            { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
            break;
        case INPUT_MINUS:
            _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
            _slotTheme[_currentSlot] = _project.themeIndex;
            _project.dirty = true;
            { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
            break;
        case INPUT_BACK:
            if (_slotExists[_cursor]) {
                _deleting = true;
                _character.say("delete?");
            }
            break;
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default:
            break;
    }
}

void ProjectView::doSwitch() {
    if (_slotExists[_cursor]) {
        if (Storage::loadProject(_project, _cursor)) {
            _currentSlot = _cursor;
            _character.setState(CHAR_SUCCESS);
            _character.say("loaded!");
            _loaded = true;
        } else {
            _character.setState(CHAR_ERROR);
            _character.say("oh no");
        }
    } else {
        Project::init(_project);
        _currentSlot = _cursor;
        _character.setState(CHAR_FLIP);
        _loaded = true;
    }
}

void ProjectView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);

    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    GridLayout grid = GridLayout::make(2, 4, 3);

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
            bgColor = slotTheme.accent;
            textColor = TFT_BLACK;
        } else {
            bgColor = theme.dark;
            textColor = 0x7BEF;
        }
        canvas.fillRect(x, y, grid.cellW, grid.cellH, bgColor);

        canvas.setTextDatum(top_left);
        canvas.setTextColor(textColor);
        if (i == _currentSlot) {
            char numStr[8];
            snprintf(numStr, sizeof(numStr), ">%02d", i + 1);
            canvas.drawString(numStr, x + 2, y + 4);
        } else {
            char numStr[4];
            snprintf(numStr, sizeof(numStr), "%02d", i + 1);
            canvas.drawString(numStr, x + 4, y + 4);
        }

        if (_slotExists[i]) {
            char bpmStr[8];
            snprintf(bpmStr, sizeof(bpmStr), "%dbpm", _slotBpm[i]);
            canvas.drawString(bpmStr, x + 4, y + 14);
        } else {
            canvas.drawString("empty", x + 4, y + 14);
        }
    }

    if (_confirming) {
        const int boxW = 140;
        const int boxH = 40;
        const int boxX = (SCREEN_WIDTH - boxW) / 2;
        const int boxY = (SCREEN_HEIGHT - boxH) / 2;
        canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
        canvas.drawRect(boxX, boxY, boxW, boxH, theme.accent);

        canvas.setTextColor(TFT_WHITE);
        canvas.setTextDatum(top_center);
        canvas.drawString("Unsaved changes!", boxX + boxW / 2, boxY + 6);

        canvas.setTextColor(theme.accent);
        canvas.drawString("OK:save  DEL:discard", boxX + boxW / 2, boxY + 22);
    }

    if (_deleting) {
        const int boxW = 150;
        const int boxH = 40;
        const int boxX = (SCREEN_WIDTH - boxW) / 2;
        const int boxY = (SCREEN_HEIGHT - boxH) / 2;
        canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
        canvas.drawRect(boxX, boxY, boxW, boxH, theme.accent);

        canvas.setTextColor(TFT_WHITE);
        canvas.setTextDatum(top_center);
        char msg[24];
        snprintf(msg, sizeof(msg), "Delete project %d?", _cursor + 1);
        canvas.drawString(msg, boxX + boxW / 2, boxY + 6);

        canvas.setTextColor(theme.accent);
        canvas.drawString("OK:yes  ESC:no", boxX + boxW / 2, boxY + 22);
    }

    if (_statusMsg[0] && (millis() - _statusTime < 2000)) {
        canvas.setTextColor(theme.highlight);
        canvas.setTextDatum(top_right);
        canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
    }
}

void ProjectView::exit() {}
