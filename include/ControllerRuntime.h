#pragma once

#include <stdint.h>

#include "ControlLogic.h"

namespace GreenhouseRuntime {

enum class ControllerState : uint8_t {
  automatic = 0,
  manualOpen,
  manualClosed,
  lowPower,
  safe,
};

struct PowerBudget {
  bool allowServoMoves = true;
  bool allowVentFans = true;
  bool allowDefogger = true;
  bool allowGrowLight = true;
  bool allowCirculation = true;
};

struct SensorFreshnessPolicy {
  uint32_t airMaxAgeMs = 0;
  uint32_t waterMaxAgeMs = 0;
  uint32_t lightMaxAgeMs = 0;
};

enum class ConfigFault : uint8_t {
  none = 0,
  climateThresholds,
  humidityThresholds,
  heaterThresholds,
  lightingWindow,
  intervals,
  batteryThresholds,
  servoAngles,
  freshnessTimeouts,
  loraPolicy,
};

enum class DiagnosticCode : uint8_t {
  none = 0,
  airSensorUnavailable,
  airSensorRecovered,
  lightSensorReadFailure,
  waterSensorReadFailure,
  loraQueueWarning,
  loraFrameDropped,
  servoCommandBlocked,
  servoMoveSlow,
  safeModeEntered,
};

struct ValidationInput {
  GreenhouseLogic::ClimateConfig climate{};
  SensorFreshnessPolicy freshness{};
  uint32_t sampleIntervalMs = 0;
  uint32_t displayIntervalMs = 0;
  uint32_t controlIntervalMs = 0;
  uint32_t otaPollIntervalMs = 0;
  uint32_t mqttPublishIntervalMs = 0;
  uint8_t loraMaxRetries = 0;
  uint32_t loraRetryBackoffMs = 0;
  float batteryLowVoltage = 0.0F;
  float batteryCriticalVoltage = 0.0F;
  float batteryFullVoltage = 0.0F;
  float batteryEmptyVoltage = 0.0F;
  int topClosedDegrees = 0;
  int topOpenDegrees = 0;
  int bottomClosedDegrees = 0;
  int bottomOpenDegrees = 0;
};

inline const char *controllerStateLabel(ControllerState state) {
  switch (state) {
    case ControllerState::automatic:
      return "AUTO";
    case ControllerState::manualOpen:
      return "OPEN";
    case ControllerState::manualClosed:
      return "CLOSED";
    case ControllerState::lowPower:
      return "LOW_PWR";
    case ControllerState::safe:
      return "SAFE";
  }
  return "UNKNOWN";
}

inline const char *configFaultLabel(ConfigFault fault) {
  switch (fault) {
    case ConfigFault::none:
      return "NONE";
    case ConfigFault::climateThresholds:
      return "CLIMATE";
    case ConfigFault::humidityThresholds:
      return "HUMIDITY";
    case ConfigFault::heaterThresholds:
      return "HEATER";
    case ConfigFault::lightingWindow:
      return "LIGHTING";
    case ConfigFault::intervals:
      return "TIMING";
    case ConfigFault::batteryThresholds:
      return "BATTERY";
    case ConfigFault::servoAngles:
      return "SERVO";
    case ConfigFault::freshnessTimeouts:
      return "FRESHNESS";
    case ConfigFault::loraPolicy:
      return "LORA";
  }
  return "UNKNOWN";
}

inline const char *diagnosticCodeLabel(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::none:
      return "NONE";
    case DiagnosticCode::airSensorUnavailable:
      return "AIR_UNAVAILABLE";
    case DiagnosticCode::airSensorRecovered:
      return "AIR_RECOVERED";
    case DiagnosticCode::lightSensorReadFailure:
      return "LIGHT_READ_FAIL";
    case DiagnosticCode::waterSensorReadFailure:
      return "WATER_READ_FAIL";
    case DiagnosticCode::loraQueueWarning:
      return "LORA_QUEUE_WARN";
    case DiagnosticCode::loraFrameDropped:
      return "LORA_DROP";
    case DiagnosticCode::servoCommandBlocked:
      return "SERVO_BLOCKED";
    case DiagnosticCode::servoMoveSlow:
      return "SERVO_SLOW";
    case DiagnosticCode::safeModeEntered:
      return "SAFE_MODE";
  }
  return "UNKNOWN";
}

