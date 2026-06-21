#include "play_view.h"
#include "platform/audio.h"
#include "core/theme.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

PlayView::PlayView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _pulseEnergy(0), _nextFloat(0) {
    memset(_wave, 0, sizeof(_wave));
    memset(_floats, 0, sizeof(_floats));
}

void PlayView::enter() {
    _character.setState(CHAR_IDLE);
    memset(_wave, 0, sizeof(_wave));
    memset(_floats, 0, sizeof(_floats));
    _pulseEnergy = 0;
    _nextFloat = 0;
}

void PlayView::onTrigger(uint8_t soundIndex) {
    _pulseEnergy = 30;

    if (soundIndex < NUM_SOUNDS && _project.sounds[soundIndex].occupied) {
        FloatingLabel& f = _floats[_nextFloat];
        strncpy(f.name, _project.sounds[soundIndex].name, 8);
        f.name[8] = '\0';
        f.x = 30 + (soundIndex * 25) % 180;
        f.y = SCREEN_HEIGHT - 12;
        f.life = 40;
        f.active = true;
        _nextFloat = (_nextFloat + 1) % MAX_FLOATS;
    }
}

void PlayView::update(InputEvent event) {
    switch (event) {
        case INPUT_SPACE:
            if (_sequencer.isPlaying()) {
                _sequencer.stop();
                _character.setState(CHAR_IDLE);
            } else {
                _sequencer.playSong(0);
                _character.setState(CHAR_PLAYING);
            }
            break;
        case INPUT_NUM1: case INPUT_NUM2: case INPUT_NUM3: case INPUT_NUM4:
        case INPUT_NUM5: case INPUT_NUM6: case INPUT_NUM7: case INPUT_NUM8: {
            uint8_t idx = event - INPUT_NUM1;
            if (idx < NUM_SOUNDS && _project.sounds[idx].occupied) {
                SoundSlot& slot = _project.sounds[idx];
                Audio::triggerSound(slot.samples, slot.length, slot.sampleRate, slot.level * 255 / 100);
                onTrigger(idx);
                _character.setState(CHAR_BEAT);
            }
            break;
        }
        default:
            break;
    }
}

void PlayView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);

    // BPM display
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme.dark);
    char bpmStr[12];
    snprintf(bpmStr, sizeof(bpmStr), "BPM %d", _project.bpm);
    canvas.drawString(bpmStr, 4, 22);

    // Playing indicator
    if (_sequencer.isPlaying()) {
        canvas.setTextColor(theme.accent);
        char posStr[8];
        snprintf(posStr, sizeof(posStr), "P%02d", _sequencer.getCurrentPattern() + 1);
        canvas.drawString(posStr, 200, 22);
    }

    // Waveform oscilloscope line
    const int waveY = 70;
    const int waveX = 10;

    // Shift waveform left
    memmove(_wave, _wave + 1, WAVE_W - 1);

    // Generate new sample at the right edge
    if (_pulseEnergy > 0) {
        int8_t amplitude = (int8_t)(_pulseEnergy * ((rand() % 3) - 1));
        _wave[WAVE_W - 1] = amplitude;
        _pulseEnergy = _pulseEnergy > 3 ? _pulseEnergy - 3 : 0;
    } else {
        // Subtle idle noise
        _wave[WAVE_W - 1] = (rand() % 3) - 1;
    }

    // Draw waveform (2px thick)
    for (int i = 1; i < WAVE_W; i++) {
        int y0 = waveY + _wave[i - 1];
        int y1 = waveY + _wave[i];

        uint16_t color;
        if (abs(_wave[i]) > 15) {
            color = theme.accent;
        } else if (abs(_wave[i]) > 5) {
            color = theme.dark;
        } else {
            color = theme.dim;
        }

        if (y0 == y1) {
            canvas.fillRect(waveX + i, y1, 1, 2, color);
        } else {
            int yMin = y0 < y1 ? y0 : y1;
            int yMax = y0 > y1 ? y0 : y1;
            canvas.fillRect(waveX + i, yMin, 2, yMax - yMin + 1, color);
        }
    }

    // Center line (very subtle)
    for (int i = 0; i < WAVE_W; i += 4) {
        canvas.drawPixel(waveX + i, waveY, theme.dim);
    }

    // Floating labels
    for (int i = 0; i < MAX_FLOATS; i++) {
        FloatingLabel& f = _floats[i];
        if (!f.active) continue;

        f.y--;
        f.life--;
        if (f.life == 0) {
            f.active = false;
            continue;
        }

        // Fade: accent → dim
        uint16_t color;
        if (f.life > 30) {
            color = theme.highlight;
        } else if (f.life > 20) {
            color = theme.accent;
        } else if (f.life > 10) {
            color = theme.dark;
        } else {
            color = theme.dim;
        }

        canvas.setTextColor(color);
        canvas.setTextDatum(top_center);
        canvas.drawString(f.name, f.x, f.y);
    }
}

void PlayView::exit() {
    if (_sequencer.isPlaying()) {
        _sequencer.stop();
    }
}
