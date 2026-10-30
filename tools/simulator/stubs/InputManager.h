#pragma once
#include <cstdint>
class InputManager {
 public:
  static constexpr uint8_t BTN_BACK = 0, BTN_CONFIRM = 1, BTN_LEFT = 2, BTN_RIGHT = 3,
                           BTN_UP = 4, BTN_DOWN = 5, BTN_POWER = 6;
  void begin() {}
  void release(uint8_t key) { pending_ = key; }
  void update() { released_ = pending_; pending_ = 255; }
  bool wasReleased(uint8_t key) const { return key == released_; }
  bool isPowerButtonPhysicallyPressed() const { return false; }
 private:
  uint8_t pending_ = 255, released_ = 255;
};
