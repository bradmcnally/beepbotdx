#include "pattern_select_view.h"
#include "platform/input.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstdio>
#include <cstring>

PatternSelectView::PatternSelectView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _cursor(0), _editRequested(false), _flashPattern(0xFF), _flashTime(0),
      _clipboard{}, _hasClipboard(false) {}

void PatternSelectView::enter() {
    _editRequested = false;
    _character.setState(CHAR_IDLE);
}

void PatternSelectView::update(InputEvent event) {
    if (!_sequencer.isPlaying() &&
        (_character.getState() == CHAR_PLAYING || _character.getState() == CHAR_BEAT)) {
        _character.setState(CHAR_IDLE);
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
                memset(_project.patterns[_cursor].steps, 0, NUM_STEPS);
                _character.setState(CHAR_SUCCESS);
                _character.say("cleared");
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
                    _character.setState(CHAR_PLAYING);
                    _flashPattern = _cursor;
                    _flashTime = millis();
                } else {
                    _character.setState(CHAR_ERROR);
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
        if (flashing) bgColor = theme.dim;
        else if (selected) bgColor = TFT_WHITE;
        else if (hasSteps) bgColor = theme.accent;
        else bgColor = theme.dark;
        canvas.fillRect(x, y, cellW, cellH, bgColor);

        const uint16_t gray50 = 0x7BEF;
        uint16_t textColor;
        if (selected) textColor = TFT_BLACK;
        else if (hasSteps) textColor = TFT_BLACK;
        else textColor = gray50;
        canvas.setTextColor(textColor);
        canvas.setTextDatum(top_left);

        char label[5];
        snprintf(label, sizeof(label), "%02d", i + 1);
        canvas.drawString(label, x + 4, y + 4);
    }
}

void PatternSelectView::exit() {}
