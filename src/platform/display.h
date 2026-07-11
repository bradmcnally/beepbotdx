#pragma once

#include "core/canvas.h"

namespace Display {

void init();
void beginFrame();
void endFrame();
Canvas& canvas();
void shutdown();
void setBrightness(uint8_t value);
void toggleFullscreen();

}
