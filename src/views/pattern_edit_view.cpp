#include "pattern_edit_view.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/power.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstdio>

PatternEditView::PatternEditView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _patternIndex(0), _cursorX(0), _cursorY(0), _backRequested(false),
      _flashSound(0xFF), _flashTime(0) {}

void PatternEditView::enter() {
    _cursorX = 0;
    _cursorY = 0;
    _backRequested = false;
    _character.setState(CHAR_FOCUSED);
}

void PatternEditView::update(InputEvent event) {
    Pattern& pat = _project.patterns[_patternIndex];

    switch (event) {
        case INPUT_LEFT:
            if (_cursorX > 0) _cursorX--;
            break;
        case INPUT_RIGHT:
            if (_cursorX < NUM_STEPS - 1) _cursorX++;
            break;
        case INPUT_UP:
            if (_cursorY > 0) _cursorY--;
            break;
        case INPUT_DOWN:
            if (_cursorY < NUM_SOUNDS - 1) _cursorY++;
            break;
        case INPUT_ENTER:
            toggleStep(_cursorY);
            break;
        case INPUT_ESC:
            _backRequested = true;
            break;
        case INPUT_SPACE:
            if (_sequencer.isPlaying()) {
                _sequencer.stop();
                _character.setState(CHAR_FOCUSED);
            } else {
                _sequencer.playPattern(_patternIndex);
                _character.setState(CHAR_IDLE);
            }
            break;
        case INPUT_NUM1: handleNumber(0); break;
        case INPUT_NUM2: handleNumber(1); break;
        case INPUT_NUM3: handleNumber(2); break;
        case INPUT_NUM4: handleNumber(3); break;
        case INPUT_NUM5: handleNumber(4); break;
        case INPUT_NUM6: handleNumber(5); break;
        case INPUT_NUM7: handleNumber(6); break;
        case INPUT_NUM8: handleNumber(7); break;
        default: break;
    }
}

void PatternEditView::toggleStep(uint8_t sound) {
    Pattern& pat = _project.patterns[_patternIndex];
    bool wasSet = pat.steps[_cursorX] & (1 << sound);
    pat.steps[_cursorX] ^= (1 << sound);
    if (!wasSet) _character.setState(CHAR_BEAT);
    _project.dirty = true;
}

void PatternEditView::toggleStepAt(uint8_t step) {
    if (step >= NUM_STEPS) return;
    Pattern& pat = _project.patterns[_patternIndex];
    pat.steps[step] ^= (1 << _cursorY);
    _project.dirty = true;
}

void PatternEditView::handleNumber(uint8_t num) {
    if (_sequencer.isPlaying() && Input::isFnHeld()) {
        Pattern& pat = _project.patterns[_patternIndex];
        uint8_t step = _sequencer.nearestStep(millis());
        pat.steps[step] |= (1 << num);
        _project.dirty = true;
        _character.setState(CHAR_BEAT);
    }
    triggerSound(num);
}

void PatternEditView::triggerSound(uint8_t sound) {
    if (sound >= NUM_SOUNDS) return;
    SoundSlot& slot = _project.sounds[sound];
    if (!slot.occupied) {
        _character.setState(CHAR_ERROR);
        _character.say("empty");
        _flashSound = sound;
        _flashTime = millis();
        return;
    }
    Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100);
    _character.setState(CHAR_BEAT);
    _character.say(slot.name);
    _flashSound = sound;
    _flashTime = millis();
}

