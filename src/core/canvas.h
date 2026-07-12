#pragma once

#include <cstdint>
#include <cstring>

#ifndef TFT_BLACK
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_GREEN   0x07E0
#define TFT_YELLOW  0xFFE0
#define TFT_DARKGREY 0x7BEF
#endif

#ifdef ARDUINO
#include <M5Cardputer.h>
typedef lgfx::v1::textdatum::textdatum_t TextDatum;
#else
enum TextDatum {
    top_left = 0,
    top_center = 1,
    top_right = 2,
};
#endif

class Canvas {
public:
    Canvas() : _width(0), _height(0), _buffer(nullptr),
               _textColor(TFT_WHITE), _textSize(1.0f), _textDatum(top_left) {}

    bool create(int w, int h) {
        _width = w;
        _height = h;
        _buffer = new uint16_t[w * h];
        return _buffer != nullptr;
    }

    ~Canvas() { delete[] _buffer; }

    int width() const { return _width; }
    int height() const { return _height; }
    uint16_t* buffer() { return _buffer; }

    void fillScreen(uint16_t color) {
        int total = _width * _height;
        for (int i = 0; i < total; i++) _buffer[i] = color;
    }

    void drawPixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < _width && y >= 0 && y < _height)
            _buffer[y * _width + x] = color;
    }

    void fillRect(int x, int y, int w, int h, uint16_t color) {
        for (int row = y; row < y + h; row++) {
            if (row < 0 || row >= _height) continue;
            for (int col = x; col < x + w; col++) {
                if (col >= 0 && col < _width)
                    _buffer[row * _width + col] = color;
            }
        }
    }

    void drawRect(int x, int y, int w, int h, uint16_t color) {
        drawFastHLine(x, y, w, color);
        drawFastHLine(x, y + h - 1, w, color);
        drawFastVLine(x, y, h, color);
        drawFastVLine(x + w - 1, y, h, color);
    }

    void drawFastVLine(int x, int y, int h, uint16_t color) {
        for (int row = y; row < y + h; row++) {
            if (row >= 0 && row < _height && x >= 0 && x < _width)
                _buffer[row * _width + x] = color;
        }
    }

    void drawFastHLine(int x, int y, int w, uint16_t color) {
        if (y < 0 || y >= _height) return;
        for (int col = x; col < x + w; col++) {
            if (col >= 0 && col < _width)
                _buffer[y * _width + col] = color;
        }
    }

    void fillCircle(int cx, int cy, int r, uint16_t color) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy <= r * r)
                    drawPixel(cx + dx, cy + dy, color);
            }
        }
    }

    void drawCircle(int cx, int cy, int r, uint16_t color) {
        int x = r, y = 0, d = 1 - r;
        while (x >= y) {
            drawPixel(cx + x, cy + y, color);
            drawPixel(cx - x, cy + y, color);
            drawPixel(cx + x, cy - y, color);
            drawPixel(cx - x, cy - y, color);
            drawPixel(cx + y, cy + x, color);
            drawPixel(cx - y, cy + x, color);
            drawPixel(cx + y, cy - x, color);
            drawPixel(cx - y, cy - x, color);
            y++;
            if (d <= 0) { d += 2 * y + 1; }
            else { x--; d += 2 * (y - x) + 1; }
        }
    }

    void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
        int dx = x0 < x1 ? x1 - x0 : x0 - x1;
        int sx = x0 < x1 ? 1 : -1;
        int dy = -(y0 < y1 ? y1 - y0 : y0 - y1);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void darken() {
        int total = _width * _height;
        for (int p = 0; p < total; p++) {
            uint16_t c = _buffer[p];
            uint8_t r = (c >> 11) & 0x1F;
            uint8_t g = (c >> 5) & 0x3F;
            uint8_t b = c & 0x1F;
            _buffer[p] = ((r >> 3) << 11) | ((g >> 3) << 5) | (b >> 3);
        }
    }

    void setTextColor(uint16_t color) { _textColor = color; }
    void setTextSize(float size) { _textSize = size; }
    void setTextDatum(TextDatum datum) { _textDatum = datum; }

    void drawString(const char* str, int x, int y);

    uint16_t textColor() const { return _textColor; }
    float textSize() const { return _textSize; }
    TextDatum textDatum() const { return _textDatum; }

private:
    int _width;
    int _height;
    uint16_t* _buffer;
    uint16_t _textColor;
    float _textSize;
    TextDatum _textDatum;
};
