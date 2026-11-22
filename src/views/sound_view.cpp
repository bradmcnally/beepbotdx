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
#include "core/slot_fx.h"
#include "config.h"
#ifdef CARDPUTER_ZERO
#include "platform_desktop/storage_paths.h"
#include <algorithm>
#include <cctype>
#endif
#include <cstring>
#include <cmath>

static uint8_t computeAudioLevel(const SoundSlot& slot, uint32_t totalLength) {
    if (!slot.samples || totalLength == 0) return 0;
    uint32_t scanStart = (totalLength > 1600) ? totalLength - 1600 : 0;
    int32_t peak = 0;
    for (uint32_t i = scanStart; i < totalLength; i++) {
        int16_t sample = SoundSlotOps::getSample(slot, i);
        int32_t val = sample < 0 ? -static_cast<int32_t>(sample) : sample;
        if (val > peak) peak = val;
    }
    return static_cast<uint8_t>(peak >= 32768 ? 255 : peak >> 7);
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
    , _trimMovingEnd(false)
    , _recordMaxLength(0) {
    _statusMsg[0] = '\0';
    SoundSlotOps::init(_previewSlot);
    _flashSlot = 0xFF;
    _flashTime = 0;
    _playbackActive = false;
    _playbackStart = 0;
    _playbackOffset = 0;
    _playbackLength = 0;
    _playbackRate = SAMPLE_RATE;
    _confirmingDelete = false;
    _recordMaxLength = 0;
}

void SoundView::enter() {
    _subState = STATE_LIST;
    _playbackActive = false;
    _character.setState(CHAR_IDLE);
}

void SoundView::update(InputEvent event) {
    if (_subState != STATE_TRIM && _subState != STATE_RENAME && event >= INPUT_NUM1 && event <= INPUT_NUM8) {
        uint8_t idx = event - INPUT_NUM1;
        triggerSlot(idx);
        return;
    }

    if (_confirmingDelete) {
        if (event == INPUT_ENTER) {
            Audio::stopAll();
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
        case STATE_LIST:         updateList(event); break;
        case STATE_RECORD_READY: updateRecordReady(event); break;
        case STATE_COUNTDOWN:    updateCountdown(event); break;
        case STATE_RECORDING:    updateRecording(event); break;
        case STATE_RECORD_DONE:  updateRecordDone(); break;
        case STATE_TRIM:         updateTrim(event); break;
        case STATE_FX:           updateFx(event); break;
        case STATE_LOAD_BROWSER: updateLoadBrowser(event); break;
        case STATE_RENAME:       updateRename(event); break;
    }
}

void SoundView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    switch (_subState) {
        case STATE_LIST:         drawList(canvas, theme); break;
        case STATE_RECORD_READY: drawRecordReady(canvas, theme); break;
        case STATE_COUNTDOWN:    drawCountdown(canvas, theme); break;
        case STATE_RECORDING:    drawRecording(canvas, theme); break;
        case STATE_RECORD_DONE:  drawRecordDone(canvas, theme); break;
        case STATE_TRIM:         drawTrim(canvas, theme); break;
        case STATE_FX:           drawFx(canvas, theme); break;
        case STATE_LOAD_BROWSER: drawLoadBrowser(canvas, theme); break;
        case STATE_RENAME:       drawRename(canvas, theme); break;
    }
}

void SoundView::exit() {
    if (_subState == STATE_RECORDING) {
        stopRecording();
    }
    Audio::stopAll();
    SoundSlotOps::free(_previewSlot);
}

// --- Update methods ---

void SoundView::updateList(InputEvent event) {
    if (Input::wasRecordPressed()) {
        startRecording();
        return;
    }

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
                _playbackActive = false;
                _subState = STATE_TRIM;
                _character.setState(CHAR_FOCUSED);
            } else {
                _recordReadySelection = 0;
                _subState = STATE_RECORD_READY;
                _character.setState(CHAR_FOCUSED);
                { char memMsg[16]; snprintf(memMsg, sizeof(memMsg), "%luKB free", (unsigned long)(Memory::getFree() / 1024));
                _character.say(memMsg); }
            }
            break;
        case INPUT_SPACE:
            triggerSlot(_cursor);
            break;
        case INPUT_BACK:
            if (_project.sounds[_cursor].occupied) {
                if (GlobalSettings::instance && GlobalSettings::instance->confirmDelete) {
                    _confirmingDelete = true;
                } else {
                    Audio::stopAll();
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
            } else if (ch == FX_SHORTCUT && _project.sounds[_cursor].occupied) {
                _fxCursor = 0;
                _subState = STATE_FX;
                _character.setState(CHAR_FOCUSED);
            } else if (ch == 't' && _project.sounds[_cursor].occupied) {
                SoundSlot& slot = _project.sounds[_cursor];
                _trimStart = 0;
                _trimEnd = slot.length;
                _trimMovingEnd = false;
                _playbackActive = false;
                _subState = STATE_TRIM;
                _character.setState(CHAR_FOCUSED);
            } else if (ch == 'n' && _project.sounds[_cursor].occupied) {
                toggleNormalize();
            } else if (ch == 'i') {
                openImportBrowser();
            }
            break;
        }
        default:
            break;
    }
}

