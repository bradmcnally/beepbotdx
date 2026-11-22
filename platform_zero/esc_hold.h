#pragma once
#include <cstdint>

namespace Zero {
class EscHold {
public:
  void setPressed(bool pressed, uint32_t now) {
    if (pressed && !pressed_) started_ = now;
    pressed_ = pressed;
  }
  bool ready(uint32_t now) const { return pressed_ && now - started_ >= 3000u; }
  int secondsRemaining(uint32_t now) const {
    if (!pressed_) return 0;
    const uint32_t elapsed = now - started_;
    if (elapsed < 500u || elapsed >= 3000u) return 0;
    return static_cast<int>((3000u - elapsed + 999u) / 1000u);
  }
  bool pressed() const { return pressed_; }
private:
  bool pressed_ = false;
  uint32_t started_ = 0;
};
}  // namespace Zero
