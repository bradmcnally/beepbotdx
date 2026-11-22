#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <linux/fb.h>
#include "esc_hold.h"

namespace Zero {

class NativeIo {
public:
  NativeIo() = default;
  ~NativeIo();
  NativeIo(const NativeIo&) = delete;
  NativeIo& operator=(const NativeIo&) = delete;

  bool open();
  void present(const uint16_t* pixels);
  int pollEvent(SDL_Event* event);
  const Uint8* keyboardState() const { return keys_.data(); }
  bool isBacklightEvent(const SDL_Event& event) const {
    return event.type == backlightEventType_;
  }
  bool isHomeEvent(const SDL_Event& event) const { return event.type == homeEventType_; }
  int homeSecondsRemaining() const { return escHold_.secondsRemaining(SDL_GetTicks()); }

private:
  void fillKeyEvent(
      SDL_Event* event, SDL_Scancode scan, bool pressed, bool repeat) const;

  Uint32 backlightEventType_ = static_cast<Uint32>(-1);
  Uint32 homeEventType_ = static_cast<Uint32>(-1);
  EscHold escHold_;
  bool homeSent_ = false;
  int framebuffer_ = -1;
  int keyboard_ = -1;
  uint8_t* mapping_ = nullptr;
  std::size_t mappingSize_ = 0;
  std::vector<uint16_t> previousFrame_;
  bool hasPreviousFrame_ = false;
  fb_fix_screeninfo fixed_{};
  fb_var_screeninfo mode_{};
  std::array<Uint8, SDL_NUM_SCANCODES> keys_{};

};

extern NativeIo nativeIo;

}  // namespace Zero