void SoundView::updateRecordReady(InputEvent event) {
    switch (event) {
        case INPUT_LEFT:
        case INPUT_UP:
            _recordReadySelection = 0;
            break;
        case INPUT_RIGHT:
        case INPUT_DOWN:
            _recordReadySelection = 1;
            break;
        case INPUT_ENTER:
        case INPUT_SPACE:
            if (_recordReadySelection == 1) {
                openImportBrowser();
            } else {
                _countdownStart = millis();
                _countdownBeat = 0;
                _subState = STATE_COUNTDOWN;
                _character.setState(CHAR_FOCUSED);
            }
            break;
        case INPUT_CHAR:
            if (Input::getChar() == 'i') {
                openImportBrowser();
            } else if (Input::getChar() == 'r') {
                _countdownStart = millis();
                _countdownBeat = 0;
                _subState = STATE_COUNTDOWN;
                _character.setState(CHAR_FOCUSED);
            }
            break;
        case INPUT_ESC:
            _subState = STATE_LIST;
            _character.setState(CHAR_IDLE);
            break;
        default:
            break;
    }
}

void SoundView::updateCountdown(InputEvent event) {
    if (event == INPUT_ESC) {
        _subState = STATE_LIST;
        _character.setState(CHAR_IDLE);
        return;
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
}

void SoundView::updateRecording(InputEvent event) {
    if (event == INPUT_ENTER || Input::wasRecordReleased() || Audio::getRecordedLength() >= _recordMaxLength) {
        stopRecording();
    }
}

void SoundView::updateRecordDone() {
    if (millis() - _recordDoneTime > 500) {
        SoundSlot& slot = _project.sounds[_cursor];
        _trimStart = 0;
        _trimEnd = slot.length;
        _trimMovingEnd = false;
        _playbackActive = false;
        _subState = STATE_TRIM;
        _character.setState(CHAR_FOCUSED);
    }
}

void SoundView::updateTrim(InputEvent event) {
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
            {
                SoundSlot trimmed = slot;
                trimmed.samples = reinterpret_cast<int16_t*>(
                    reinterpret_cast<uint8_t*>(slot.samples) +
                    _trimStart * SoundSlotOps::bytesPerSample(slot.bitDepth));
                trimmed.length = _trimEnd - _trimStart;
                Audio::triggerSound(trimmed, slot.level * 255 / 100);
            }
            _playbackStart = millis();
            _playbackOffset = _trimStart;
            _playbackLength = _trimEnd - _trimStart;
            _playbackRate = slot.sampleRate;
            _playbackActive = true;
            _character.setState(CHAR_BEAT);
            break;
        case INPUT_ENTER: {
            applyTrim();
            _character.setState(CHAR_SUCCESS);
            char memMsg[16];
            snprintf(memMsg, sizeof(memMsg), "%luKB free", (unsigned long)(Memory::getFree() / 1024));
            _character.say(memMsg);
            _subState = STATE_LIST;
            break;
        }
        case INPUT_ESC:
            _subState = STATE_LIST;
            _character.setState(CHAR_IDLE);
            break;
        case INPUT_CHAR: {
            char ch = Input::getChar();
            if (ch == 'n') {
                toggleNormalize();
            } else if (ch == FX_SHORTCUT) {
                _fxCursor = 0;
                _subState = STATE_FX;
                _character.setState(CHAR_FOCUSED);
            }
            break;
        }
        default:
            break;
    }
}

void SoundView::updateFx(InputEvent event) {
    SoundSlot& slot = _project.sounds[_cursor];
    switch (event) {
        case INPUT_LEFT:
            if (_fxCursor > 0) _fxCursor--;
            break;
        case INPUT_RIGHT:
            if (_fxCursor < NUM_FX - 1) _fxCursor++;
            break;
        case INPUT_UP: {
            uint8_t step = (_fxCursor == FX_PITCH) ? 1 : 5;
            uint8_t max = SlotFxOps::maxValue(_fxCursor);
            if (slot.fx.value[_fxCursor] < max) {
                uint8_t v = slot.fx.value[_fxCursor] + step;
                slot.fx.value[_fxCursor] = (v > max) ? max : v;
                slot.fx.enabled[_fxCursor] = true;
                _project.dirty = true;
            }
            break;
        }
        case INPUT_DOWN: {
            uint8_t step = (_fxCursor == FX_PITCH) ? 1 : 5;
            if (slot.fx.value[_fxCursor] > 0) {
                uint8_t v = slot.fx.value[_fxCursor];
                slot.fx.value[_fxCursor] = (v >= step) ? v - step : 0;
                slot.fx.enabled[_fxCursor] = true;
                _project.dirty = true;
            }
            break;
        }
        case INPUT_ENTER:
            slot.fx.enabled[_fxCursor] = !slot.fx.enabled[_fxCursor];
            _project.dirty = true;
            break;
        case INPUT_SPACE:
            Audio::triggerSound(slot, slot.level * 255 / 100, &slot.fx);
            _character.setState(CHAR_BEAT);
            break;
        case INPUT_BACK:
            slot.fx.value[_fxCursor] = SlotFxOps::defaultValue(_fxCursor);
            slot.fx.enabled[_fxCursor] = true;
            _project.dirty = true;
            break;
        case INPUT_ESC:
            _subState = STATE_LIST;
            _character.setState(CHAR_IDLE);
            break;
        case INPUT_CHAR: {
            char ch = Input::getChar();
            if (ch == 'n') {
                toggleNormalize();
            } else if (ch == 't') {
                _trimStart = 0;
                _trimEnd = slot.length;
                _trimMovingEnd = false;
                _playbackActive = false;
                _subState = STATE_TRIM;
                _character.setState(CHAR_FOCUSED);
            }
            break;
        }
        default:
            break;
    }
}

