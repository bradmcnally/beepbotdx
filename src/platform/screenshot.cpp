#include "config.h"

#if ENABLE_SCREENSHOTS

#include "screenshot.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <PNGENC.h>

#define PNG_BUFFER_SIZE 16384

bool takeScreenshot() {
    const int displayWidth = SCREEN_WIDTH;
    const int displayHeight = SCREEN_HEIGHT;

    bool success = false;
    uint8_t* pngBuffer = (uint8_t*)malloc(PNG_BUFFER_SIZE);
    uint8_t* lineBuffer = (uint8_t*)malloc(displayWidth * 4);
    uint16_t* displayLine = (uint16_t*)malloc(displayWidth * 2);
    PNGENC* png = new PNGENC();

    if (!pngBuffer || !lineBuffer || !displayLine || !png) goto cleanup;

    if (png->open(pngBuffer, PNG_BUFFER_SIZE) != PNG_SUCCESS) goto cleanup;
    if (png->encodeBegin(displayWidth, displayHeight, PNG_PIXEL_TRUECOLOR_ALPHA, 32, NULL, 3) != PNG_SUCCESS) {
        png->close();
        goto cleanup;
    }

    for (int y = 0; y < displayHeight; y++) {
        M5Cardputer.Display.readRect(0, y, displayWidth, 1, displayLine);

        for (int x = 0; x < displayWidth; x++) {
            uint16_t color565 = displayLine[x];
            color565 = (color565 >> 8) | (color565 << 8);

            uint8_t r = ((color565 >> 11) & 0x1F);
            r = (r << 3) | (r >> 2);
            uint8_t g = ((color565 >> 5) & 0x3F);
            g = (g << 2) | (g >> 4);
            uint8_t b = (color565 & 0x1F);
            b = (b << 3) | (b >> 2);

            lineBuffer[x * 4 + 0] = r;
            lineBuffer[x * 4 + 1] = g;
            lineBuffer[x * 4 + 2] = b;
            lineBuffer[x * 4 + 3] = 255;
        }

        if (png->addLine(lineBuffer) != PNG_SUCCESS) {
            png->close();
            goto cleanup;
        }
    }

    {
        int pngSize = png->close();
        if (pngSize <= 0) goto cleanup;

        if (!SD.exists("/beepbotdx/screenshots")) {
            SD.mkdir("/beepbotdx");
            SD.mkdir("/beepbotdx/screenshots");
        }

        int num = 0;
        char filename[64];
        do {
            snprintf(filename, sizeof(filename), "/beepbotdx/screenshots/screenshot_%04d.png", num);
            num++;
        } while (SD.exists(filename) && num < 10000);

        if (num >= 10000) goto cleanup;

        File file = SD.open(filename, FILE_WRITE);
        if (!file) goto cleanup;

        size_t written = file.write(pngBuffer, pngSize);
        file.close();
        success = (written == (size_t)pngSize);
    }

cleanup:
    delete png;
    free(displayLine);
    free(lineBuffer);
    free(pngBuffer);
    return success;
}

#endif
