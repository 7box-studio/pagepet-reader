#pragma once

#include <BatteryMonitor.h>
#include <FreeInkDisplay.h>

struct ReaderButtonHints {
  const char* back = "";
  const char* confirm = "";
  const char* previous = "";
  const char* next = "";
  bool sides = false;
};

class ReaderChrome {
 public:
  void draw(freeink::FreeInkDisplay& display, const ReaderButtonHints& hints);
  // Used by simulator controls after explicitly changing the fake sensor.
  void invalidateBattery() { polled_ = false; }
  const BatteryMonitor::Status& battery() const { return battery_; }

 private:
  static constexpr uint32_t POLL_MS = 30000;
  void updateBattery();
  BatteryMonitor::Status battery_{};
  uint32_t lastPollMs_ = 0;
  bool polled_ = false;
};
