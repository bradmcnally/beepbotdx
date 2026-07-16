#include "config.h"

#if ENABLE_SCREENSHOTS

#include "screenshot.h"
#include "display.h"
#include <SD.h>

bool takeScreenshot() {
    const int w = SCREEN_WIDTH;
    const int h = SCREEN_HEIGHT;

    uint16_t* src = Display::canvas().buffer();
    if (!src) return false;

    if (!SD.exists("/beepbotdx/screenshots")) {
        SD.mkdir("/beepbotdx");
        SD.mkdir("/beepbotdx/screenshots");
    }

    int num = 0;
    char filename[64];
    do {
        snprintf(filename, sizeof(filename), "/beepbotdx/screenshots/screenshot_%04d.bmp", num);
        num++;
    } while (SD.exists(filename) && num < 10000);
    if (num >= 10000) return false;

    File file = SD.open(filename, FILE_WRITE);
    if (!file) return false;

    const uint32_t rowSize = ((w * 3 + 3) & ~3);
    const uint32_t imageSize = rowSize * h;
    const uint32_t fileSize = 54 + imageSize;

    uint8_t hdr[54] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = fileSize; hdr[3] = fileSize >> 8; hdr[4] = fileSize >> 16; hdr[5] = fileSize >> 24;
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = w; hdr[19] = w >> 8;
    hdr[22] = h; hdr[23] = h >> 8;
    hdr[26] = 1;
    hdr[28] = 24;
    hdr[34] = imageSize; hdr[35] = imageSize >> 8; hdr[36] = imageSize >> 16; hdr[37] = imageSize >> 24;

    file.write(hdr, 54);

    uint8_t row[rowSize];
    memset(row, 0, rowSize);

    // BMP is bottom-up
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint16_t c = src[y * w + x];

            uint8_t r = ((c >> 11) & 0x1F); r = (r << 3) | (r >> 2);
            uint8_t g = ((c >> 5) & 0x3F);  g = (g << 2) | (g >> 4);
            uint8_t b = (c & 0x1F);         b = (b << 3) | (b >> 2);

            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        file.write(row, rowSize);
    }

    file.close();
    return true;
}

#endif