inline ControllerState resolveControllerState(bool safeMode,
                                              bool batteryAvailable,
                                              bool batteryLow,
                                              bool batteryCritical,
                                              GreenhouseLogic::ControlMode mode) {
  if (safeMode) {
    return ControllerState::safe;
  }
  if (batteryAvailable && (batteryLow || batteryCritical)) {
    return ControllerState::lowPower;
  }
  switch (mode) {
    case GreenhouseLogic::ControlMode::automatic:
      return ControllerState::automatic;
    case GreenhouseLogic::ControlMode::forceOpen:
      return ControllerState::manualOpen;
    case GreenhouseLogic::ControlMode::forceClosed:
      return ControllerState::manualClosed;
  }
  return ControllerState::automatic;
}

inline PowerBudget evaluatePowerBudget(bool batteryAvailable, bool batteryLow, bool batteryCritical) {
  PowerBudget budget{};
  if (!batteryAvailable) {
    return budget;
  }
  if (batteryLow) {
    budget.allowGrowLight = false;
    budget.allowCirculation = false;
  }
  if (batteryCritical) {
    budget.allowServoMoves = false;
    budget.allowVentFans = false;
    budget.allowDefogger = false;
    budget.allowGrowLight = false;
    budget.allowCirculation = false;
  }
  return budget;
}

inline uint32_t sampleAgeMs(uint32_t now, uint32_t lastValidAtMs) {
  return lastValidAtMs == 0U ? UINT32_MAX : (now - lastValidAtMs);
}

inline bool sampleFresh(uint32_t now, uint32_t lastValidAtMs, uint32_t maxAgeMs) {
  return lastValidAtMs > 0U && maxAgeMs > 0U && (now - lastValidAtMs) <= maxAgeMs;
}

inline ConfigFault validateConfiguration(const ValidationInput &input) {
  if (!(input.climate.ventCloseTempC < input.climate.ventOpenTempC &&
        input.climate.fanOffTempC < input.climate.fanOnTempC)) {
    return ConfigFault::climateThresholds;
  }
  if (!(input.climate.humidityFanOffPct < input.climate.humidityFanOnPct)) {
    return ConfigFault::humidityThresholds;
  }
  if (!(input.climate.heaterOnTempC < input.climate.heaterOffTempC)) {
    return ConfigFault::heaterThresholds;
  }
  if (!(input.climate.growLightStartMinutes < input.climate.growLightStopMinutes &&
        input.climate.growLightStopMinutes <= 24U * 60U)) {
    return ConfigFault::lightingWindow;
  }
  if (input.sampleIntervalMs == 0U || input.displayIntervalMs == 0U || input.controlIntervalMs == 0U ||
      input.otaPollIntervalMs == 0U || input.mqttPublishIntervalMs == 0U) {
    return ConfigFault::intervals;
  }
  if (!(input.batteryCriticalVoltage <= input.batteryLowVoltage &&
        input.batteryLowVoltage > input.batteryEmptyVoltage &&
        input.batteryFullVoltage > input.batteryLowVoltage)) {
    return ConfigFault::batteryThresholds;
  }
  const int servoAngles[] = {
      input.topClosedDegrees,
      input.topOpenDegrees,
      input.bottomClosedDegrees,
      input.bottomOpenDegrees,
  };
  for (const int angle : servoAngles) {
    if (angle < 0 || angle > 180) {
      return ConfigFault::servoAngles;
    }
  }
  if (input.freshness.airMaxAgeMs < input.controlIntervalMs ||
      input.freshness.waterMaxAgeMs < input.controlIntervalMs ||
      input.freshness.lightMaxAgeMs < input.controlIntervalMs) {
    return ConfigFault::freshnessTimeouts;
  }
  if (input.loraMaxRetries == 0U || input.loraRetryBackoffMs == 0U) {
    return ConfigFault::loraPolicy;
  }
  return ConfigFault::none;
}

}  // namespace GreenhouseRuntime
