#include "play_view.h"
#include "platform/audio.h"
#include "core/theme.h"
#include "core/timing.h"
#include "core/bloom_field.h"
#include "config.h"
#include <cstdio>
#include <cstring>

PlayView::PlayView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer), _lastSound(0xFF) {
    BloomFieldOps::reset(_bloom);
}

void PlayView::enter() {
    _character.setState(CHAR_IDLE);
    BloomFieldOps::reset(_bloom);
}

void PlayView::onTrigger(uint8_t soundIndex) {
    if (soundIndex >= NUM_SOUNDS) return;
    _lastSound = soundIndex;

    // Each slot has its own position on the grid (2 rows x 4 cols layout)
    const int colSpacing = BLOOM_COLS / 5;
    const int slotX[8] = {
        colSpacing, colSpacing*2, colSpacing*3, colSpacing*4,
        colSpacing, colSpacing*2, colSpacing*3, colSpacing*4
    };
    const int rowY1 = BLOOM_ROWS / 4;
    const int rowY2 = BLOOM_ROWS * 3 / 4;
    const int slotY[8] = {rowY1, rowY1, rowY1, rowY1, rowY2, rowY2, rowY2, rowY2};

    uint8_t energy = 120;
    if (_project.sounds[soundIndex].occupied) {
        energy = 80 + (_project.sounds[soundIndex].level * 47 / 100);
    }
    BloomFieldOps::injectAt(_bloom, energy, slotX[soundIndex], slotY[soundIndex]);
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

    canvas.setTextSize(1);
    const int infoY = 22;
    const int infoLeft = 7;
    const int infoRight = SCREEN_WIDTH - 8;

    // Left: current pattern
    canvas.setTextDatum(top_left);
    if (_sequencer.isPlaying()) {
        canvas.setTextColor(theme.accent);
        char patStr[8];
        snprintf(patStr, sizeof(patStr), "P%02d", _sequencer.getCurrentPattern() + 1);
        canvas.drawString(patStr, infoLeft, infoY);
    }

    // Center: last triggered sound name (only while playing)
    if (_sequencer.isPlaying() && _lastSound < NUM_SOUNDS && _project.sounds[_lastSound].occupied) {
        canvas.setTextColor(theme.accent);
        canvas.setTextDatum(top_center);
        canvas.drawString(_project.sounds[_lastSound].name, SCREEN_WIDTH / 2, infoY);
    }

    // Right: current position / total patterns in song
    canvas.setTextColor(theme.accent);
    canvas.setTextDatum(top_right);
    uint8_t total = 0;
    for (int i = 0; i < NUM_SONG_POSITIONS; i++) {
        if (_project.song[i] != 0xFF) total++;
        else break;
    }
    if (_sequencer.isPlaying() && total > 0) {
        char posStr[10];
        snprintf(posStr, sizeof(posStr), "%d/%d", _sequencer.getCurrentSongPosition() + 1, total);
        canvas.drawString(posStr, infoRight, infoY);
    } else if (total > 0) {
        char posStr[10];
        snprintf(posStr, sizeof(posStr), "1/%d", total);
        canvas.drawString(posStr, infoRight, infoY);
    }

    BloomFieldOps::tick(_bloom, millis());

    // Draw bloom field — with margin
    const int fieldX = 4;
    const int fieldY = 34;
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

void PlayView::exit() {
    if (_sequencer.isPlaying()) {
        _sequencer.stop();
    }
}