void SoundView::openImportBrowser() {
    if (!Storage::isReady()) {
        _character.setState(CHAR_ERROR);
        _character.say("storage error");
        return;
    }
#ifdef CARDPUTER_ZERO
    _browserReturnState = _subState;
    _browserDirectory = Storage::sampleDirectory();
    _browserPreviewPath.clear();
    refreshImportBrowser();
    _subState = STATE_LOAD_BROWSER;
    _character.setState(CHAR_FOCUSED);
#else
    _fileCursor = 0;
    if (Storage::listWavFiles("/beepbotdx/samples", _wavFiles, _wavFileCount, 64)) {
        _subState = STATE_LOAD_BROWSER;
        _character.setState(CHAR_FOCUSED);
    } else {
        _character.setState(CHAR_ERROR);
        _character.say("folder error");
    }
#endif
}

#ifdef CARDPUTER_ZERO
void SoundView::refreshImportBrowser() {
    _browserEntries.clear();
    _browserCursor = 0;
    if (_browserDirectory.has_parent_path() &&
        _browserDirectory != _browserDirectory.root_path())
        _browserEntries.push_back({"..", true});

    std::error_code error;
    std::filesystem::directory_iterator iterator(_browserDirectory, error);
    if (error) {
        _character.say("folder error");
        return;
    }
    const std::filesystem::directory_iterator end;
    for (; iterator != end; iterator.increment(error)) {
        if (error) { _character.say("folder error"); break; }
        const auto& entry = *iterator;
        if (_browserEntries.size() >= 256) break;
        const std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        const bool directory = entry.is_directory(error);
        if (error) { error.clear(); continue; }
        if (!directory) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".wav") continue;
        }
        _browserEntries.push_back({name, directory});
    }
    const size_t first = !_browserEntries.empty() && _browserEntries[0].name == ".." ? 1 : 0;
    std::sort(_browserEntries.begin() + first, _browserEntries.end(),
              [](const BrowserEntry& a, const BrowserEntry& b) {
                  if (a.directory != b.directory) return a.directory;
                  return a.name < b.name;
              });
}
#endif

