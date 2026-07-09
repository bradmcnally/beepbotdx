#include "sound_view.h"
#include "views/settings_view.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/storage.h"
#include "platform/memory.h"
#include "core/theme.h"
#include "core/timing.h"
#include "core/bloom_field.h"
#include "core/grid_layout.h"
#include "config.h"
#include <cstring>

static uint8_t computeAudioLevel(const int16_t* samples, uint32_t totalLength) {
    if (!samples || totalLength == 0) return 0;
    uint32_t scanStart = (totalLength > 1600) ? totalLength - 1600 : 0;
    int16_t peak = 0;
    for (uint32_t i = scanStart; i < totalLength; i++) {
        int16_t val = samples[i] < 0 ? -samples[i] : samples[i];
        if (val > peak) peak = val;
    }
    return (uint8_t)(peak >> 7);
}

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
    _confirmingDelete = false;
}

void SoundView::enter() {
    _subState = STATE_LIST;
    _character.setState(CHAR_IDLE);
}

void SoundView::update(InputEvent event) {
    // Number keys always trigger sounds regardless of substate
    if (_subState != STATE_TRIM && _subState != STATE_RENAME && event >= INPUT_NUM1 && event <= INPUT_NUM8) {
        uint8_t idx = event - INPUT_NUM1;
        triggerSlot(idx);
        return;
    }

    if (_confirmingDelete) {
        if (event == INPUT_ENTER) {
            SoundSlotOps::free(_project.sounds[_cursor]);
            _project.dirty = true;
            _character.setState(CHAR_CRYING);
            _character.say("cleared");
            _confirmingDelete = false;
        } else if (event == INPUT_ESC || event == INPUT_BACK) {
            _confirmingDelete = false;
            _character.setState(CHAR_IDLE);
        }
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
                        { uint32_t used = 0;
                        for (int i = 0; i < NUM_SOUNDS; i++) {
                            if (_project.sounds[i].occupied) used += _project.sounds[i].length * 2;
                        }
                        uint32_t budget = Memory::getSampleBudget();
                        uint8_t pct = budget > 0 ? (uint8_t)(used * 100 / budget) : 0;
                        char memMsg[12]; snprintf(memMsg, sizeof(memMsg), "mem %d%%", pct);
                        _character.say(memMsg); }
                    }
                    break;
                case INPUT_SPACE:
                    triggerSlot(_cursor);
                    break;
                case INPUT_PLUS:
                    _subState = STATE_RECORD_READY;
                    _character.setState(CHAR_IDLE);
                    { char memMsg[16]; snprintf(memMsg, sizeof(memMsg), "%luKB free", Memory::getFree() / 1024); _character.say(memMsg); }
                    break;
                case INPUT_BACK:
                    if (_project.sounds[_cursor].occupied) {
                        if (GlobalSettings::instance && GlobalSettings::instance->confirmDelete) {
                            _confirmingDelete = true;
                        } else {
                            SoundSlotOps::free(_project.sounds[_cursor]);
                            _project.dirty = true;
                            _character.setState(CHAR_SUCCESS);
                            _character.say("cleared");
                        }
                    }
                    break;
                case INPUT_CHAR: {
                    char ch = Input::getChar();
                    if (ch == 'r' && _project.sounds[_cursor].occupied) {
                        strncpy(_renameBuffer, _project.sounds[_cursor].name, 8);
                        _renameBuffer[8] = '\0';
                        _renameLen = strlen(_renameBuffer);
                        _subState = STATE_RENAME;
                        _character.setState(CHAR_FOCUSED);
                    } else if (ch == 'i') {
                        _fileCursor = 0;
                        if (!Storage::isReady()) {
                            _character.setState(CHAR_ERROR);
                        } else if (Storage::listWavFiles("/beepbotdx/samples", _wavFiles, _wavFileCount, 64) && _wavFileCount > 0) {
                            _subState = STATE_LOAD_BROWSER;
                            _character.setState(CHAR_FOCUSED);
                        } else {
                            _character.setState(CHAR_ERROR);
                        }
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
                    _countdownStart = millis();
                    _countdownBeat = 0;
                    _subState = STATE_COUNTDOWN;
                    _character.setState(CHAR_FOCUSED);
                    break;
                case INPUT_CHAR:
                    if (Input::getChar() == 'i') {
                        _fileCursor = 0;
                        if (!Storage::isReady()) {
                            _character.setState(CHAR_ERROR);
                        } else if (Storage::listWavFiles("/beepbotdx/samples", _wavFiles, _wavFileCount, 64) && _wavFileCount > 0) {
                            _subState = STATE_LOAD_BROWSER;
                            _character.setState(CHAR_FOCUSED);
                        } else {
                            _character.setState(CHAR_ERROR);
                        }
                    }
                    break;
                case INPUT_ESC:
                    _subState = STATE_LIST;
                    _character.setState(CHAR_IDLE);
                    break;
                default:
                    break;
            }
            break;

        case STATE_COUNTDOWN: {
            if (event == INPUT_ESC) {
                _subState = STATE_LIST;
                _character.setState(CHAR_IDLE);
                break;
            }
            uint32_t beatMs = 60000 / _project.bpm;
            uint8_t elapsed = (millis() - _countdownStart) / beatMs;
            if (elapsed > _countdownBeat) {
                _countdownBeat = elapsed;
                if (_countdownBeat >= 3) {
                    startRecording();
                } else {
                    _character.setState(CHAR_BEAT);
                }
            }
            break;
        }

        case STATE_RECORDING:
            if (event == INPUT_ENTER || Audio::getRecordedLength() >= MAX_SAMPLE_LENGTH) {
                stopRecording();
            }
            break;

        case STATE_RECORD_DONE:
            if (millis() - _recordDoneTime > 500) {
                SoundSlot& slot = _project.sounds[_cursor];
                _trimStart = 0;
                _trimEnd = slot.length;
                _trimMovingEnd = false;
                _subState = STATE_TRIM;
                _character.setState(CHAR_FOCUSED);
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
                    _project.dirty = true;
                    break;
                case INPUT_MINUS:
                    if (slot.level >= 5) slot.level -= 5;
                    else slot.level = 0;
                    _project.dirty = true;
                    break;
                case INPUT_ENTER: {
                    applyTrim();
                    _character.setState(CHAR_SUCCESS);
                    uint32_t used = 0;
                    for (int i = 0; i < NUM_SOUNDS; i++) {
                        if (_project.sounds[i].occupied)
                            used += _project.sounds[i].length * 2;
                    }
                    uint32_t budget = Memory::getSampleBudget();
                    uint8_t pct = budget > 0 ? (uint8_t)(used * 100 / budget) : 0;
                    char memMsg[12];
                    snprintf(memMsg, sizeof(memMsg), "mem %d%%", pct);
                    _character.say(memMsg);
                    _subState = STATE_LIST;
                    break;
                }
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
                        _project.dirty = true;
                        snprintf(_statusMsg, sizeof(_statusMsg), "OK: %s", path);
                        _character.setState(CHAR_SUCCESS);
                        _character.say("nice one");
                    } else {
                        snprintf(_statusMsg, sizeof(_statusMsg), "FAIL: %s", path);
                        _character.setState(CHAR_ERROR);
                        _character.say("oh no");
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
                        _character.setState(CHAR_IDLE);
                    }
                    break;
                }
                case INPUT_ESC:
                    SoundSlotOps::free(_previewSlot);
                    _subState = STATE_RECORD_READY;
                    _character.setState(CHAR_IDLE);
                    { char memMsg[16]; snprintf(memMsg, sizeof(memMsg), "%luKB free", Memory::getFree() / 1024); _character.say(memMsg); }
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
                        _project.dirty = true;
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
            GridLayout grid = GridLayout::make(4, 2, 22);

            for (int i = 0; i < NUM_SOUNDS; i++) {
                int x, y;
                grid.cellXY(i, x, y);

                bool selected = (i == _cursor);
                bool occupied = _project.sounds[i].occupied;
                bool flashing = (_flashSlot == i && (millis() - _flashTime) < 150);

                // Cell background
                uint16_t bgColor;
                uint16_t textColor;
                if (flashing) { bgColor = theme.dim; textColor = theme.textOnAccent; }
                else if (selected) { bgColor = theme.accent; textColor = theme.textOnAccent; }
                else { bgColor = theme.dark; textColor = occupied ? theme.accent : theme.dim; }
                canvas.fillRect(x, y, grid.cellW, grid.cellH, bgColor);

                canvas.setTextDatum(top_left);
                canvas.setTextColor(textColor);
                char numStr[4];
                snprintf(numStr, sizeof(numStr), "%02d", i + 1);
                canvas.drawString(numStr, x + 4, y + 4);
                if (occupied) canvas.drawString(_project.sounds[i].name, x + 4, y + 14);
            }


            // Status message (show for 3 seconds)
            if (_statusMsg[0] && (millis() - _statusTime < 3000)) {
                canvas.setTextColor(theme.highlight);
                canvas.setTextDatum(top_right);
                canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
            }

            if (_confirmingDelete) {
                const int boxW = 150;
                const int boxH = 40;
                const int boxX = (SCREEN_WIDTH - boxW) / 2;
                const int boxY = (SCREEN_HEIGHT - boxH) / 2;
                canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
                canvas.drawRect(boxX, boxY, boxW, boxH, theme.accent);

                canvas.setTextColor(TFT_WHITE);
                canvas.setTextDatum(top_center);
                canvas.drawString("Delete sound?", boxX + boxW / 2, boxY + 6);

                canvas.setTextColor(theme.accent);
                canvas.drawString("OK:yes  ESC:no", boxX + boxW / 2, boxY + 22);
            }
            break;
        }

        case STATE_RECORD_READY: {
            const int margin = 3;
            const int gap = 3;
            const int top = 22;
            const int boxH = SCREEN_HEIGHT - top - margin;
            const int totalW = SCREEN_WIDTH - margin * 2;
            const int boxW = (totalW - gap) / 2;
            int leftX = margin;
            int rightX = margin + boxW + gap;

            canvas.fillRect(leftX, top, boxW, boxH, theme.dark);
            canvas.fillRect(rightX, top, boxW, boxH, theme.dark);

            canvas.setTextDatum(top_center);
            int textY = top + (boxH - 17) / 2;

            canvas.setTextColor(theme.accent);
            canvas.drawString("ENTER", leftX + boxW / 2, textY);
            canvas.drawString("to record", leftX + boxW / 2, textY + 10);

            canvas.setTextColor(theme.accent);
            canvas.drawString("[i] import", rightX + boxW / 2, textY);
            canvas.drawString("from SD card", rightX + boxW / 2, textY + 10);
            break;
        }

        case STATE_COUNTDOWN: {
            uint8_t count = 3 - _countdownBeat;
            char countStr[2] = { (char)('0' + count), '\0' };
            canvas.setTextSize(3);
            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_center);
            canvas.drawString(countStr, SCREEN_WIDTH / 2, startY + (SCREEN_HEIGHT - startY - 21) / 2 - 8);
            canvas.setTextSize(1);
            break;
        }

        case STATE_RECORDING: {
            SoundSlot& slot = _project.sounds[_cursor];
            uint32_t recorded = Audio::getRecordedLength();
            uint8_t level = computeAudioLevel(slot.samples, recorded);

            float progress = (float)recorded / MAX_SAMPLE_LENGTH;
            if (progress > 1.0f) progress = 1.0f;

            if (level > 10) {
                uint8_t boosted = level < 85 ? level * 3 : 255;
                BloomFieldOps::injectAt(_bloom, boosted, BLOOM_COLS / 3, BLOOM_ROWS * 2 / 5);
                BloomFieldOps::injectAt(_bloom, boosted * 4 / 5, BLOOM_COLS * 2 / 3, BLOOM_ROWS * 3 / 5);
                BloomFieldOps::injectAt(_bloom, boosted * 3 / 4, BLOOM_COLS / 5, BLOOM_ROWS * 3 / 4);
            }

            BloomFieldOps::tick(_bloom, millis());

            // Info row: RECORDING (left), time (right)
            float seconds = (float)recorded / SAMPLE_RATE;
            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_left);
            canvas.drawString("RECORDING", 7, startY);

            char timeStr[8];
            snprintf(timeStr, sizeof(timeStr), "%.1fs", seconds);
            canvas.setTextDatum(top_right);
            canvas.drawString(timeStr, SCREEN_WIDTH - 8, startY);

            // Bloom field — with 3px margin
            const int fieldX = 3;
            const int fieldY = startY + 10;
            char ch[2] = {0, 0};
            canvas.setTextDatum(top_left);

            for (int row = 0; row < BLOOM_ROWS; row++) {
                for (int col = 0; col < BLOOM_COLS; col++) {
                    uint8_t energy = _bloom.cells[row][col].energy;
                    ch[0] = BloomFieldOps::glyphForEnergy(energy);
                    if (ch[0] == ' ') continue;

                    if (energy > 150) canvas.setTextColor(TFT_WHITE);
                    else if (energy > 100) canvas.setTextColor(theme.highlight);
                    else if (energy > 40) canvas.setTextColor(theme.accent);
                    else if (energy > 10) canvas.setTextColor(theme.dim);
                    else canvas.setTextColor(theme.dark);

                    canvas.drawString(ch, fieldX + col * 6, fieldY + row * 8);
                }
            }

            // Input level bar
            const int barY = SCREEN_HEIGHT - 10;
            const int barX = 8;
            const int barW = SCREEN_WIDTH - 16;
            const int barH = 4;
            canvas.fillRect(barX, barY, barW, barH, theme.dark);
            int fillW = (int)(level * barW / 255);
            if (fillW > 0) {
                uint16_t barColor = (level > 200) ? theme.highlight : theme.accent;
                canvas.fillRect(barX, barY, fillW, barH, barColor);
            }

            break;
        }

        case STATE_RECORD_DONE: {
            SoundSlot& slot = _project.sounds[_cursor];
            float seconds = (float)slot.length / SAMPLE_RATE;

            // Info row: RECORDING (left), final time (right)
            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_left);
            canvas.drawString("RECORDING", 7, startY);

            char timeStr[8];
            snprintf(timeStr, sizeof(timeStr), "%.1fs", seconds);
            canvas.setTextDatum(top_right);
            canvas.drawString(timeStr, SCREEN_WIDTH - 8, startY);

            // Keep showing bloom field frozen
            const int fieldX = 3;
            const int fieldY = startY + 10;
            char ch[2] = {0, 0};
            canvas.setTextDatum(top_left);

            for (int row = 0; row < BLOOM_ROWS; row++) {
                for (int col = 0; col < BLOOM_COLS; col++) {
                    uint8_t energy = _bloom.cells[row][col].energy;
                    ch[0] = BloomFieldOps::glyphForEnergy(energy);
                    if (ch[0] == ' ') continue;

                    if (energy > 150) canvas.setTextColor(TFT_WHITE);
                    else if (energy > 100) canvas.setTextColor(theme.highlight);
                    else if (energy > 40) canvas.setTextColor(theme.accent);
                    else if (energy > 10) canvas.setTextColor(theme.dim);
                    else canvas.setTextColor(theme.dark);

                    canvas.drawString(ch, fieldX + col * 6, fieldY + row * 8);
                }
            }
            break;
        }

        case STATE_TRIM: {
            SoundSlot& slot = _project.sounds[_cursor];

            const int margin = 3;
            const int hdrGridW = SCREEN_WIDTH - margin * 2;
            const int hdrCellW = (hdrGridW - margin * 3) / 4;
            const int hdrContentW = hdrCellW * 4 + margin * 3;
            const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
            const int infoY = 23;

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
            const int itemH = 10;
            const int listTop = startY + 4;
            const int listBottom = SCREEN_HEIGHT - 4;
            const int visibleItems = (listBottom - listTop) / itemH;
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

            if (_wavFileCount > visibleItems) {
                int maxScroll = _wavFileCount - visibleItems;
                const int trackTop = listTop;
                const int trackH = listBottom - listTop;
                int thumbH = trackH * visibleItems / _wavFileCount;
                if (thumbH < 6) thumbH = 6;
                int thumbY = trackTop + (trackH - thumbH) * scrollOffset / maxScroll;
                canvas.fillRect(SCREEN_WIDTH - 7, thumbY, 3, thumbH, theme.accent);
            }

            break;
        }

        case STATE_RENAME: {
            const int margin = 3;
            const int hdrGridW = SCREEN_WIDTH - margin * 2;
            const int hdrCellW = (hdrGridW - margin * 3) / 4;
            const int hdrContentW = hdrCellW * 4 + margin * 3;
            const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
            const int infoY = 23;

            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_left);
            canvas.drawString("RENAME", hdrLeft + 4, infoY);

            canvas.setTextColor(theme.dim);
            canvas.setTextDatum(top_right);
            canvas.drawString("8 CHAR", SCREEN_WIDTH - margin - 4, infoY);
            canvas.setTextDatum(top_left);

            // Show typed name with blinking cursor
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

            canvas.setTextColor(theme.accent);
            canvas.setTextDatum(top_center);
            canvas.drawString("[Esc] Cancel    [Ok] Confirm", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 14);
            canvas.setTextDatum(top_left);

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

    // Draw waveform (2px wide bars, scaled by level)
    float levelScale = slot.level / 100.0f;
    for (int px = 0; px < w; px += 2) {
        uint32_t sampleIdx = (uint32_t)((float)px / w * slot.length);
        int16_t sampleVal = slot.samples[sampleIdx];
        int barH = (int)((float)abs(sampleVal) / peak * (h / 2) * levelScale);
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
    SoundSlotOps::free(_previewSlot);
}

void SoundView::startRecording() {
    SoundSlot& slot = _project.sounds[_cursor];
    if (!SoundSlotOps::allocate(slot, MAX_SAMPLE_LENGTH)) {
        _character.setState(CHAR_DEAD);
        _character.say("no memory!");
        snprintf(_statusMsg, sizeof(_statusMsg), "NO MEMORY");
        _statusTime = millis();
        return;
    }
    Audio::recordStart(slot.samples, MAX_SAMPLE_LENGTH);
    BloomFieldOps::reset(_bloom);
    BloomFieldOps::inject(_bloom, 80, 0.0f);
    BloomFieldOps::step(_bloom);
    _recordStartTime = millis();
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
        _project.dirty = true;

        _subState = STATE_RECORD_DONE;
        _recordDoneTime = millis();
        _character.setState(CHAR_SUCCESS);
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
    _project.dirty = true;

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
