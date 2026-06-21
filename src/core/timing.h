#pragma once

#include <cstdint>

#ifdef DESKTOP_BUILD
#include <SDL2/SDL.h>
inline unsigned long millis() { return SDL_GetTicks(); }
#elif defined(NATIVE_TEST)
inline unsigned long millis() { return 0; }
#else
#include <Arduino.h>
#endif