void SoundView::updateLoadBrowser(InputEvent event) {
#ifdef CARDPUTER_ZERO
    if (event == INPUT_ESC) {
        Audio::stopAll();
        SoundSlotOps::free(_previewSlot);
        _browserPreviewPath.clear();
        _subState = _browserReturnState;
        _character.setState(CHAR_IDLE);
        return;
    }
    if (event == INPUT_LEFT || event == INPUT_BACK) {
        Audio::stopAll();
        SoundSlotOps::free(_previewSlot);
        _browserPreviewPath.clear();
        if (_browserDirectory.has_parent_path() &&
            _browserDirectory != _browserDirectory.root_path()) {
            _browserDirectory = _browserDirectory.parent_path();
            refreshImportBrowser();
        }
        return;
    }
    if (event == INPUT_UP && _browserCursor > 0) --_browserCursor;
    if (event == INPUT_DOWN && _browserCursor + 1 < _browserEntries.size()) ++_browserCursor;
    if (_browserCursor >= _browserEntries.size()) return;

    const BrowserEntry& selected = _browserEntries[_browserCursor];
    if ((event == INPUT_ENTER || event == INPUT_RIGHT) && selected.directory) {
        Audio::stopAll();
        SoundSlotOps::free(_previewSlot);
        _browserPreviewPath.clear();
        _browserDirectory = selected.name == ".." ? _browserDirectory.parent_path()
                                                    : _browserDirectory / selected.name;
        refreshImportBrowser();
        return;
    }
    if (!selected.directory &&
        (event == INPUT_ENTER || event == INPUT_RIGHT || event == INPUT_SPACE)) {
        const std::string path = (_browserDirectory / selected.name).string();
        Audio::stopAll();
        SoundSlotOps::free(_previewSlot);
        _browserPreviewPath.clear();
        if (event == INPUT_SPACE) {
            if (Storage::loadWav(_previewSlot, path.c_str(), _project.bitDepth)) {
                Audio::triggerSound(_previewSlot);
                _browserPreviewPath = path;
                _browserPreviewStart = millis();
                const uint64_t audioMs = (uint64_t(_previewSlot.length) * 1000 +
                                          _previewSlot.sampleRate - 1) / _previewSlot.sampleRate;
                _browserPreviewDuration = audioMs < 300 ? 300 : static_cast<uint32_t>(audioMs);
            } else _character.say("can't preview");
        } else if (Storage::loadWav(_project.sounds[_cursor], path.c_str(), _project.bitDepth)) {
            _project.dirty = true;
            _character.setState(CHAR_SUCCESS);
            _character.say("nice one");
            _subState = STATE_LIST;
        } else {
            _character.setState(CHAR_ERROR);
            _character.say("import failed");
        }
    }
    return;
#else
    switch (event) {
        case INPUT_UP:
            if (_fileCursor > 0) _fileCursor--;
            break;
        case INPUT_DOWN:
            if (_fileCursor < _wavFileCount - 1) _fileCursor++;
            break;
        case INPUT_ENTER: {
            Audio::stopAll();
            SoundSlotOps::free(_previewSlot);
            char path[64];
            snprintf(path, sizeof(path), "/beepbotdx/samples/%s", _wavFiles[_fileCursor]);
            if (Storage::loadWav(
                    _project.sounds[_cursor], path, _project.bitDepth)) {
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
            Audio::stopAll();
            SoundSlotOps::free(_previewSlot);
            if (Storage::loadWav(_previewSlot, path, _project.bitDepth)) {
                Audio::triggerSound(_previewSlot);
                _character.setState(CHAR_IDLE);
            }
            break;
        }
        case INPUT_ESC:
            Audio::stopAll();
            SoundSlotOps::free(_previewSlot);
            _subState = STATE_RECORD_READY;
            _character.setState(CHAR_IDLE);
            { char memMsg[16]; snprintf(memMsg, sizeof(memMsg), "%luKB free", (unsigned long)(Memory::getFree() / 1024)); _character.say(memMsg); }
            break;
        default:
            break;
    }
#endif
}

void SoundView::updateRename(InputEvent event) {
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
}

// --- Draw methods ---

void SoundView::drawList(Canvas& canvas, const Theme& theme) {
    GridLayout grid = GridLayout::make(4, 2, 22, 3, 3, 12);

    for (int i = 0; i < NUM_SOUNDS; i++) {
        int x, y;
        grid.cellXY(i, x, y);

        bool selected = (i == _cursor);
        bool occupied = _project.sounds[i].occupied;
        bool flashing = (_flashSlot == i && (millis() - _flashTime) < 150);

        uint16_t bgColor;
        uint16_t textColor;
        if (flashing) { bgColor = TFT_WHITE; textColor = TFT_BLACK; }
        else if (selected) { bgColor = theme.accent; textColor = TFT_BLACK; }
        else { bgColor = theme.dark; textColor = occupied ? theme.accent : theme.dim; }
        canvas.fillRect(x, y, grid.cellW, grid.cellH, bgColor);

        canvas.setTextDatum(top_left);
        canvas.setTextColor(textColor);
        char numStr[4];
        snprintf(numStr, sizeof(numStr), "%02d", i + 1);
        canvas.drawString(numStr, x + 4, y + 4);
        if (occupied && SlotFxOps::anyActive(_project.sounds[i].fx)) {
            canvas.setTextDatum(top_right);
            canvas.drawString("FX", x + grid.cellW - 4, y + 4);
            canvas.setTextDatum(top_left);
        }
        if (occupied) canvas.drawString(_project.sounds[i].name, x + 4, y + 14);
    }

    if (_statusMsg[0] && (millis() - _statusTime < 3000)) {
        canvas.setTextColor(theme.highlight);
        canvas.setTextDatum(top_right);
        canvas.drawString(_statusMsg, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 10);
    }

    if (_project.sounds[_cursor].occupied) {
        canvas.setTextDatum(top_left);
        canvas.setTextColor(_project.sounds[_cursor].normalize ? theme.accent : theme.dim);
        canvas.drawString("N", 4, SCREEN_HEIGHT - 10);
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
        SoundSlot& slot = _project.sounds[_cursor];
        char msg[32];
        if (slot.name[0]) {
            snprintf(msg, sizeof(msg), "DELETE \"%s\"?", slot.name);
        } else {
            snprintf(msg, sizeof(msg), "DELETE SOUND %d?", _cursor + 1);
        }
        canvas.drawString(msg, boxX + boxW / 2, boxY + 16);

        canvas.setTextColor(theme.accent);
        canvas.drawString("YES:OK  NO:ESC", boxX + boxW / 2, boxY + 36);
    }
}

void SoundView::drawRecordReady(Canvas& canvas, const Theme& theme) {
    const int margin = 3;
    const int gap = 3;
    const int top = 36;
    const int boxH = SCREEN_HEIGHT - top - margin;
    const int totalW = SCREEN_WIDTH - margin * 2;
    const int boxW = (totalW - gap) / 2;
    int leftX = margin;
    int rightX = margin + boxW + gap;

    canvas.fillRect(leftX, top, boxW, boxH,
                    _recordReadySelection == 0 ? theme.accent : theme.dark);
    canvas.fillRect(rightX, top, boxW, boxH,
                    _recordReadySelection == 1 ? theme.accent : theme.dark);

    canvas.setTextDatum(top_center);
    int textY = top + (boxH - 8) / 2;

    const uint16_t recordColor = _recordReadySelection == 0 ? TFT_BLACK : theme.accent;
    const uint16_t importColor = _recordReadySelection == 1 ? TFT_BLACK : theme.accent;
    canvas.setTextColor(recordColor);
    canvas.drawString("RECORD", leftX + boxW / 2, textY);
    canvas.fillRect(leftX + boxW / 2 - 18, textY + 8, 5, 1, recordColor);

    canvas.setTextColor(importColor);
    canvas.drawString("IMPORT", rightX + boxW / 2, textY);
    canvas.fillRect(rightX + boxW / 2 - 18, textY + 8, 5, 1, importColor);
}

void SoundView::drawCountdown(Canvas& canvas, const Theme& theme) {
    const int startY = 24;
    uint8_t count = 3 - _countdownBeat;
    char countStr[2] = { (char)('0' + count), '\0' };
    canvas.setTextSize(3);
    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_center);
    canvas.drawString(countStr, SCREEN_WIDTH / 2, startY + (SCREEN_HEIGHT - startY - 21) / 2 - 8);
    canvas.setTextSize(1);
}

