#include "project_info_view.h"
#include "platform/memory.h"
#include "platform/led.h"
#include "views/settings_view.h"
#include "core/theme.h"
#include "config.h"
#include <cstdio>

// static const uint8_t NUM_SETTINGS = 2;  // TODO: restore when bit depth is implemented
static const uint8_t NUM_SETTINGS = 1;
static const uint8_t TOTAL_ROWS = NUM_SETTINGS + 1 + NUM_SOUNDS;

ProjectInfoView::ProjectInfoView(Project& project, Character& character)
    : _project(project), _character(character) {}

void ProjectInfoView::enter() {
    _cursor = 0;
    _closeRequested = false;
}

void ProjectInfoView::update(InputEvent event) {
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
                if (GlobalSettings::instance->ledMode != LED_OFF) {
                    uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b);
                }
            }
            break;
        case INPUT_RIGHT:
        case INPUT_ENTER:
            if (_cursor == 0) {
                _project.themeIndex = (_project.themeIndex + 1) % ThemeOps::NUM_PRESETS;
                _project.dirty = true;
                if (GlobalSettings::instance->ledMode != LED_OFF) {
                    uint8_t r, g, b; ThemeOps::getPresetRGB(_project.themeIndex, r, g, b); LED::setColor(r, g, b);
                }
            }
            break;
        case INPUT_BACK:
        case INPUT_ESC:
            _closeRequested = true;
            break;
        default:
            break;
    }
}

void ProjectInfoView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

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

    uint8_t bytesPerSample = (_project.bitDepth == BIT_DEPTH_8) ? 1 : 2;
    uint32_t available = Memory::getFree();
    float availSecs = (float)available / (SAMPLE_RATE * bytesPerSample);
    uint32_t usedBytes = 0;
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (_project.sounds[i].occupied)
            usedBytes += _project.sounds[i].length * bytesPerSample;
    }

    char buf[40];

    for (int i = scrollOffset; i < TOTAL_ROWS && (i - scrollOffset) < drawItems; i++) {
        int y = listTop + (i - scrollOffset) * lineH;
        bool selected = (i == _cursor);

        if (i < NUM_SETTINGS) {
            const char* label = "COLOR";
            const char* value = ThemeOps::getPresetName(_project.themeIndex);

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
                uint32_t kb = (_project.sounds[slot].length * bytesPerSample) / 1024;
                canvas.setTextColor(selected ? TFT_WHITE : theme.accent);
                snprintf(buf, sizeof(buf), "%d. %-8s", slot + 1, _project.sounds[slot].name);
                canvas.drawString(buf, labelX, y);
                canvas.setTextColor(selected ? TFT_WHITE : theme.dim);
                snprintf(buf, sizeof(buf), "%.2fs", secs);
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
}

void ProjectInfoView::exit() {}
