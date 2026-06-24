#pragma once

#include <cstdint>

namespace LED {

void init();
void setColor(uint8_t r, uint8_t g, uint8_t b);
void off();

}