void SoundView::drawRecording(Canvas& canvas, const Theme& theme) {
    const int startY = 24;
    SoundSlot& slot = _project.sounds[_cursor];
    uint32_t recorded = Audio::getRecordedLength();
    uint8_t level = computeAudioLevel(slot, recorded);

    float progress = _recordMaxLength > 0 ? (float)recorded / _recordMaxLength : 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    if (level > 10) {
        uint8_t boosted = level < 85 ? level * 3 : 255;
        BloomFieldOps::injectAt(_bloom, boosted, BLOOM_COLS / 3, BLOOM_ROWS * 2 / 5);
        BloomFieldOps::injectAt(_bloom, boosted * 4 / 5, BLOOM_COLS * 2 / 3, BLOOM_ROWS * 3 / 5);
        BloomFieldOps::injectAt(_bloom, boosted * 3 / 4, BLOOM_COLS / 5, BLOOM_ROWS * 3 / 4);
    }

    BloomFieldOps::tick(_bloom, millis());

    float seconds = (float)recorded / SAMPLE_RATE;
    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_left);
    canvas.drawString("RECORDING", 7, startY);



    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%.1fs", seconds);
    canvas.setTextDatum(top_right);
    canvas.drawString(timeStr, SCREEN_WIDTH - 8, startY);

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
}

void SoundView::drawRecordDone(Canvas& canvas, const Theme& theme) {
    const int startY = 24;
    SoundSlot& slot = _project.sounds[_cursor];
    float seconds = (float)slot.length / SAMPLE_RATE;

    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_left);
    canvas.drawString("RECORDING", 7, startY);

    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%.1fs", seconds);
    canvas.setTextDatum(top_right);
    canvas.drawString(timeStr, SCREEN_WIDTH - 8, startY);

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
}

void SoundView::drawTrim(Canvas& canvas, const Theme& theme) {
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

    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_left);
    canvas.drawString("TRIM", hdrLeft + 4, infoY);

    char durStr[10];
    snprintf(durStr, sizeof(durStr), "%.2fs", durSec);
    canvas.setTextDatum(top_center);
    canvas.drawString(durStr, hdrLeft + hdrContentW / 2, infoY);

    char lvlStr[8];
    snprintf(lvlStr, sizeof(lvlStr), "LVL:%d", slot.level);
    canvas.setTextDatum(top_right);
    canvas.drawString(lvlStr, hdrLeft + hdrContentW - 4, infoY);

    const int waveTop = infoY + 12;
    const int waveBottom = SCREEN_HEIGHT - 14;
    drawWaveform(canvas, theme, hdrLeft + 4, waveTop, hdrContentW - 8, waveBottom - waveTop);

    char startStr[10];
    snprintf(startStr, sizeof(startStr), "%.2fs", startSec);
    canvas.setTextColor(_trimMovingEnd ? theme.dim : theme.accent);
    canvas.setTextDatum(top_left);
    canvas.drawString(startStr, hdrLeft + 4, waveBottom + 2);

    canvas.setTextColor(theme.dim);
    canvas.setTextDatum(top_center);
#ifdef CARDPUTER_ZERO
    canvas.drawString("E:FX", hdrLeft + hdrContentW / 2 - 24, waveBottom + 2);
#else
    canvas.drawString("FX", hdrLeft + hdrContentW / 2 - 24, waveBottom + 2);
#endif
    canvas.setTextDatum(top_left);
    canvas.setTextColor(slot.normalize ? theme.accent : theme.dim);
    canvas.drawString("N", hdrLeft + hdrContentW / 2, waveBottom + 2);

    char endStr[10];
    snprintf(endStr, sizeof(endStr), "%.2fs", endSec);
    canvas.setTextColor(_trimMovingEnd ? theme.accent : theme.dim);
    canvas.setTextDatum(top_right);
    canvas.drawString(endStr, hdrLeft + hdrContentW - 4, waveBottom + 2);
}

