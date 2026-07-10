#include "project_view.h"
#include "platform/storage.h"
#include "platform/input.h"
#include "platform/led.h"
#include "core/theme.h"
#include "core/timing.h"
#include "core/grid_layout.h"
#include "config.h"
#include <cstring>

ProjectView::ProjectView(Project& project, Character& character, uint8_t& currentSlot)
    : _project(project), _character(character), _currentSlot(currentSlot),
      _cursor(0), _closeRequested(false), _loaded(false), _confirming(false), _deleting(false), _statusTime(0),
      _renaming(false), _renameLen(0) {
    _statusMsg[0] = '\0';
    memset(_slotExists, 0, sizeof(_slotExists));
    memset(_slotName, 0, sizeof(_slotName));
}

void ProjectView::enter() {
    _cursor = _currentSlot;
    _closeRequested = false;
    _loaded = false;
    _confirming = false;
    _deleting = false;
    _statusMsg[0] = '\0';
    _character.setState(CHAR_IDLE);

    _renaming = false;
    for (uint8_t i = 0; i < 8; i++) {
        _slotExists[i] = Storage::projectExists(i);
        if (i == _currentSlot) {
            _slotTheme[i] = _project.themeIndex;
            _slotBpm[i] = _project.bpm;
            strncpy(_slotName[i], _project.name, 8);
            _slotName[i][8] = '\0';
        } else {
            _slotTheme[i] = _slotExists[i] ? Storage::loadProjectTheme(i) : 0;
            _slotBpm[i] = _slotExists[i] ? Storage::loadProjectBpm(i) : DEFAULT_BPM;
            if (_slotExists[i]) {
                Storage::loadProjectName(i, _slotName[i], 9);
            } else {
                _slotName[i][0] = '\0';
            }
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
                _character.setState(CHAR_CRYING);
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

    if (_renaming) {
        switch (event) {
            case INPUT_ENTER:
                if (_renameLen > 0) {
                    strncpy(_slotName[_cursor], _renameBuffer, 8);
                    _slotName[_cursor][8] = '\0';
                    if (_cursor == _currentSlot) {
                        strncpy(_project.name, _renameBuffer, 8);
                        _project.name[8] = '\0';
                        _project.dirty = true;
                    } else {
                        Storage::saveProjectName(_cursor, _renameBuffer);
                    }
                    _character.setState(CHAR_SUCCESS);
                    _character.say("renamed!");
                }
                _renaming = false;
                break;
            case INPUT_ESC:
                _renaming = false;
                _character.setState(CHAR_IDLE);
                break;
            case INPUT_BACK:
                if (_renameLen > 0) _renameLen--;
                _renameBuffer[_renameLen] = '\0';
                break;
            case INPUT_CHAR: {
                char ch = Input::getChar();
                if (_renameLen < 8 && ch >= ' ' && ch <= '~') {
                    _renameBuffer[_renameLen++] = ch;
                    _renameBuffer[_renameLen] = '\0';
                }
                break;
            }
            case INPUT_NUM1: case INPUT_NUM2: case INPUT_NUM3:
            case INPUT_NUM4: case INPUT_NUM5: case INPUT_NUM6:
            case INPUT_NUM7: case INPUT_NUM8: case INPUT_NUM9:
                if (_renameLen < 8) {
                    _renameBuffer[_renameLen++] = '1' + (event - INPUT_NUM1);
                    _renameBuffer[_renameLen] = '\0';
                }
                break;
            case INPUT_NUM0:
                if (_renameLen < 8) {
                    _renameBuffer[_renameLen++] = '0';
                    _renameBuffer[_renameLen] = '\0';
                }
                break;
            default:
                break;
        }
        return;
    }

    switch (event) {
        case INPUT_LEFT:
            if (_cursor % 2 > 0) _cursor--;
            break;
        case INPUT_RIGHT:
            if (_cursor % 2 < 1) _cursor++;
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
                _character.setState(CHAR_SUSPICIOUS);
                _character.say("unsaved!");
            } else {
                doSwitch();
            }
            break;
        case INPUT_SPACE:
            break;
        case INPUT_PLUS:
            if (_cursor == _currentSlot) {
                _project.themeIndex = (_project.themeIndex + 1) % ThemeOps::NUM_PRESETS;
                _slotTheme[_currentSlot] = _project.themeIndex;
                _project.dirty = true;
                Storage::saveProjectTheme(_currentSlot, _project.themeIndex);
                { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
            } else if (_slotExists[_cursor]) {
                _slotTheme[_cursor] = (_slotTheme[_cursor] + 1) % ThemeOps::NUM_PRESETS;
                Storage::saveProjectTheme(_cursor, _slotTheme[_cursor]);
            }
            break;
        case INPUT_MINUS:
            if (_cursor == _currentSlot) {
                _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
                _slotTheme[_currentSlot] = _project.themeIndex;
                _project.dirty = true;
                Storage::saveProjectTheme(_currentSlot, _project.themeIndex);
                { uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b); }
            } else if (_slotExists[_cursor]) {
                _slotTheme[_cursor] = (_slotTheme[_cursor] + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
                Storage::saveProjectTheme(_cursor, _slotTheme[_cursor]);
            }
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
        case INPUT_CHAR: {
            char ch = Input::getChar();
            if (ch == 'r' && _slotExists[_cursor]) {
                strncpy(_renameBuffer, _slotName[_cursor], 8);
                _renameBuffer[8] = '\0';
                _renameLen = strlen(_renameBuffer);
                _renaming = true;
                _character.setState(CHAR_FOCUSED);
            } else if (ch == 'e') {
                _character.setState(CHAR_SAVING);
                _character.say("rendering...");
                char path[64];
                if (_project.name[0]) {
                    snprintf(path, sizeof(path), "/beepbotdx/%s.wav", _project.name);
                } else {
                    snprintf(path, sizeof(path), "/beepbotdx/export_%d.wav", _currentSlot + 1);
                }
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
            }
            break;
        }
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

    if (_renaming) {
        Theme slotTheme = ThemeOps::getPreset(_slotTheme[_cursor]);
        const int margin = 3;
        const int hdrGridW = SCREEN_WIDTH - margin * 2;
        const int hdrCellW = (hdrGridW - margin * 3) / 4;
        const int hdrContentW = hdrCellW * 4 + margin * 3;
        const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
        const int infoY = 23;

        canvas.setTextSize(1);
        canvas.setTextColor(slotTheme.accent);
        canvas.setTextDatum(top_left);
        canvas.drawString("RENAME", hdrLeft + 4, infoY);

        canvas.setTextColor(slotTheme.dim);
        canvas.setTextDatum(top_right);
        canvas.drawString("8 CHAR", SCREEN_WIDTH - margin - 4, infoY);
        canvas.setTextDatum(top_left);

        char display[12];
        bool showCursor = (millis() / 400) % 2 == 0;
        if (showCursor) {
            snprintf(display, sizeof(display), "%s_", _renameBuffer);
        } else {
            snprintf(display, sizeof(display), "%s ", _renameBuffer);
        }
        canvas.setTextColor(TFT_WHITE);
        canvas.setTextDatum(top_left);
        canvas.setTextSize(2);
        int nameY = infoY + 12 + (SCREEN_HEIGHT - 14 - (infoY + 12)) / 2 - 14;
        canvas.drawString(display, hdrLeft + 4, nameY);
        canvas.setTextSize(1);

        canvas.setTextColor(slotTheme.accent);
        canvas.setTextDatum(top_center);
        canvas.drawString("CONFIRM:OK  CANCEL:ESC", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 14);
        canvas.setTextDatum(top_left);
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    GridLayout grid = GridLayout::make(2, 4, 22);

    for (uint8_t i = 0; i < 8; i++) {
        int x, y;
        grid.cellXY(i, x, y);

        uint16_t bgColor;
        uint16_t textColor;
        if (i == _cursor) {
            if (_slotExists[i]) {
                Theme slotTheme = ThemeOps::getPreset(_slotTheme[i]);
                bgColor = slotTheme.accent;
                textColor = TFT_BLACK;
            } else {
                bgColor = theme.accent;
                textColor = TFT_BLACK;
            }
        } else if (_slotExists[i]) {
            Theme slotTheme = ThemeOps::getPreset(_slotTheme[i]);
            bgColor = slotTheme.dark;
            textColor = slotTheme.accent;
        } else {
            bgColor = ThemeOps::rgb565(15, 15, 15);
            textColor = ThemeOps::rgb565(72, 72, 72);
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
            if (_slotName[i][0]) {
                canvas.drawString(_slotName[i], x + 4, y + 14);
            }
            char bpmStr[8];
            snprintf(bpmStr, sizeof(bpmStr), "%d", _slotBpm[i]);
            canvas.setTextDatum(top_right);
            canvas.drawString(bpmStr, x + grid.cellW - 3, y + 4);
            canvas.setTextDatum(top_left);
        }
    }

    if (_confirming) {
        canvas.darken();

        const int boxW = 180;
        const int boxH = 76;
        const int boxX = (SCREEN_WIDTH - boxW) / 2;
        const int boxY = (SCREEN_HEIGHT - boxH) / 2;
        canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
        canvas.drawRect(boxX, boxY, boxW, boxH, theme.accent);

        canvas.setTextColor(TFT_WHITE);
        canvas.setTextDatum(top_center);
        canvas.drawString("UNSAVED CHANGES", boxX + boxW / 2, boxY + 16);

        canvas.setTextColor(theme.accent);
        canvas.drawString("SAVE:OK  CANCEL:ESC", boxX + boxW / 2, boxY + 38);
        canvas.drawString("DISCARD:DEL", boxX + boxW / 2, boxY + 54);
    }

    if (_deleting) {
        canvas.darken();

        const int boxW = 150;
        const int boxH = 60;
        const int boxX = (SCREEN_WIDTH - boxW) / 2;
        const int boxY = (SCREEN_HEIGHT - boxH) / 2;
        canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
        canvas.drawRect(boxX, boxY, boxW, boxH, theme.accent);

        canvas.setTextColor(TFT_WHITE);
        canvas.setTextDatum(top_center);
        char msg[24];
        snprintf(msg, sizeof(msg), "DELETE PROJECT %d?", _cursor + 1);
        canvas.drawString(msg, boxX + boxW / 2, boxY + 16);

        canvas.setTextColor(theme.accent);
        canvas.drawString("YES:OK  NO:ESC", boxX + boxW / 2, boxY + 36);
    }


    if (_statusMsg[0] && (millis() - _statusTime < 2000)) {
        canvas.setTextColor(theme.highlight);
        canvas.setTextDatum(top_right);
        canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
    }
}

void ProjectView::exit() {}
