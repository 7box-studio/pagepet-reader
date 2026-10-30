#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#ifdef PAGEPET_SIMULATOR
#include <chrono>
#include <thread>
#define PROGMEM
inline const auto hostStarted = std::chrono::steady_clock::now();
#endif
inline uint32_t hostMillis = 0;
#ifdef PAGEPET_SIMULATOR
inline uint32_t millis() {
  return hostMillis + std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - hostStarted).count();
}
inline void delay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
#else
inline uint32_t millis() { return hostMillis; }
inline void delay(uint32_t ms) { hostMillis += ms; }
#endif
struct HostSerial {
  void begin(unsigned long) {}
  void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
#ifdef PAGEPET_SIMULATOR
    vfprintf(stderr, format, args);
#else
    vprintf(format, args);
#endif
    va_end(args);
  }
};
inline HostSerial Serial;
struct HostEsp {
  uint32_t getFreeHeap() { return 0; }
  uint32_t getMaxAllocHeap() { return 0; }
};
inline HostEsp ESP;