void SoundView::drawFx(Canvas& canvas, const Theme& theme) {
    SoundSlot& slot = _project.sounds[_cursor];

    const int margin = 3;
    const int hdrGridW = SCREEN_WIDTH - margin * 2;
    const int hdrCellW = (hdrGridW - margin * 3) / 4;
    const int hdrContentW = hdrCellW * 4 + margin * 3;
    const int hdrLeft = margin + (hdrGridW - hdrContentW) / 2;
    const int infoY = 23;

    canvas.setTextColor(theme.accent);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    canvas.drawString("FX", hdrLeft + 4, infoY);

    char lvlStr[8];
    snprintf(lvlStr, sizeof(lvlStr), "LVL:%d", slot.level);
    canvas.setTextDatum(top_right);
    canvas.drawString(lvlStr, hdrLeft + hdrContentW - 4, infoY);

    static const char* labels[NUM_FX] = {"PITCH", "CRUSH", "LPF", "HPF"};
    const int knobR = 16;
    const int tileH = 4 + 10 + 10 + knobR * 2 + 10; // pad + label + value + knob + pad
    const int gridW = SCREEN_WIDTH - margin * 2;
    const int spacing = 3;
    const int cellW = (gridW - spacing * 3) / 4;
    const int contentW = cellW * 4 + spacing * 3;
    const int gridLeft = margin + (gridW - contentW) / 2;
    const int footerY = SCREEN_HEIGHT - 12;
    const int availH = footerY - 34;
    const int gridTop = 34 + (availH - tileH) / 2 - 4;

    for (int i = 0; i < NUM_FX; i++) {
        int x = gridLeft + i * (cellW + spacing);
        int y = gridTop;
        bool focused = (i == _fxCursor);
        bool enabled = slot.fx.enabled[i];

        uint16_t bgColor = focused ? theme.accent : theme.dark;
        canvas.fillRect(x, y, cellW, tileH, bgColor);

        // Label top-left of tile
        uint16_t textColor;
        if (focused) textColor = enabled ? TFT_BLACK : theme.dim;
        else textColor = enabled ? theme.accent : theme.dim;
        canvas.setTextColor(textColor);
        canvas.setTextDatum(top_left);
        canvas.drawString(labels[i], x + 4, y + 4);

        // Value below label
        char valStr[8];
        if (i == FX_PITCH) {
            int semi = (int)slot.fx.value[i] - 12;
            if (semi > 0) snprintf(valStr, sizeof(valStr), "+%d", semi);
            else snprintf(valStr, sizeof(valStr), "%d", semi);
        } else {
            snprintf(valStr, sizeof(valStr), "%d", slot.fx.value[i]);
        }
        canvas.drawString(valStr, x + 4, y + 14);

        // Knob centered below text
        int cx = x + cellW / 2;
        int cy = y + 4 + 10 + 10 + knobR;
        drawKnob(canvas, cx, cy, knobR, slot.fx.value[i], SlotFxOps::maxValue(i), enabled, focused, theme);
    }

    canvas.setTextSize(1);
    canvas.setTextColor(theme.dim);
    canvas.setTextDatum(top_left);
    canvas.drawString("TOGGLE:OK", hdrLeft + 4, footerY);
    canvas.setTextDatum(top_center);
    canvas.drawString("T:TRIM", hdrLeft + hdrContentW / 2 - 24, footerY);
    canvas.setTextColor(slot.normalize ? theme.accent : theme.dim);
    canvas.setTextDatum(top_left);
    canvas.drawString("N", hdrLeft + hdrContentW / 2, footerY);
    canvas.setTextColor(theme.dim);
    canvas.setTextDatum(top_right);
    canvas.drawString("RESET:DEL", hdrLeft + hdrContentW - 4, footerY);
}

