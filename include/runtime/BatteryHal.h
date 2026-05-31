#pragma once

#include <stdint.h>

namespace GreenhouseHal {

struct BatteryState {
  float voltageV = 0.0F;
  uint32_t rawMilliVolts = 0;
  int percent = 0;
  bool available = false;
  bool calibrated = false;
  bool low = false;
  bool critical = false;
};

void initBatteryMonitor();
BatteryState readBatteryState();

}  // namespace GreenhouseHal