#include "runtime/BatteryHal.h"

#include <Arduino.h>

#include "PinMap.h"
#include "Settings.h"

namespace GreenhouseHal {

namespace {

void batteryAdcEnable() {
  digitalWrite(PinMap::BATTERY_ADC_CTRL, LOW);
  delay(10);
}

void batteryAdcDisable() {
  digitalWrite(PinMap::BATTERY_ADC_CTRL, HIGH);
}

}  // namespace

void initBatteryMonitor() {
  if (!Settings::BATTERY.enableVoltageMonitor) {
    return;
  }

  pinMode(PinMap::BATTERY_ADC_CTRL, OUTPUT);
  digitalWrite(PinMap::BATTERY_ADC_CTRL, HIGH);
}

BatteryState readBatteryState() {
  BatteryState state{};
  const int pin = Settings::BATTERY.analogPin >= 0 ? Settings::BATTERY.analogPin : PinMap::BATTERY_VOLTAGE;
  if (!Settings::BATTERY.enableVoltageMonitor || pin < 0) {
    return state;
  }

  batteryAdcEnable();
  uint32_t totalMilliVolts = 0;
  for (uint8_t index = 0; index < Settings::BATTERY.samplesPerRead; ++index) {
    totalMilliVolts += static_cast<uint32_t>(analogReadMilliVolts(pin));
  }
  batteryAdcDisable();

  const float measuredV = (static_cast<float>(totalMilliVolts) /
                           static_cast<float>(Settings::BATTERY.samplesPerRead)) /
                          1000.0F;
  state.rawMilliVolts = totalMilliVolts / static_cast<uint32_t>(Settings::BATTERY.samplesPerRead);
  state.voltageV = (measuredV * Settings::BATTERY.dividerRatio) + Settings::BATTERY.calibrationOffsetVolts;
  state.available = state.voltageV > 0.1F;
  if (!state.available) {
    return state;
  }

  state.calibrated = Settings::BATTERY.calibrationVerified;

  const float percent = ((state.voltageV - Settings::BATTERY.emptyVoltage) /
                         (Settings::BATTERY.fullVoltage - Settings::BATTERY.emptyVoltage)) * 100.0F;
  const float clamped = percent < 0.0F ? 0.0F : (percent > 100.0F ? 100.0F : percent);
  state.percent = static_cast<int>(clamped + 0.5F);
  state.low = state.voltageV <= Settings::BATTERY.lowVoltage;
  state.critical = state.voltageV <= Settings::BATTERY.criticalVoltage;
  return state;
}

}  // namespace GreenhouseHal