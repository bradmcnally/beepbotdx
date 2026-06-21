#include "song_view.h"
#include "core/theme.h"
#include "config.h"
#include <cstdio>

SongView::SongView(Project& project, Character& character, Sequencer& sequencer)
    : _project(project), _character(character), _sequencer(sequencer),
      _cursor(0), _editRequested(false) {}

void SongView::enter() {
    _editRequested = false;
    _character.setState(CHAR_IDLE);
}

void SongView::update(InputEvent event) {
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
        case INPUT_PLUS:
            if (_project.song[_cursor] == 0xFF) {
                _project.song[_cursor] = 0;
            } else if (_project.song[_cursor] < NUM_PATTERNS - 1) {
                _project.song[_cursor]++;
            }
            break;
        case INPUT_MINUS:
            if (_project.song[_cursor] != 0xFF) {
                if (_project.song[_cursor] > 0) {
                    _project.song[_cursor]--;
                } else {
                    _project.song[_cursor] = 0xFF;
                }
            }
            break;
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
                _sequencer.playSong(0);
                _character.setState(CHAR_PLAYING);
            }
            break;
        default:
            break;
    }
}

void SongView::draw(Canvas& canvas) {
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

    for (uint8_t i = 0; i < NUM_SONG_POSITIONS; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = gridLeft + col * (cellW + spacing);
        int y = gridTopY + row * (cellH + spacing);

        bool selected = (i == _cursor);
        bool empty = (_project.song[i] == 0xFF);

        // Cell background
        uint16_t bgColor;
        if (selected) bgColor = TFT_WHITE;
        else if (!empty) bgColor = theme.accent;
        else bgColor = theme.dark;
        canvas.fillRect(x, y, cellW, cellH, bgColor);

        if (!empty) {
            canvas.setTextColor(TFT_BLACK);
            canvas.setTextDatum(top_left);

            char label[5];
            snprintf(label, sizeof(label), "%02d", _project.song[i] + 1);
            canvas.drawString(label, x + 4, y + 4);
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