void PatternEditView::draw(Canvas& canvas) {
    Pattern& pat = _project.patterns[_patternIndex];

    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    uint16_t NOTE_COLOR = theme.accent;
    uint16_t NOTE_DIM = theme.dim;

    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    const int margin = 3;
    const int infoY = 23;
    const int gridTop = infoY + 10;
    const int gridBottom = SCREEN_HEIGHT - 3;
    const int gridH = gridBottom - gridTop;

    // Align grid to header text positions
    const int hdrGridW = SCREEN_WIDTH - margin * 2;
    const int hdrCellW = (hdrGridW - margin * 3) / 4;
    const int hdrContentW = hdrCellW * 4 + margin * 3;
    const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
    const int gridLeft = hdrLeft + 4;
    const int gridRight = hdrLeft + hdrContentW - 4;
    const int gridW = gridRight - gridLeft;

    const int cellGap = 2;
    const int cellW = (gridW - (NUM_STEPS - 1) * cellGap) / NUM_STEPS;
    const int cellH = (gridH - (NUM_SOUNDS - 1) * cellGap) / NUM_SOUNDS;
    const int actualW = cellW * NUM_STEPS + (NUM_STEPS - 1) * cellGap;
    const int actualH = cellH * NUM_SOUNDS + (NUM_SOUNDS - 1) * cellGap;
    const int gridLeftAdj = gridLeft + (gridW - actualW) / 2;
    const int gridTopY = gridTop + (gridH - actualH) / 2;

    // Step grid
    for (uint8_t x = 0; x < NUM_STEPS; x++) {
        int px = gridLeftAdj + x * (cellW + cellGap);
        for (uint8_t y = 0; y < NUM_SOUNDS; y++) {
            int py = gridTopY + y * (cellH + cellGap);

            bool active = (pat.steps[x] & (1 << y)) != 0;
            bool isCursor = (x == _cursorX && y == _cursorY);
            bool isPlayhead = _sequencer.isPlaying() &&
                              _sequencer.getCurrentStep() == x;

            if (active) {
                uint16_t color = isPlayhead ? TFT_WHITE : NOTE_COLOR;
                canvas.fillRect(px + 1, py + 1, cellW - 2, cellH - 2, color);
            } else if (isPlayhead) {
                int dotW = cellW / 3;
                int dotH = cellH / 3;
                if (dotW < 2) dotW = 2;
                if (dotH < 2) dotH = 2;
                canvas.fillRect(px + (cellW - dotW) / 2, py + (cellH - dotH) / 2, dotW, dotH, NOTE_COLOR);
            } else {
                uint16_t bg = (x % 4 == 0) ? theme.measure : theme.dark;
                canvas.fillRect(px + 1, py + 1, cellW - 2, cellH - 2, bg);
            }

            if (isCursor) {
                canvas.drawRect(px, py, cellW, cellH, TFT_WHITE);
            }
        }
    }

    // Trigger indicator
    if (_flashSound < NUM_SOUNDS && (millis() - _flashTime) < 1200) {
        int py = gridTopY + _flashSound * (cellH + cellGap) + cellH / 2 - 3;
        uint16_t arrowColor = _project.sounds[_flashSound].occupied ? theme.accent : theme.dim;
        canvas.setTextColor(arrowColor);
        canvas.setTextDatum(top_right);
        canvas.drawString(">", gridLeftAdj, py);
    }

    // Info above grid: sound name (left), step number (right)
    canvas.setTextDatum(top_left);
    if (_project.sounds[_cursorY].occupied) {
        canvas.setTextColor(theme.accent);
        canvas.drawString(_project.sounds[_cursorY].name, hdrLeft + 4, infoY);
    } else {
        canvas.setTextColor(theme.dim);
        char sndLabel[8];
        snprintf(sndLabel, sizeof(sndLabel), "SND %02d", _cursorY + 1);
        canvas.drawString(sndLabel, hdrLeft + 4, infoY);
    }

    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_center);
    char stepLabel[8];
    snprintf(stepLabel, sizeof(stepLabel), "%02d", _cursorX + 1);
    canvas.drawString(stepLabel, hdrLeft + hdrContentW / 2, infoY);

    canvas.setTextDatum(top_right);
    char bpmLabel[10];
    snprintf(bpmLabel, sizeof(bpmLabel), "BPM:%d", _project.bpm);
    canvas.drawString(bpmLabel, hdrLeft + hdrContentW - 4, infoY);
}

void PatternEditView::exit() {
    if (_sequencer.isPlaying()) {
        _sequencer.stop();
    }
}
