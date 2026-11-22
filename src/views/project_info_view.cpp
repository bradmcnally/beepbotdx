#include "project_info_view.h"
#include "platform/memory.h"
#include "platform/led.h"
#include "platform/input.h"
#include "views/settings_view.h"
#include "core/theme.h"
#include "core/timing.h"
#include "config.h"
#include <cstdio>
#include <cstring>

#ifndef CARDPUTER_ZERO
static const uint8_t NUM_SETTINGS = 2;
static const uint8_t TOTAL_ROWS = NUM_SETTINGS + 1 + NUM_SOUNDS;
#endif

ProjectInfoView::ProjectInfoView(Project& project, Character& character)
    : _project(project), _character(character) {}

void ProjectInfoView::enter() {
    _cursor = 0;
    _closeRequested = false;
    _saveRequested = false;
    _exportRequested = false;
    _editingName = false;
    _actionFeedback = 0xFF;
}

void ProjectInfoView::showActionResult(bool exporting, bool success) {
    _actionFeedback = exporting ? 1 : 0;
    _actionFeedbackTime = millis();
    _actionSucceeded = success;
}

void ProjectInfoView::update(InputEvent event) {
#ifdef CARDPUTER_ZERO
    if (_editingName) {
        switch (event) {
            case INPUT_ENTER:
                if (strcmp(_project.name, _renameBuffer) != 0) {
                    strcpy(_project.name, _renameBuffer);
                    _project.dirty = true;
                }
                _editingName = false;
                _character.say("renamed!");
                break;
            case INPUT_ESC:
                _editingName = false;
                break;
            case INPUT_BACK:
                if (_renameLength > 0) _renameBuffer[--_renameLength] = '\0';
                break;
            case INPUT_SPACE:
                if (_renameLength < 8) {
                    _renameBuffer[_renameLength++] = ' ';
                    _renameBuffer[_renameLength] = '\0';
                }
                break;
            case INPUT_CHAR: {
                const char ch = Input::getChar();
                if (_renameLength < 8 && ch >= ' ' && ch <= '~') {
                    _renameBuffer[_renameLength++] = ch;
                    _renameBuffer[_renameLength] = '\0';
                }
                break;
            }
            case INPUT_NUM1: case INPUT_NUM2: case INPUT_NUM3:
            case INPUT_NUM4: case INPUT_NUM5: case INPUT_NUM6:
            case INPUT_NUM7: case INPUT_NUM8: case INPUT_NUM9:
                if (_renameLength < 8) {
                    _renameBuffer[_renameLength++] = '1' + (event - INPUT_NUM1);
                    _renameBuffer[_renameLength] = '\0';
                }
                break;
            case INPUT_NUM0:
                if (_renameLength < 8) {
                    _renameBuffer[_renameLength++] = '0';
                    _renameBuffer[_renameLength] = '\0';
                }
                break;
            default: break;
        }
        return;
    }

    switch (event) {
        case INPUT_UP:
            if (_cursor > 0) _cursor = _cursor >= 4 ? 3 : _cursor - 1;
            break;
        case INPUT_DOWN:
            if (_cursor < 4) ++_cursor;
            break;
        case INPUT_LEFT:
        case INPUT_RIGHT:
            if (_cursor == 1) {
                _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS +
                    (event == INPUT_LEFT ? -1 : 1)) % ThemeOps::NUM_PRESETS;
                _project.dirty = true;
                if (GlobalSettings::instance && ledShowsIdleColor(GlobalSettings::instance->ledMode)) {
                    uint8_t r, g, b;
                    ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
                    LED::setColor(r, g, b);
                }
            } else if (_cursor == 2) {
                _project.bitDepth = _project.bitDepth == BIT_DEPTH_16 ? BIT_DEPTH_8 : BIT_DEPTH_16;
                _project.dirty = true;
            } else if (_cursor == 3) {
                if (event == INPUT_LEFT && _project.bpm > MIN_BPM) {
                    --_project.bpm;
                    _project.dirty = true;
                }
                if (event == INPUT_RIGHT && _project.bpm < MAX_BPM) {
                    ++_project.bpm;
                    _project.dirty = true;
                }
            } else if (_cursor >= 4) {
                _cursor = event == INPUT_LEFT ? 4 : 5;
            }
            break;
        case INPUT_ENTER:
            if (_cursor == 0) {
                strcpy(_renameBuffer, _project.name);
                _renameLength = strlen(_renameBuffer);
                _editingName = true;
            } else if (_cursor == 1) {
                _project.themeIndex = (_project.themeIndex + 1) % ThemeOps::NUM_PRESETS;
                _project.dirty = true;
                if (GlobalSettings::instance && ledShowsIdleColor(GlobalSettings::instance->ledMode)) {
                    uint8_t r, g, b;
                    ThemeOps::getPresetRGB(_project.themeIndex, r, g, b);
                    LED::setColor(r, g, b);
                }
            } else if (_cursor == 2) {
                _project.bitDepth = _project.bitDepth == BIT_DEPTH_16 ? BIT_DEPTH_8 : BIT_DEPTH_16;
                _project.dirty = true;
            } else if (_cursor == 3) {
                if (_project.bpm < MAX_BPM) {
                    ++_project.bpm;
                    _project.dirty = true;
                }
            } else if (_cursor == 4) {
                _saveRequested = true;
            } else {
                _exportRequested = true;
            }
            break;
        case INPUT_BACK:
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default: break;
    }
