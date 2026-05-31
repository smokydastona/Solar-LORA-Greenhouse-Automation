#pragma once

#include <stdint.h>

namespace GreenhouseLogic {

enum class ControlMode : uint8_t {
  automatic = 0,
  forceOpen = 1,
  forceClosed = 2,
};

struct SensorSnapshot {
  float airTempC = 0.0F;
  float humidityPct = 0.0F;
  float waterTempC = 0.0F;
  float lightLux = 0.0F;
  bool airAvailable = false;
  bool waterAvailable = false;
  bool lightAvailable = false;
};

struct ActuatorState {
  bool topVentOpen = false;
  bool bottomVentOpen = false;
  bool exhaustFanOn = false;
  bool intakeFanOn = false;
  bool defoggerOn = false;
  bool growLightOn = false;
  bool circulationFansOn = false;
};

struct ClimateConfig {
  float ventOpenTempC;
  float ventCloseTempC;
  float fanOnTempC;
  float fanOffTempC;
  float humidityFanOnPct;
  float humidityFanOffPct;
  float heaterOnTempC;
  float heaterOffTempC;
  float waterHighTempC;
  float waterLowTempC;
  uint32_t growLightStartMinutes;
  uint32_t growLightStopMinutes;
  float growLightLuxThreshold;
};

struct SystemConfig {
  bool enableDefogger;
  bool enableGrowLight;
  bool enableCirculationFans;
};

struct EvaluationContext {
  SensorSnapshot snapshot;
  ActuatorState previousActuators;
  ControlMode mode;
  ClimateConfig climate;
  SystemConfig system;
  bool daylight;
  bool hasTime;
  uint32_t currentMinute;
};

inline ControlMode nextMode(ControlMode mode) {
  switch (mode) {
    case ControlMode::automatic:
      return ControlMode::forceOpen;
    case ControlMode::forceOpen:
      return ControlMode::forceClosed;
    case ControlMode::forceClosed:
      return ControlMode::automatic;
  }

  return ControlMode::automatic;
}

inline bool inGrowLightWindow(const ClimateConfig &climate, bool hasTime, uint32_t currentMinute) {
  return hasTime && currentMinute >= climate.growLightStartMinutes && currentMinute < climate.growLightStopMinutes;
}

inline ActuatorState evaluateActuators(const EvaluationContext &ctx) {
  ActuatorState actuators = ctx.previousActuators;

  if (ctx.mode == ControlMode::forceOpen) {
    actuators.topVentOpen = true;
    actuators.bottomVentOpen = true;
    actuators.exhaustFanOn = true;
    actuators.intakeFanOn = true;
    actuators.circulationFansOn = ctx.system.enableCirculationFans;
    actuators.defoggerOn = false;
    actuators.growLightOn = false;
    return actuators;
  }

  if (ctx.mode == ControlMode::forceClosed) {
    actuators.topVentOpen = false;
    actuators.bottomVentOpen = false;
    actuators.exhaustFanOn = false;
    actuators.intakeFanOn = false;
    actuators.circulationFansOn = false;
    actuators.defoggerOn = false;
    actuators.growLightOn = false;
    return actuators;
  }

  if (ctx.snapshot.airAvailable) {
    const bool tempOpen = ctx.snapshot.airTempC >= ctx.climate.ventOpenTempC;
    const bool tempClose = ctx.snapshot.airTempC <= ctx.climate.ventCloseTempC;

    if (tempOpen) {
      actuators.topVentOpen = true;
      actuators.bottomVentOpen = true;
    } else if (tempClose) {
      actuators.topVentOpen = false;
      actuators.bottomVentOpen = false;
    }

    const bool fanTempOn = ctx.snapshot.airTempC >= ctx.climate.fanOnTempC;
    const bool fanTempOff = ctx.snapshot.airTempC <= ctx.climate.fanOffTempC;
    const bool humidityBoostOn = ctx.snapshot.humidityPct >= ctx.climate.humidityFanOnPct;
    const bool humidityBoostOff = ctx.snapshot.humidityPct <= ctx.climate.humidityFanOffPct;

    if ((fanTempOn || humidityBoostOn) && ctx.daylight) {
      actuators.exhaustFanOn = true;
      actuators.intakeFanOn = true;
    } else if ((fanTempOff && humidityBoostOff) || !ctx.daylight) {
      actuators.exhaustFanOn = false;
      actuators.intakeFanOn = false;
    }

    if (ctx.system.enableDefogger) {
      if (!ctx.daylight && ctx.snapshot.airTempC <= ctx.climate.heaterOnTempC) {
        actuators.defoggerOn = true;
      } else if (ctx.snapshot.airTempC >= ctx.climate.heaterOffTempC || ctx.daylight) {
        actuators.defoggerOn = false;
      }
    }

    actuators.circulationFansOn = ctx.system.enableCirculationFans &&
                                  ctx.daylight &&
                                  (ctx.snapshot.humidityPct >= ctx.climate.humidityFanOffPct ||
                                   ctx.snapshot.airTempC >= ctx.climate.ventCloseTempC);
  } else {
    // Conservative fallback when the main air sensor is unavailable.
    actuators.topVentOpen = ctx.daylight;
    actuators.bottomVentOpen = ctx.daylight;
    actuators.exhaustFanOn = false;
    actuators.intakeFanOn = false;
    actuators.defoggerOn = false;
    actuators.circulationFansOn = false;
  }

  if (ctx.system.enableGrowLight) {
    actuators.growLightOn = inGrowLightWindow(ctx.climate, ctx.hasTime, ctx.currentMinute) &&
                            (!ctx.snapshot.lightAvailable || ctx.snapshot.lightLux < ctx.climate.growLightLuxThreshold);
  } else {
    actuators.growLightOn = false;
  }

  return actuators;
}

}  // namespace GreenhouseLogic