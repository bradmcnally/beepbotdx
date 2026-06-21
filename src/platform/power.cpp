#include <M5Cardputer.h>
#include "power.h"

static uint32_t lastReadTime = 0;
static uint8_t cachedPercent = 0;
static bool initialized = false;

uint8_t Power::getBatteryPercent() {
    uint32_t now = millis();
    if (!initialized || now - lastReadTime >= 2000) {
        uint8_t raw = M5.Power.getBatteryLevel();
        if (!initialized) {
            cachedPercent = raw;
            initialized = true;
        } else {
            cachedPercent = (cachedPercent * 7 + raw) / 8;
        }
        lastReadTime = now;
    }
    return cachedPercent;
}