#else
    switch (event) {
        case INPUT_UP:
            if (_cursor > 0) {
                _cursor--;
                if (_cursor == NUM_SETTINGS) _cursor--;
            }
            break;
        case INPUT_DOWN:
            if (_cursor < TOTAL_ROWS - 1) {
                _cursor++;
                if (_cursor == NUM_SETTINGS) _cursor++;
            }
            break;
        case INPUT_LEFT:
            if (_cursor == 0) {
                _project.themeIndex = (_project.themeIndex + ThemeOps::NUM_PRESETS - 1) % ThemeOps::NUM_PRESETS;
                _project.dirty = true;
                if (ledShowsIdleColor(GlobalSettings::instance->ledMode)) {
                    uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b);
                }
            } else if (_cursor == 1) {
                _project.bitDepth = _project.bitDepth == BIT_DEPTH_16
                    ? BIT_DEPTH_8 : BIT_DEPTH_16;
                _project.dirty = true;
            }
            break;
        case INPUT_RIGHT:
        case INPUT_ENTER:
            if (_cursor == 0) {
                _project.themeIndex = (_project.themeIndex + 1) % ThemeOps::NUM_PRESETS;
                _project.dirty = true;
                if (ledShowsIdleColor(GlobalSettings::instance->ledMode)) {
                    uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b);
                }
            } else if (_cursor == 1) {
                _project.bitDepth = _project.bitDepth == BIT_DEPTH_16
                    ? BIT_DEPTH_8 : BIT_DEPTH_16;
                _project.dirty = true;
            }
            break;
        case INPUT_BACK:
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default:
            break;
    }
#endif
}

void ProjectInfoView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

