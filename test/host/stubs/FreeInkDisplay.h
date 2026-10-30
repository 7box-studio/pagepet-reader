#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
namespace freeink {
class FreeInkDisplay {
 public:
  static constexpr int FAST_REFRESH = 0;
  static constexpr int HALF_REFRESH = 1;
  FreeInkDisplay() = default;
  FreeInkDisplay(int, int, int, int, int, int) {}
  std::vector<uint8_t> pixels = std::vector<uint8_t>(792 * 528 / 8, 0xff);
  bool lent = false;
  uint32_t loans = 0;
  uint32_t refreshes = 0;
  std::vector<uint8_t> presented = pixels;
  std::function<void()> onPresent;
  void setDisplayX3() {}
  void begin() {}
  void deepSleep() {}
  uint8_t* lendBuildStorage(uint32_t* size) {
    if (lent) return nullptr;
    lent = true;
    ++loans;
    *size = pixels.size();
    return pixels.data();
  }
  void returnBuildStorage() { lent = false; clearScreen(); }
  uint8_t* getFrameBuffer() { return lent ? nullptr : pixels.data(); }
  int getDisplayWidth() { return 792; }
  int getDisplayHeight() { return 528; }
  int getDisplayWidthBytes() { return 99; }
  void clearScreen() { memset(pixels.data(), 0xff, pixels.size()); }
  void displayBuffer(int) {
    presented = pixels;
    ++refreshes;
    if (onPresent) onPresent();
  }
  // Match FreeInkDisplay's panel-native, 1=white bitmap blit.
  void drawImage(const uint8_t* image, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool) {
    const uint16_t stride = (w + 7) / 8;
    for (uint16_t row = 0; row < h && y + row < getDisplayHeight(); ++row) {
      for (uint16_t col = 0; col < w && x + col < getDisplayWidth(); ++col) {
        const uint8_t mask = 0x80u >> ((x + col) & 7);
        auto& pixel = pixels[(y + row) * getDisplayWidthBytes() + (x + col) / 8];
        if (image[row * stride + col / 8] & (0x80u >> (col & 7))) pixel |= mask;
        else pixel &= static_cast<uint8_t>(~mask);
      }
    }
  }
};
}
