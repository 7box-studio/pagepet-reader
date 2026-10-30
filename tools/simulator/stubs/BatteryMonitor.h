#pragma once
#include <cstdint>

class BatteryMonitor {
 public:
  struct Status {
    bool supported = false;
    bool percentageKnown = false;
    bool millivoltsKnown = false;
    bool chargingKnown = false;
    bool externalPowerKnown = false;
    uint16_t percentage = 0;
    uint16_t millivolts = 0;
    bool charging = false;
    bool externalPower = false;
  };
  Status readStatus() const;
};
namespace hostbattery {
inline BatteryMonitor::Status sample = [] {
  BatteryMonitor::Status value;
  value.supported = value.percentageKnown = value.millivoltsKnown = value.chargingKnown = true;
  value.percentage = 82;
  value.millivolts = 3900;
  return value;
}();
inline uint32_t reads = 0;
}
inline BatteryMonitor::Status BatteryMonitor::readStatus() const {
  ++hostbattery::reads;
  return hostbattery::sample;
}