void SoundView::drawKnob(Canvas& canvas, int cx, int cy, int r,
                          uint8_t value, uint8_t maxVal, bool enabled, bool focused,
                          const Theme& theme) {
    int scale = r >= 24 ? 3 : 2;
    const float startAngle = 2.356f;  // 135 deg (7 o'clock)
    const float endAngle = 7.069f;    // 405 deg (5 o'clock)
    const float sweep = endAngle - startAngle;

    float angle = startAngle + ((float)value / maxVal) * sweep;

    uint16_t ringColor, dotColor;
    if (focused) {
        ringColor = enabled ? TFT_BLACK : theme.dim;
        dotColor = ringColor;
    } else if (enabled) {
        ringColor = theme.accent;
        dotColor = theme.accent;
    } else {
        ringColor = theme.dim;
        dotColor = theme.dim;
    }

    // Draw at half res (17x17) then blit at 2x for chunky pixels
    const int hr = 7;  // radius in half-res space
    const int sz = 17;
    const int ctr = 8; // true center pixel
    uint8_t buf[17][17] = {};  // 0=bg, 1=ring, 2=indicator

    // Ring (midpoint circle at half res, 2px thick)
    for (int tr = hr; tr >= hr - 1; tr--) {
        int px = tr, py = 0, d = 1 - tr;
        while (px >= py) {
            buf[ctr + py][ctr + px] = 1; buf[ctr - py][ctr + px] = 1;
            buf[ctr + py][ctr - px] = 1; buf[ctr - py][ctr - px] = 1;
            buf[ctr + px][ctr + py] = 1; buf[ctr - px][ctr + py] = 1;
            buf[ctr + px][ctr - py] = 1; buf[ctr - px][ctr - py] = 1;
            py++;
            if (d <= 0) { d += 2 * py + 1; }
            else { px--; d += 2 * (py - px) + 1; }
        }
    }

    // Indicator line (1px wide, from r=2 to r=7)
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    int innerR = 2;
    for (int d = innerR; d <= hr; d++) {
        int px1 = ctr + (int)(d * cosA);
        int py1 = ctr + (int)(d * sinA);
        if (px1 >= 0 && px1 < sz && py1 >= 0 && py1 < sz)
            buf[py1][px1] = 2;
    }

    // Blit at scale (17x17 -> scaled output)
    int halfOut = (sz * scale) / 2;
    int ox = cx - halfOut;
    int oy = cy - halfOut;
    for (int py2 = 0; py2 < sz; py2++) {
        for (int px2 = 0; px2 < sz; px2++) {
            if (buf[py2][px2] == 0) continue;
            uint16_t c = (buf[py2][px2] == 2) ? dotColor : ringColor;
            int dx = ox + px2 * scale;
            int dy = oy + py2 * scale;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    canvas.drawPixel(dx + sx, dy + sy, c);
        }
    }
}

void SoundView::drawLoadBrowser(Canvas& canvas, const Theme& theme) {
#ifdef CARDPUTER_ZERO
    std::string folder = _browserDirectory.string();
    if (folder.size() > 49) folder = "..." + folder.substr(folder.size() - 46);
    canvas.setTextColor(theme.dim);
    canvas.setTextDatum(top_left);
    canvas.drawString(folder.empty() ? "/" : folder.c_str(), 12, 23);

    const int listTop = 37;
    const int itemH = 10;
    const int visibleItems = (SCREEN_HEIGHT - listTop - 12) / itemH;
    const int scrollOffset = _browserCursor >= static_cast<size_t>(visibleItems)
        ? static_cast<int>(_browserCursor) - visibleItems + 1 : 0;
    if (_browserEntries.empty() ||
        (_browserEntries.size() == 1 && _browserEntries[0].name == "..")) {
        canvas.drawString("NO WAV FILES", 12,
                          listTop + (_browserEntries.empty() ? 0 : itemH));
    }
    for (int i = scrollOffset;
         i < static_cast<int>(_browserEntries.size()) && i - scrollOffset < visibleItems; ++i) {
        const BrowserEntry& entry = _browserEntries[i];
        const int y = listTop + (i - scrollOffset) * itemH;
        canvas.setTextColor(i == static_cast<int>(_browserCursor) ? TFT_WHITE :
                            entry.directory ? theme.dim : theme.accent);
        if (i == static_cast<int>(_browserCursor)) canvas.drawString(">", 4, y);
        std::string label = entry.name + (entry.directory && entry.name != ".." ? "/" : "");
        if (label.size() > 49) label = label.substr(0, 46) + "...";
        canvas.drawString(label.c_str(), 12, y);
        if (!entry.directory && _browserPreviewDuration > 0 &&
            _browserDirectory / entry.name == _browserPreviewPath) {
            const uint32_t elapsed = millis() - _browserPreviewStart;
            if (elapsed < _browserPreviewDuration) {
                const int sweepLeft = 12;
                const int sweepWidth = SCREEN_WIDTH - sweepLeft - 8;
                const int playX = sweepLeft + static_cast<int>(
                    uint64_t(elapsed) * (sweepWidth - 1) / _browserPreviewDuration);
                canvas.fillRect(playX, y, 2, 8, TFT_WHITE);
            }
        }
    }
    canvas.setTextColor(theme.dim);
    canvas.drawString("LEFT: UP  SPACE: PREVIEW", 12, SCREEN_HEIGHT - 11);
    return;
#else
    const int startY = 24;
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

        canvas.setTextColor(i == _fileCursor ? TFT_WHITE : theme.accent);
        if (i == _fileCursor) canvas.drawString(">", 4, y);
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
#endif
}

void SoundView::drawRename(Canvas& canvas, const Theme& theme) {
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
    canvas.drawString("CONFIRM:OK  CANCEL:ESC", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 14);
    canvas.setTextDatum(top_left);
}

// --- Helper methods ---

