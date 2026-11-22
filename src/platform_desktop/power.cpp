#include "platform/power.h"

#ifdef CARDPUTER_ZERO_DEVICE
#include "core/timing.h"
#include <filesystem>
#include <fstream>

int Power::getBatteryPercent() {
    static int percent = -1;
    static uint32_t nextRead = 0;
    const uint32_t now = millis();
    if (!SDL_TICKS_PASSED(now, nextRead)) return percent;
    nextRead = now + 2000;

    percent = -1;
    std::error_code error;
    std::filesystem::directory_iterator supply("/sys/class/power_supply", error);
    for (const std::filesystem::directory_iterator end; !error && supply != end;
         supply.increment(error)) {
        const auto& path = supply->path();
        std::string type;
        if (!(std::ifstream(path / "type") >> type) || type != "Battery") continue;
        int value;
        if (std::ifstream(path / "capacity") >> value && value >= 0 && value <= 100) {
            percent = value;
            break;
        }
    }
    return percent;
}
#else
int Power::getBatteryPercent() {
    return 85;
}
#endif
