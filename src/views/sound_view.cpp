#include "sound_view.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstring>

SoundView::SoundView(Project& project, Character& character)
    : _project(project)
    , _character(character)
    , _cursor(0)
    , _subState(STATE_LIST)
    , _wavFileCount(0)
    , _fileCursor(0)
    , _statusTime(0)
    , _trimStart(0)
    , _trimEnd(0)
    , _trimMovingEnd(false) {
    _statusMsg[0] = '\0';
    SoundSlotOps::init(_previewSlot);
    _flashSlot = 0xFF;
    _flashTime = 0;
    _playbackActive = false;
    _playbackStart = 0;
    _playbackLength = 0;
    _playbackRate = SAMPLE_RATE;
}

void SoundView::enter() {
    _subState = STATE_LIST;
    _character.setState(CHAR_IDLE);
}

void SoundView::update(InputEvent event) {
    // Number keys always trigger sounds regardless of substate
    if (_subState != STATE_TRIM && event >= INPUT_NUM1 && event <= INPUT_NUM8) {
        uint8_t idx = event - INPUT_NUM1;
        triggerSlot(idx);
        return;
    }

    switch (_subState) {
        case STATE_LIST:
            switch (event) {
                case INPUT_UP:
                    if (_cursor >= 4) _cursor -= 4;
                    break;
                case INPUT_DOWN:
                    if (_cursor < 4) _cursor += 4;
                    break;
                case INPUT_LEFT:
                    if (_cursor > 0) _cursor--;
                    break;
                case INPUT_RIGHT:
                    if (_cursor < NUM_SOUNDS - 1) _cursor++;
                    break;
                case INPUT_ENTER:
                    if (_project.sounds[_cursor].occupied) {
                        SoundSlot& slot = _project.sounds[_cursor];
                        _trimStart = 0;
                        _trimEnd = slot.length;
                        _trimMovingEnd = false;
                        _subState = STATE_TRIM;
                        _character.setState(CHAR_FOCUSED);
                    } else {
                        _subState = STATE_RECORD_READY;
                        _character.setState(CHAR_IDLE);
                        _character.say("ready");
                    }
                    break;
                case INPUT_SPACE:
                    triggerSlot(_cursor);
                    break;
                case INPUT_PLUS:
                    _subState = STATE_RECORD_READY;
                    _character.setState(CHAR_IDLE);
                    _character.say("ready");
                    break;
                case INPUT_BACK:
                    if (_project.sounds[_cursor].occupied) {
                        SoundSlotOps::free(_project.sounds[_cursor]);
                        _character.setState(CHAR_SUCCESS);
                        _character.say("cleared");
                    }
                    break;
                case INPUT_CHAR: {
                    char ch = Input::getChar();
                    if (ch == 'i') {
                        _fileCursor = 0;
                        if (!Storage::isReady()) {
                            _character.setState(CHAR_ERROR);
                        } else if (Storage::listWavFiles("/beepbotdx/samples", _wavFiles, _wavFileCount, 64) && _wavFileCount > 0) {
                            _subState = STATE_LOAD_BROWSER;
                            _character.setState(CHAR_FOCUSED);
                        } else {
                            _character.setState(CHAR_ERROR);
                        }
                    } else if (ch == 'r' && _project.sounds[_cursor].occupied) {
                        strncpy(_renameBuffer, _project.sounds[_cursor].name, 8);
                        _renameBuffer[8] = '\0';
                        _renameLen = strlen(_renameBuffer);
                        _subState = STATE_RENAME;
                        _character.setState(CHAR_FOCUSED);
                    }
                    break;
                }
                default:
                    break;
            }
            break;

        case STATE_RECORD_READY:
            switch (event) {
                case INPUT_ENTER:
                case INPUT_SPACE:
                    startRecording();
                    break;
                case INPUT_ESC:
                    _subState = STATE_LIST;
                    _character.setState(CHAR_IDLE);
                    break;
                default:
                    break;
            }
            break;

        case STATE_RECORDING:
            if (event == INPUT_ENTER || Audio::getRecordedLength() >= MAX_SAMPLE_LENGTH) {
                stopRecording();
            }
            break;

        case STATE_TRIM: {
            SoundSlot& slot = _project.sounds[_cursor];
            uint32_t step = slot.length / 64;
            if (step < 1) step = 1;

            switch (event) {
                case INPUT_LEFT:
                    if (_trimMovingEnd) {
                        if (_trimEnd > _trimStart + step) _trimEnd -= step;
                    } else {
                        if (_trimStart >= step) _trimStart -= step;
                        else _trimStart = 0;
                    }
                    break;
                case INPUT_RIGHT:
                    if (_trimMovingEnd) {
                        if (_trimEnd + step <= slot.length) _trimEnd += step;
                        else _trimEnd = slot.length;
                    } else {
                        if (_trimStart + step < _trimEnd) _trimStart += step;
                    }
                    break;
                case INPUT_UP:
                case INPUT_DOWN:
                    _trimMovingEnd = !_trimMovingEnd;
                    break;
                case INPUT_SPACE:
                    // Preview trimmed region at slot volume
                    Audio::triggerSound(slot.samples + _trimStart, _trimEnd - _trimStart, slot.sampleRate, slot.level * 255 / 100);
                    _playbackStart = millis();
                    _playbackLength = _trimEnd - _trimStart;
                    _playbackRate = slot.sampleRate;
                    _playbackActive = true;
                    _character.setState(CHAR_BEAT);
                    break;
                case INPUT_PLUS:
                    if (slot.level <= 95) slot.level += 5;
                    else slot.level = 100;
                    break;
                case INPUT_MINUS:
                    if (slot.level >= 5) slot.level -= 5;
                    else slot.level = 0;
                    break;
                case INPUT_ENTER:
                    applyTrim();
                    _character.setState(CHAR_SUCCESS);
                    _subState = STATE_LIST;
                    break;
                case INPUT_ESC:
                    _subState = STATE_LIST;
                    _character.setState(CHAR_IDLE);
                    break;
                default:
                    break;
            }
            break;
        }

        case STATE_LOAD_BROWSER:
            switch (event) {
                case INPUT_UP:
                    if (_fileCursor > 0) _fileCursor--;
                    break;
                case INPUT_DOWN:
                    if (_fileCursor < _wavFileCount - 1) _fileCursor++;
                    break;
                case INPUT_ENTER: {
                    SoundSlotOps::free(_previewSlot);
                    char path[64];
                    snprintf(path, sizeof(path), "/beepbotdx/samples/%s", _wavFiles[_fileCursor]);
                    if (Storage::loadWav(_project.sounds[_cursor], path)) {
                        snprintf(_statusMsg, sizeof(_statusMsg), "OK: %s", path);
                        _character.setState(CHAR_SUCCESS);
                    } else {
                        snprintf(_statusMsg, sizeof(_statusMsg), "FAIL: %s", path);
                        _character.setState(CHAR_ERROR);
                    }
                    _statusTime = millis();
                    _subState = STATE_LIST;
                    break;
                }
                case INPUT_SPACE: {
                    char path[64];
                    snprintf(path, sizeof(path), "/beepbotdx/samples/%s", _wavFiles[_fileCursor]);
                    SoundSlotOps::free(_previewSlot);
                    if (Storage::loadWav(_previewSlot, path)) {
                        Audio::triggerSound(_previewSlot.samples, _previewSlot.length, _previewSlot.sampleRate);
                        _character.setState(CHAR_PLAYING);
                    }
                    break;
                }
                case INPUT_ESC:
                    SoundSlotOps::free(_previewSlot);
                    _subState = STATE_LIST;
                    _character.setState(CHAR_IDLE);
                    break;
                default:
                    break;
            }
            break;

        case STATE_RENAME:
            switch (event) {
                case INPUT_ENTER:
                    if (_renameLen > 0) {
                        SoundSlotOps::setName(_project.sounds[_cursor], _renameBuffer);
                        _character.setState(CHAR_SUCCESS);
                        _character.say("renamed!");
                    }
                    _subState = STATE_LIST;
                    break;
                case INPUT_ESC:
                    _subState = STATE_LIST;
                    _character.setState(CHAR_IDLE);
                    break;
                case INPUT_BACK:
                    if (_renameLen > 0) {
                        _renameLen--;
                        _renameBuffer[_renameLen] = '\0';
                    }
                    break;
                case INPUT_CHAR:
                    if (_renameLen < 8) {
                        _renameBuffer[_renameLen] = Input::getChar();
                        _renameLen++;
                        _renameBuffer[_renameLen] = '\0';
                    }
                    break;
                case INPUT_SPACE:
                    if (_renameLen < 8) {
                        _renameBuffer[_renameLen] = ' ';
                        _renameLen++;
                        _renameBuffer[_renameLen] = '\0';
                    }
                    break;
                case INPUT_NUM1: case INPUT_NUM2: case INPUT_NUM3:
                case INPUT_NUM4: case INPUT_NUM5: case INPUT_NUM6:
                case INPUT_NUM7: case INPUT_NUM8: case INPUT_NUM9:
                    if (_renameLen < 8) {
                        _renameBuffer[_renameLen] = '1' + (event - INPUT_NUM1);
                        _renameLen++;
                        _renameBuffer[_renameLen] = '\0';
                    }
                    break;
                case INPUT_NUM0:
                    if (_renameLen < 8) {
                        _renameBuffer[_renameLen] = '0';
                        _renameLen++;
                        _renameBuffer[_renameLen] = '\0';
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}

void SoundView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    const int startY = 24;
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    switch (_subState) {
        case STATE_LIST: {
            const int margin = 3;
            const int spacing = 3;
            const int cols = 4;
            const int rows = 2;
            const int gridTop = 22;
            const int gridW = SCREEN_WIDTH - margin * 2;
            const int gridH = SCREEN_HEIGHT - gridTop - margin;
            const int cellW = (gridW - spacing * (cols - 1)) / cols;
            const int cellH = (gridH - spacing * (rows - 1)) / rows;
            const int gridLeft = margin + (gridW - cellW * cols - spacing * (cols - 1)) / 2;
            const int gridTopY = gridTop + (gridH - cellH * rows - spacing * (rows - 1)) / 2;

            for (int i = 0; i < NUM_SOUNDS; i++) {
                int col = i % cols;
                int row = i / cols;
                int x = gridLeft + col * (cellW + spacing);
                int y = gridTopY + row * (cellH + spacing);

                bool selected = (i == _cursor);
                bool occupied = _project.sounds[i].occupied;
                bool flashing = (_flashSlot == i && (millis() - _flashTime) < 150);

                // Cell background
                uint16_t bgColor;
                if (flashing) bgColor = theme.dim;
                else if (selected) bgColor = TFT_WHITE;
                else if (occupied) bgColor = theme.accent;
                else bgColor = theme.dark;
                canvas.fillRect(x, y, cellW, cellH, bgColor);

                // Cell content: "01\nName"
                canvas.setTextDatum(top_left);
                char numStr[4];
                snprintf(numStr, sizeof(numStr), "%02d", i + 1);

                if (occupied) {
                    uint16_t textColor = (bgColor == theme.dark) ? TFT_WHITE : TFT_BLACK;
                    canvas.setTextColor(textColor);
                    canvas.drawString(numStr, x + 4, y + 4);
                    canvas.drawString(_project.sounds[i].name, x + 4, y + 14);
                } else {
                    const uint16_t gray50 = 0x7BEF;
                    canvas.setTextColor(selected ? TFT_BLACK : gray50);
                    canvas.drawString(numStr, x + 4, y + 4);
                    canvas.drawString("-", x + 4, y + 14);
                }
            }

            // Memory usage
            uint32_t used = 0;
            for (int i = 0; i < NUM_SOUNDS; i++) {
                if (_project.sounds[i].occupied)
                    used += _project.sounds[i].length * 2;
            }
            uint32_t budget = Memory::getSampleBudget();
            uint8_t pct = budget > 0 ? (uint8_t)(used * 100 / budget) : 0;
            char memStr[12];
            snprintf(memStr, sizeof(memStr), "MEM %d%%", pct);
            canvas.setTextDatum(top_left);
            canvas.setTextColor(theme.dim);
            canvas.drawString(memStr, 4, SCREEN_HEIGHT - 10);

            // Status message (show for 3 seconds)
            if (_statusMsg[0] && (millis() - _statusTime < 3000)) {
                canvas.setTextColor(theme.highlight);
                canvas.setTextDatum(top_right);
                canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
            }
            break;
        }

        case STATE_RECORD_READY: {

            char slotLabel[16];
            snprintf(slotLabel, sizeof(slotLabel), "Slot %d", _cursor + 1);
            canvas.setTextColor(theme.dark);
            canvas.drawString(slotLabel, 8, startY + 16);

            canvas.setTextColor(theme.highlight);
            canvas.drawString("press ENTER to start", 8, startY + 40);

            break;
        }

        case STATE_RECORDING: {

            char slotLabel[32];
            uint32_t recorded = Audio::getRecordedLength();
            float seconds = (float)recorded / SAMPLE_RATE;
            snprintf(slotLabel, sizeof(slotLabel), "Slot %d  %.1fs", _cursor + 1, seconds);
            canvas.setTextColor(theme.accent);
            canvas.drawString(slotLabel, 8, startY + 16);

            // Record indicator
            canvas.fillCircle(SCREEN_WIDTH / 2, startY + 50, 8, theme.accent);

            break;
        }

        case STATE_TRIM: {
            SoundSlot& slot = _project.sounds[_cursor];

            const int margin = 3;
            const int hdrGridW = SCREEN_WIDTH - margin * 2;
            const int hdrCellW = (hdrGridW - margin * 3) / 4;
            const int hdrContentW = hdrCellW * 4 + margin * 3;
            const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
            const int infoY = 22;

            float startSec = (float)_trimStart / slot.sampleRate;
            float endSec = (float)_trimEnd / slot.sampleRate;
            float durSec = endSec - startSec;

            // Upper left: TRIM
            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_left);
            canvas.drawString("TRIM", hdrLeft + 4, infoY);

            // Upper center: total duration
            char durStr[10];
            snprintf(durStr, sizeof(durStr), "%.2fs", durSec);
            canvas.setTextDatum(top_center);
            canvas.drawString(durStr, hdrLeft + hdrContentW / 2, infoY);

            // Upper right: level
            char lvlStr[8];
            snprintf(lvlStr, sizeof(lvlStr), "LVL:%d", slot.level);
            canvas.setTextDatum(top_right);
            canvas.drawString(lvlStr, hdrLeft + hdrContentW - 4, infoY);

            // Waveform
            const int waveTop = infoY + 12;
            const int waveBottom = SCREEN_HEIGHT - 14;
            drawWaveform(canvas, hdrLeft + 4, waveTop, hdrContentW - 8, waveBottom - waveTop);

            // Lower left: start time
            char startStr[10];
            snprintf(startStr, sizeof(startStr), "%.2fs", startSec);
            canvas.setTextColor(_trimMovingEnd ? theme.dim : theme.accent);
            canvas.setTextDatum(top_left);
            canvas.drawString(startStr, hdrLeft + 4, waveBottom + 2);

            // Lower right: end time
            char endStr[10];
            snprintf(endStr, sizeof(endStr), "%.2fs", endSec);
            canvas.setTextColor(_trimMovingEnd ? theme.accent : theme.dim);
            canvas.setTextDatum(top_right);
            canvas.drawString(endStr, hdrLeft + hdrContentW - 4, waveBottom + 2);

            break;
        }

        case STATE_LOAD_BROWSER: {
            const int itemH = 11;
            const int listTop = startY + 14;
            const int visibleItems = (SCREEN_HEIGHT - 12 - listTop) / itemH;
            int scrollOffset = 0;
            if (_fileCursor >= visibleItems) {
                scrollOffset = _fileCursor - visibleItems + 1;
            }

            for (int i = scrollOffset; i < _wavFileCount && (i - scrollOffset) < visibleItems; i++) {
                int y = listTop + (i - scrollOffset) * itemH;

                if (i == _fileCursor) {
                    canvas.setTextColor(theme.accent);
                    canvas.drawString(">", 4, y);
                }
                canvas.setTextColor(i == _fileCursor ? theme.accent : TFT_WHITE);
                canvas.drawString(_wavFiles[i], 12, y);
            }

            break;
        }

        case STATE_RENAME: {

            char slotLabel[16];
            snprintf(slotLabel, sizeof(slotLabel), "Slot %d", _cursor + 1);
            canvas.setTextColor(theme.dark);
            canvas.drawString(slotLabel, 8, startY + 14);

            // Show typed name with cursor
            char display[12];
            snprintf(display, sizeof(display), "%s_", _renameBuffer);
            canvas.setTextColor(theme.highlight);
            canvas.drawString(display, 8, startY + 34);

            break;
        }
    }
}

void SoundView::drawWaveform(Canvas& canvas, int x, int y, int w, int h) {
    SoundSlot& slot = _project.sounds[_cursor];
    if (!slot.samples || slot.length == 0) return;

    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    int midY = y + h / 2;

    // Find peak amplitude for normalization (min threshold so silence stays small)
    int16_t peak = 0;
    for (int px = 0; px < w; px += 2) {
        uint32_t sampleIdx = (uint32_t)((float)px / w * slot.length);
        int16_t val = abs(slot.samples[sampleIdx]);
        if (val > peak) peak = val;
    }
    const int16_t minPeak = 4000;
    if (peak < minPeak) peak = minPeak;

    // Draw waveform (2px wide bars)
    for (int px = 0; px < w; px += 2) {
        uint32_t sampleIdx = (uint32_t)((float)px / w * slot.length);
        int16_t sampleVal = slot.samples[sampleIdx];
        int barH = (int)((float)abs(sampleVal) / peak * (h / 2));
        if (barH < 1) barH = 1;

        uint16_t color;
        if (sampleIdx >= _trimStart && sampleIdx <= _trimEnd) {
            color = theme.accent;
        } else {
            color = theme.dark;
        }

        canvas.fillRect(x + px, midY - barH, 2, barH * 2, color);
    }

    // Draw start marker (dotted line, 36px tall, 2x2 dots)
    const uint16_t gray50 = 0x7BEF;
    int startPx = x + (int)((float)_trimStart / slot.length * w);
    int markerH = 36;
    int markerY = midY - markerH / 2 + 1;
    uint16_t startColor = _trimMovingEnd ? gray50 : TFT_WHITE;
    for (int dy = 0; dy < markerH; dy += 4) {
        canvas.fillRect(startPx, markerY + dy, 2, 2, startColor);
    }

    // Draw end marker (dotted line, 36px tall, 2x2 dots)
    int endPx = x + (int)((float)_trimEnd / slot.length * w);
    uint16_t endColor = _trimMovingEnd ? TFT_WHITE : gray50;
    for (int dy = 0; dy < markerH; dy += 4) {
        canvas.fillRect(endPx, markerY + dy, 2, 2, endColor);
    }

}

void SoundView::exit() {
    if (_subState == STATE_RECORDING) {
        stopRecording();
    }
}

void SoundView::startRecording() {
    SoundSlot& slot = _project.sounds[_cursor];
    if (!SoundSlotOps::allocate(slot, MAX_SAMPLE_LENGTH)) {
        _character.setState(CHAR_ERROR);
        _character.say("no memory!");
        snprintf(_statusMsg, sizeof(_statusMsg), "NO MEMORY");
        _statusTime = millis();
        return;
    }
    Audio::recordStart(slot.samples, MAX_SAMPLE_LENGTH);
    _subState = STATE_RECORDING;
    _character.setState(CHAR_RECORDING);
    _character.say("listening...");
}

void SoundView::stopRecording() {
    Audio::recordStop();
    SoundSlot& slot = _project.sounds[_cursor];
    slot.length = Audio::getRecordedLength();
    slot.occupied = (slot.length > 0);

    if (slot.occupied) {
        char defaultName[9];
        snprintf(defaultName, sizeof(defaultName), "REC%d", _cursor + 1);
        SoundSlotOps::setName(slot, defaultName);

        // Enter trim mode
        _trimStart = 0;
        _trimEnd = slot.length;
        _trimMovingEnd = false;
        _subState = STATE_TRIM;
        _character.setState(CHAR_FOCUSED);
        _character.say("got it!");
    } else {
        _character.setState(CHAR_ERROR);
        _character.say("nothing?");
        _subState = STATE_LIST;
    }
}

void SoundView::applyTrim() {
    SoundSlot& slot = _project.sounds[_cursor];
    uint32_t newLen = _trimEnd - _trimStart;

    if (_trimStart > 0 && newLen > 0) {
        memmove(slot.samples, slot.samples + _trimStart, newLen * sizeof(int16_t));
    }

    slot.length = newLen;

    // Shrink buffer to free wasted memory
    SoundSlotOps::shrinkToFit(slot);

    // Auto-save to SD
    if (Storage::isReady()) {
        char path[32];
        snprintf(path, sizeof(path), "/beepbotdx/samples/%s.wav", slot.name);
        Storage::saveWav(slot, path);
    }
}

void SoundView::triggerSlot(uint8_t index) {
    if (index >= NUM_SOUNDS) return;
    SoundSlot& slot = _project.sounds[index];
    if (!slot.occupied) {
        _character.setState(CHAR_ERROR);
        _character.say("empty");
        return;
    }
    Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100);
    _character.setState(CHAR_BEAT);
    _flashSlot = index;
    _flashTime = millis();
}
