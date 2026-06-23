#pragma once

#include "config.h"

struct GridLayout {
    int left;
    int topY;
    int cellW;
    int cellH;
    int spacing;
    int cols;

    static GridLayout make(int cols, int rows, int gridTop, int margin = 3, int spacing = 3) {
        int gridW = SCREEN_WIDTH - margin * 2;
        int gridH = SCREEN_HEIGHT - gridTop - margin;
        int cellW = (gridW - spacing * (cols - 1)) / cols;
        int cellH = (gridH - spacing * (rows - 1)) / rows;
        int contentW = cellW * cols + spacing * (cols - 1);
        int contentH = cellH * rows + spacing * (rows - 1);
        int left = margin + (gridW - contentW) / 2;
        int topY = gridTop + (gridH - contentH) / 2;
        return {left, topY, cellW, cellH, spacing, cols};
    }

    void cellXY(int index, int& x, int& y) const {
        int col = index % cols;
        int row = index / cols;
        x = left + col * (cellW + spacing);
        y = topY + row * (cellH + spacing);
    }
};
