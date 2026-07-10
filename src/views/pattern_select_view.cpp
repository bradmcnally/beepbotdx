#include "pattern_select_view.h"
#include "views/settings_view.h"
#include "platform/input.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstdio>
#include <cstring>

PatternSelectView::PatternSelectView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _cursor(0), _editRequested(false), _flashPattern(0xFF), _flashTime(0),
      _clipboard{}, _hasClipboard(false), _confirmingDelete(false) {}

void PatternSelectView::enter() {
    _editRequested = false;
    _character.setState(CHAR_IDLE);
}

void PatternSelectView::update(InputEvent event) {
    if (!_sequencer.isPlaying() &&
        (_character.getState() == CHAR_BEAT || _character.getState() == CHAR_DANCE_L || _character.getState() == CHAR_DANCE_R)) {
        _character.setState(CHAR_IDLE);
    }

    if (_confirmingDelete) {
        if (event == INPUT_ENTER) {
            memset(_project.patterns[_cursor].steps, 0, NUM_STEPS);
            _project.dirty = true;
            _character.setState(CHAR_SUCCESS);
            _character.say("cleared");
            _confirmingDelete = false;
        } else if (event == INPUT_ESC || event == INPUT_BACK) {
            _confirmingDelete = false;
            _character.setState(CHAR_IDLE);
        }
        return;
    }

    switch (event) {
        case INPUT_LEFT:
            if (_cursor > 0) _cursor--;
            break;
        case INPUT_RIGHT:
            if (_cursor < NUM_PATTERNS - 1) _cursor++;
            break;
        case INPUT_UP:
            if (_cursor >= 4) _cursor -= 4;
            break;
        case INPUT_DOWN:
            if (_cursor < 12) _cursor += 4;
            break;
        case INPUT_BACK: {
            bool hasSteps = false;
            for (int s = 0; s < NUM_STEPS && !hasSteps; s++) {
                if (_project.patterns[_cursor].steps[s] != 0) hasSteps = true;
            }
            if (hasSteps) {
                if (GlobalSettings::instance && GlobalSettings::instance->confirmDelete) {
                    _confirmingDelete = true;
                } else {
                    memset(_project.patterns[_cursor].steps, 0, NUM_STEPS);
                    _project.dirty = true;
                    _character.setState(CHAR_SUCCESS);
                    _character.say("cleared");
                }
            }
            break;
        }
        case INPUT_ENTER:
            _editRequested = true;
            break;
        case INPUT_SPACE:
            if (_sequencer.isPlaying()) {
                _sequencer.stop();
                _character.setState(CHAR_IDLE);
            } else {
                bool hasSteps = false;
                for (int s = 0; s < NUM_STEPS && !hasSteps; s++) {
                    if (_project.patterns[_cursor].steps[s] != 0) hasSteps = true;
                }
                if (hasSteps) {
                    _sequencer.playPattern(_cursor, true);
                    _character.setState(CHAR_IDLE);
                    _flashPattern = _cursor;
                    _flashTime = millis();
                } else {
                    _character.setState(CHAR_SUSPICIOUS);
                    _character.say("empty");
                }
            }
            break;
        case INPUT_CHAR: {
            char ch = Input::getChar();
            if (Input::isFnHeld() && ch == 'c') {
                memcpy(_clipboard.steps, _project.patterns[_cursor].steps, NUM_STEPS);
                _hasClipboard = true;
                _character.say("copied");
            } else if (Input::isFnHeld() && ch == 'v' && _hasClipboard) {
                memcpy(_project.patterns[_cursor].steps, _clipboard.steps, NUM_STEPS);
                _project.dirty = true;
                _character.say("pasted");
            }
            break;
        }
        default:
            break;
    }
}

void PatternSelectView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.setTextSize(1);

    const int margin = 3;
    const int spacing = 3;
    const int cols = 4;
    const int rows = 4;
    const int gridTop = 22;
    const int gridW = SCREEN_WIDTH - margin * 2;
    const int gridH = SCREEN_HEIGHT - gridTop - margin;
    const int cellW = (gridW - spacing * (cols - 1)) / cols;
    const int cellH = (gridH - spacing * (rows - 1)) / rows;
    const int gridLeft = margin + (gridW - cellW * cols - spacing * (cols - 1)) / 2;
    const int gridTopY = gridTop + (gridH - cellH * rows - spacing * (rows - 1)) / 2;

    for (uint8_t i = 0; i < NUM_PATTERNS; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = gridLeft + col * (cellW + spacing);
        int y = gridTopY + row * (cellH + spacing);

        bool hasSteps = false;
        for (int s = 0; s < NUM_STEPS && !hasSteps; s++) {
            if (_project.patterns[i].steps[s] != 0) hasSteps = true;
        }

        bool selected = (i == _cursor);
        bool flashing = (i == _flashPattern && (millis() - _flashTime) < 150);

        // Cell background
        uint16_t bgColor;
        uint16_t textColor;
        if (flashing) { bgColor = TFT_WHITE; textColor = TFT_BLACK; }
        else if (selected) { bgColor = theme.accent; textColor = TFT_BLACK; }
        else { bgColor = theme.dark; textColor = hasSteps ? theme.accent : theme.dim; }
        canvas.fillRect(x, y, cellW, cellH, bgColor);
        canvas.setTextColor(textColor);
        canvas.setTextDatum(top_left);

        char label[5];
        snprintf(label, sizeof(label), "%02d", i + 1);
        canvas.drawString(label, x + 4, y + 4);

        if (hasSteps) {
            Pattern& pat = _project.patterns[i];
            const int px = 2;
            int miniW = NUM_STEPS * px;
            int miniH = NUM_SOUNDS * px;
            int mx = x + cellW - miniW - 3;
            int my = y + (cellH - miniH + 2) / 2;

            bool isPlaying = _sequencer.isPlaying() && _sequencer.getCurrentPattern() == i;
            uint8_t playStep = isPlaying ? _sequencer.getCurrentStep() : 0xFF;

            for (uint8_t step = 0; step < NUM_STEPS; step++) {
                for (uint8_t snd = 0; snd < NUM_SOUNDS; snd++) {
                    if (step == playStep) {
                        uint16_t c = (pat.steps[step] & (1 << snd)) ? TFT_WHITE : (selected ? TFT_BLACK : theme.dim);
                        canvas.fillRect(mx + step * px, my + snd * px, px, px, c);
                    } else if (pat.steps[step] & (1 << snd)) {
                        canvas.fillRect(mx + step * px, my + snd * px, px, px, textColor);
                    }
                }
            }
        }
    }

    if (_confirmingDelete) {
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
        snprintf(msg, sizeof(msg), "CLEAR PATTERN %d?", _cursor + 1);
        canvas.drawString(msg, boxX + boxW / 2, boxY + 16);

        canvas.setTextColor(theme.accent);
        canvas.drawString("YES:OK  NO:ESC", boxX + boxW / 2, boxY + 36);
    }
}

void PatternSelectView::exit() {}
