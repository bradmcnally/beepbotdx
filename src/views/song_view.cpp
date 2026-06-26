#include "song_view.h"
#include "platform/input.h"
#include "core/theme.h"
#include "core/timing.h"
#include "core/grid_layout.h"
#include "config.h"
#include <cstdio>

SongView::SongView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _cursor(0), _editRequested(false), _lastSongPos(0xFF) {}

void SongView::enter() {
    _editRequested = false;
    _character.setState(CHAR_IDLE);
}

void SongView::update(InputEvent event) {
    if (!_sequencer.isPlaying() &&
        (_character.getState() == CHAR_PLAYING || _character.getState() == CHAR_BEAT)) {
        _character.setState(CHAR_IDLE);
        _lastSongPos = 0xFF;
    }

    if (_sequencer.isPlaying()) {
        uint8_t pos = _sequencer.getCurrentSongPosition();
        if (pos != _lastSongPos) {
            _lastSongPos = pos;
            _cursor = pos;
            char msg[8];
            snprintf(msg, sizeof(msg), "P%02d", _project.song[pos] + 1);
            _character.say(msg);
        }
    }

    switch (event) {
        case INPUT_LEFT:
            if (_cursor > 0) _cursor--;
            break;
        case INPUT_RIGHT:
            if (_cursor < NUM_SONG_POSITIONS - 1) _cursor++;
            break;
        case INPUT_UP:
            if (_cursor >= 4) _cursor -= 4;
            break;
        case INPUT_DOWN:
            if (_cursor < 12) _cursor += 4;
            break;
        case INPUT_CHAR: {
            char ch = Input::getChar();
            if (ch == ']') {
                if (_project.song[_cursor] == 0xFF) {
                    _project.song[_cursor] = 0;
                } else if (_project.song[_cursor] < NUM_PATTERNS - 1) {
                    _project.song[_cursor]++;
                }
            } else if (ch == '[') {
                if (_project.song[_cursor] != 0xFF) {
                    if (_project.song[_cursor] > 0) {
                        _project.song[_cursor]--;
                    } else {
                        _project.song[_cursor] = 0xFF;
                    }
                }
            }
            break;
        }
        case INPUT_BACK:
            _project.song[_cursor] = 0xFF;
            break;
        case INPUT_ENTER:
            if (_project.song[_cursor] != 0xFF) {
                _editRequested = true;
            }
            break;
        case INPUT_SPACE:
            if (_sequencer.isPlaying()) {
                _sequencer.stop();
                _character.setState(CHAR_IDLE);
            } else {
                bool hasContent = false;
                for (int i = 0; i < NUM_SONG_POSITIONS && !hasContent; i++) {
                    if (_project.song[i] != 0xFF) hasContent = true;
                }
                if (hasContent) {
                    _sequencer.playSong(0);
                    _character.setState(CHAR_PLAYING);
                    _character.say("let's go!");
                    _lastSongPos = 0xFF;
                } else {
                    _character.setState(CHAR_ERROR);
                    _character.say("empty");
                }
            }
            break;
        default:
            break;
    }
}

void SongView::draw(Canvas& canvas) {
    Theme theme = ThemeOps::getPreset(_project.themeIndex);
    canvas.setTextSize(1);

    GridLayout grid = GridLayout::make(4, 4, 22);

    for (uint8_t i = 0; i < NUM_SONG_POSITIONS; i++) {
        int x, y;
        grid.cellXY(i, x, y);

        bool selected = (i == _cursor);
        bool empty = (_project.song[i] == 0xFF);

        uint16_t bgColor;
        if (selected) bgColor = TFT_WHITE;
        else if (!empty) bgColor = theme.accent;
        else bgColor = theme.dark;
        canvas.fillRect(x, y, grid.cellW, grid.cellH, bgColor);

        if (!empty) {
            uint8_t patIdx = _project.song[i];
            Pattern& pat = _project.patterns[patIdx];

            canvas.setTextColor(TFT_BLACK);
            canvas.setTextDatum(top_left);
            char label[5];
            snprintf(label, sizeof(label), "%02d", patIdx + 1);
            canvas.drawString(label, x + 4, y + 2);

            const int px = 2;
            int miniW = NUM_STEPS * px;
            int miniH = NUM_SOUNDS * px;
            int mx = x + grid.cellW - miniW - 3;
            int my = y + (grid.cellH - miniH + 2) / 2;

            bool isPlayingHere = _sequencer.isPlaying() &&
                                  _sequencer.getCurrentSongPosition() == i;
            uint8_t playStep = isPlayingHere ? _sequencer.getCurrentStep() : 0xFF;

            for (uint8_t step = 0; step < NUM_STEPS; step++) {
                for (uint8_t snd = 0; snd < NUM_SOUNDS; snd++) {
                    if (step == playStep) {
                        uint16_t c = (pat.steps[step] & (1 << snd)) ? TFT_WHITE : theme.dim;
                        canvas.fillRect(mx + step * px, my + snd * px, px, px, c);
                    } else if (pat.steps[step] & (1 << snd)) {
                        canvas.fillRect(mx + step * px, my + snd * px, px, px, TFT_BLACK);
                    }
                }
            }
        }
    }
}

void SongView::exit() {
    if (_sequencer.isPlaying()) {
        _sequencer.stop();
    }
}

uint8_t SongView::getEditPattern() const {
    return _project.song[_cursor];
}