#ifdef CARDPUTER_ZERO
    const char* labels[] = {"NAME", "COLOR", "NEW SAMPLES", "BPM"};
    char value[32];
    for (uint8_t row = 0; row < 4; ++row) {
        const int y = 24 + row * 16;
        const bool selected = _cursor == row;
        if (selected) canvas.fillRect(3, y, SCREEN_WIDTH - 6, 14, theme.accent);
        canvas.setTextColor(selected ? TFT_BLACK : theme.accent);
        canvas.drawString(labels[row], 7, y + 3);

        if (row == 0) {
            snprintf(value, sizeof(value), "%s", _project.name[0] ? _project.name : "UNTITLED");
        } else if (row == 1) {
            snprintf(value, sizeof(value), "< %s >", ThemeOps::getPresetName(_project.themeIndex));
        } else if (row == 2) {
            snprintf(value, sizeof(value), "< %s >",
                     _project.bitDepth == BIT_DEPTH_8 ? "8-BIT" : "16-BIT");
        } else {
            snprintf(value, sizeof(value), "< %u >", static_cast<unsigned>(_project.bpm));
        }
        canvas.setTextColor(selected ? TFT_BLACK : theme.dim);
        canvas.drawString(value, 140, y + 3);
        if (row == 0 && _project.dirty) canvas.drawString("*", SCREEN_WIDTH - 13, y + 3);
    }

    uint8_t soundCount = 0;
    uint8_t patternCount = 0;
    for (int i = 0; i < NUM_SOUNDS; ++i)
        if (_project.sounds[i].occupied) ++soundCount;
    for (int i = 0; i < NUM_PATTERNS; ++i) {
        for (int step = 0; step < NUM_STEPS; ++step) {
            if (_project.patterns[i].steps[step]) { ++patternCount; break; }
        }
    }

    canvas.drawFastHLine(7, 94, SCREEN_WIDTH - 14, theme.dim);
    canvas.setTextColor(theme.dim);
    snprintf(value, sizeof(value), "SOUNDS %u/%u", soundCount, NUM_SOUNDS);
    canvas.drawString(value, 7, 102);
    snprintf(value, sizeof(value), "PATTERNS %u/%u", patternCount, NUM_PATTERNS);
    canvas.setTextDatum(top_right);
    canvas.drawString(value, SCREEN_WIDTH - 7, 102);
    canvas.setTextDatum(top_left);

    const uint32_t used = SoundSlotOps::usedBytes();
    const uint32_t capacity = Memory::getSampleBudget();
    snprintf(value, sizeof(value), "SAMPLE MEMORY %.1f/%.1f MiB",
             used / 1048576.0f, capacity / 1048576.0f);
    canvas.drawString(value, 7, 118);
    canvas.fillRect(7, 130, SCREEN_WIDTH - 14, 4, theme.dark);
    if (capacity > 0)
        canvas.fillRect(7, 130, static_cast<int>(uint64_t(used) * (SCREEN_WIDTH - 14) / capacity),
                        4, theme.accent);

    const int actionY = SCREEN_HEIGHT - 27;
    const int actionW = (SCREEN_WIDTH - 9) / 2;
    for (uint8_t action = 0; action < 2; ++action) {
        const int x = 3 + action * (actionW + 3);
        const bool selected = _cursor == action + 4;
        const bool feedback = _actionFeedback == action &&
                              millis() - _actionFeedbackTime < 700;
        canvas.fillRect(x, actionY, actionW, 24,
                        feedback ? TFT_WHITE : selected ? theme.accent : theme.dark);
        canvas.setTextColor(feedback || selected ? TFT_BLACK : theme.accent);
        canvas.setTextDatum(top_center);
        const char* label = feedback
            ? (_actionSucceeded ? (action == 0 ? "SAVED" : "EXPORTED")
                                : (action == 0 ? "SAVE FAILED" : "EXPORT FAILED"))
            : (action == 0 ? "SAVE" : "EXPORT WAV");
        canvas.drawString(label, x + actionW / 2, actionY + 8);
    }

    if (_editingName) {
        canvas.darken();
        const int x = (SCREEN_WIDTH - 190) / 2;
        const int y = (SCREEN_HEIGHT - 52) / 2;
        canvas.fillRect(x, y, 190, 52, TFT_BLACK);
        canvas.drawRect(x, y, 190, 52, theme.accent);
        canvas.setTextColor(theme.accent);
        canvas.setTextDatum(top_center);
        canvas.drawString("PROJECT NAME", SCREEN_WIDTH / 2, y + 8);
        char editing[12];
        snprintf(editing, sizeof(editing), "%s%s", _renameBuffer,
                 (millis() / 400) % 2 == 0 ? "_" : " ");
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString(editing, SCREEN_WIDTH / 2, y + 27);
    }
