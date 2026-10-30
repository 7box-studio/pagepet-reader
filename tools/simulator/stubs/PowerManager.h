#pragma once
namespace freeink {
struct SimulatedSleep {};
class PowerManager {
 public:
  static void powerDownRailsForSleep() {}
  [[noreturn]] static void deepSleepUntilPowerButton() { throw SimulatedSleep{}; }
};
}