void SoundView::drawWaveform(Canvas& canvas, const Theme& theme, int x, int y, int w, int h) {
    SoundSlot& slot = _project.sounds[_cursor];
    if (!slot.samples || slot.length == 0) return;

    int midY = y + h / 2;

    // Use a fixed full-scale range so playback boosts are visible instead of
    // automatically stretching every sample to the same display height.
    float levelScale = slot.level / 100.0f;
    for (int px = 0; px < w; px += 2) {
        uint32_t sampleIdx = (uint32_t)((float)px / w * slot.length);
        int32_t sampleVal = SoundSlotOps::getPlaybackSample(slot, sampleIdx);
        int32_t magnitude = sampleVal < 0 ? -sampleVal : sampleVal;
        int barH = (int)((float)magnitude / 32768.0f * (h / 2) * levelScale);
        if (barH < 1) barH = 1;

        uint16_t color;
        if (sampleIdx >= _trimStart && sampleIdx <= _trimEnd) {
            color = theme.accent;
        } else {
            color = theme.dark;
        }

        canvas.fillRect(x + px, midY - barH, 2, barH * 2, color);
    }

    int startPx = x + (int)((float)_trimStart / slot.length * w);
    int markerH = 36;
    int markerY = midY - markerH / 2 + 1;
    uint16_t startColor = _trimMovingEnd ? theme.dim : TFT_WHITE;
    for (int dy = 0; dy < markerH; dy += 4) {
        canvas.fillRect(startPx, markerY + dy, 2, 2, startColor);
    }

    int endPx = x + (int)((float)_trimEnd / slot.length * w);
    uint16_t endColor = _trimMovingEnd ? TFT_WHITE : theme.dim;
    for (int dy = 0; dy < markerH; dy += 4) {
        canvas.fillRect(endPx, markerY + dy, 2, 2, endColor);
    }

    if (_playbackActive && _playbackRate > 0) {
        const uint64_t audioDurationMs =
            (uint64_t(_playbackLength) * 1000 + _playbackRate - 1) / _playbackRate;
        const uint64_t visualDurationMs = audioDurationMs < 300 ? 300 : audioDurationMs;
        const uint32_t elapsedMs = millis() - _playbackStart;
        if (elapsedMs >= visualDurationMs) {
            _playbackActive = false;
        } else {
            const uint32_t position = _playbackOffset +
                static_cast<uint32_t>(uint64_t(elapsedMs) * _playbackLength / visualDurationMs);
            const int playX = x + static_cast<int>(uint64_t(position) * (w - 1) / slot.length);
            canvas.fillRect(playX, y, 2, h, TFT_WHITE);
        }
    }
}

void SoundView::startRecording() {
    SoundSlot& slot = _project.sounds[_cursor];
    Audio::stopAll();
    _recordMaxLength = SoundSlotOps::availableSamples(_project.bitDepth, &slot);
#ifndef CARDPUTER_ZERO
    uint32_t formatMaximum = MAX_SAMPLE_LENGTH *
        (_project.bitDepth == BIT_DEPTH_8 ? 2 : 1);
    if (_recordMaxLength > formatMaximum) _recordMaxLength = formatMaximum;
#endif
    if (_recordMaxLength < SAMPLE_RATE / 10 ||
        !SoundSlotOps::allocate(slot, _recordMaxLength, _project.bitDepth)) {
        _character.setState(CHAR_DEAD);
        _character.say("no memory!");
        snprintf(_statusMsg, sizeof(_statusMsg), "NO MEMORY");
        _statusTime = millis();
        return;
    }
    Audio::recordStart(slot.samples, _recordMaxLength, slot.bitDepth);
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
        SoundSlotOps::shrinkToFit(slot);
        char defaultName[9];
        snprintf(defaultName, sizeof(defaultName), "REC%d", _cursor + 1);
        SoundSlotOps::setName(slot, defaultName);
        _project.dirty = true;

        _subState = STATE_RECORD_DONE;
        _recordDoneTime = millis();
        _character.setState(CHAR_SUCCESS);
        _character.say("got it!");
    } else {
        SoundSlotOps::free(slot);
        _character.setState(CHAR_ERROR);
        _character.say("nothing?");
        _subState = STATE_LIST;
    }
}

void SoundView::applyTrim() {
    SoundSlot& slot = _project.sounds[_cursor];
    Audio::stopAll();
    uint32_t newLen = _trimEnd - _trimStart;

    if (_trimStart > 0 && newLen > 0) {
        uint8_t bytes = SoundSlotOps::bytesPerSample(slot.bitDepth);
        memmove(slot.samples,
                reinterpret_cast<uint8_t*>(slot.samples) + _trimStart * bytes,
                newLen * bytes);
    }

    slot.length = newLen;
    SoundSlotOps::updateNormalizationGain(slot);
    _project.dirty = true;

    SoundSlotOps::shrinkToFit(slot);

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
    Audio::triggerSound(slot, slot.level * 255 / 100, &slot.fx);
    _character.setState(CHAR_BEAT);
    _flashSlot = index;
    _flashTime = millis();
}

void SoundView::toggleNormalize() {
    SoundSlot& slot = _project.sounds[_cursor];
    if (!slot.occupied) return;
    Audio::stopAll();
    _playbackActive = false;
    slot.normalize = !slot.normalize;
    SoundSlotOps::updateNormalizationGain(slot);
    _project.dirty = true;
    _character.say(slot.normalize ? "normalize on" : "normalize off");
}