#else
    const int lineH = 14;
    const int startY = 24;
    const int listTop = startY + 4;
    const int listBottom = SCREEN_HEIGHT - 4;
    const int visibleItems = (listBottom - listTop) / lineH;
    const int labelX = 7;
    const int valueX = 140;

    const int drawItems = (listBottom - listTop + lineH - 1) / lineH;

    int scrollOffset = 0;
    if (_cursor >= visibleItems) {
        scrollOffset = _cursor - visibleItems + 1;
    }

    uint8_t bytesPerSample = SoundSlotOps::bytesPerSample(_project.bitDepth);
    uint32_t available = Memory::getFree();
    float availSecs = (float)available / (SAMPLE_RATE * bytesPerSample);
    uint32_t usedBytes = 0;
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (_project.sounds[i].occupied)
            usedBytes += SoundSlotOps::allocatedBytes(_project.sounds[i]);
    }

    char buf[40];

    for (int i = scrollOffset; i < TOTAL_ROWS && (i - scrollOffset) < drawItems; i++) {
        int y = listTop + (i - scrollOffset) * lineH;
        bool selected = (i == _cursor);

        if (i < NUM_SETTINGS) {
            const char* label = i == 0 ? "COLOR" : "NEW SAMPLES";
            const char* value = i == 0
                ? ThemeOps::getPresetName(_project.themeIndex)
                : (_project.bitDepth == BIT_DEPTH_8 ? "8-BIT" : "16-BIT");

            canvas.setTextColor(selected ? TFT_WHITE : theme.accent);
            canvas.drawString(label, labelX, y);

            canvas.setTextColor(selected ? TFT_WHITE : theme.dim);
            if (selected) {
                snprintf(buf, sizeof(buf), "< %s >", value);
                canvas.drawString(buf, valueX - 12, y);
            } else {
                canvas.drawString(value, valueX, y);
            }
        } else if (i == NUM_SETTINGS) {
            canvas.setTextColor(theme.dim);
            snprintf(buf, sizeof(buf), "%luKB/%luKB  %.1fs remaining",
                (unsigned long)(usedBytes / 1024), (unsigned long)((usedBytes + available) / 1024), availSecs);
            canvas.drawString(buf, labelX, y);
        } else {
            int slot = i - NUM_SETTINGS - 1;
            if (_project.sounds[slot].occupied) {
                float secs = (float)_project.sounds[slot].length / SAMPLE_RATE;
                uint32_t kb = SoundSlotOps::allocatedBytes(_project.sounds[slot]) / 1024;
                canvas.setTextColor(selected ? TFT_WHITE : theme.accent);
                snprintf(buf, sizeof(buf), "%d. %-8s", slot + 1, _project.sounds[slot].name);
                canvas.drawString(buf, labelX, y);
                canvas.setTextColor(selected ? TFT_WHITE : theme.dim);
                snprintf(buf, sizeof(buf), "%.2fs %db", secs,
                    _project.sounds[slot].bitDepth == BIT_DEPTH_8 ? 8 : 16);
                canvas.drawString(buf, 120, y);
                snprintf(buf, sizeof(buf), "%luKB", (unsigned long)kb);
                canvas.drawString(buf, 180, y);
            } else {
                canvas.setTextColor(selected ? TFT_WHITE : theme.dim);
                snprintf(buf, sizeof(buf), "%d. --", slot + 1);
                canvas.drawString(buf, labelX, y);
            }
        }
    }

    if (TOTAL_ROWS > visibleItems) {
        int maxScroll = TOTAL_ROWS - visibleItems;
        const int trackTop = listTop;
        const int trackH = listBottom - listTop;
        int thumbH = trackH * visibleItems / TOTAL_ROWS;
        if (thumbH < 6) thumbH = 6;
        int thumbY = trackTop + (trackH - thumbH) * scrollOffset / maxScroll;
        canvas.fillRect(SCREEN_WIDTH - 7, thumbY, 3, thumbH, theme.accent);
    }
#endif
}

void ProjectInfoView::exit() {}
