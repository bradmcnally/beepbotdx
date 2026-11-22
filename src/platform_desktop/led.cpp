#include "platform/led.h"

#ifdef CARDPUTER_ZERO_DEVICE
#include <SDL2/SDL.h>

#include <fstream>
#include <string>

namespace {
// Linux handles the ACT LED polarity; disable its trigger while we own it.
constexpr const char* brightnessPath = "/sys/class/leds/ACT/brightness";
constexpr const char* triggerPath = "/sys/class/leds/ACT/trigger";
bool available = false;
int originalBrightness = 0;
int currentBrightness = -1;
std::string originalTrigger;

bool writeValue(const char* path, const std::string& value) {
    std::ofstream file(path);
    file << value << '\n';
    file.close();
    return static_cast<bool>(file);
}

void setBrightness(int value) {
    if (!available || currentBrightness == value) return;
    if (writeValue(brightnessPath, std::to_string(value))) {
        currentBrightness = value;
    } else {
        SDL_Log("Zero ACT LED write failed: %s", brightnessPath);
        available = false;
    }
}
}

void LED::init() {
    if (!(std::ifstream(brightnessPath) >> originalBrightness)) {
        SDL_Log("Zero ACT LED unavailable: %s", brightnessPath);
        return;
    }
    std::string triggers;
    std::getline(std::ifstream(triggerPath), triggers);
    const auto start = triggers.find('[');
    const auto end = triggers.find(']', start);
    if (start == std::string::npos || end == std::string::npos) {
        SDL_Log("Zero ACT LED trigger unavailable: %s", triggerPath);
        return;
    }
    originalTrigger = triggers.substr(start + 1, end - start - 1);
    if (!writeValue(triggerPath, "none")) {
        SDL_Log("Zero ACT LED control denied: %s", triggerPath);
        originalTrigger.clear();
        return;
    }
    available = true;
    currentBrightness = -1;
    setBrightness(0);
}

void LED::setColor(uint8_t r, uint8_t g, uint8_t b) {
    setBrightness((r || g || b) ? 1 : 0);
}

void LED::off() { setBrightness(0); }

void LED::shutdown() {
    if (originalTrigger.empty()) return;
    const bool brightnessRestored =
        writeValue(brightnessPath, std::to_string(originalBrightness));
    const bool triggerRestored = writeValue(triggerPath, originalTrigger);
    if (!brightnessRestored || !triggerRestored) {
        SDL_Log("Zero ACT LED state could not be restored");
    }
    available = false;
    originalTrigger.clear();
}
#else
void LED::init() {}
void LED::setColor(uint8_t, uint8_t, uint8_t) {}
void LED::off() {}
void LED::shutdown() {}
#endif
