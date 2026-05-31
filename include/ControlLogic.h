#pragma once

#include <math.h>
#include <stdint.h>

namespace GreenhouseLogic {

enum class ControlMode : uint8_t {
  automatic = 0,
  forceOpen = 1,
  forceClosed = 2,
};

enum class CropProfileId : uint8_t {
  tomato = 0,
  lettuce = 1,
  pepper = 2,
  herbs = 3,
};

enum class CropStatus : uint8_t {
  optimal = 0,
  acceptable = 1,
  stressed = 2,
  unavailable = 3,
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

struct CropProfile {
  CropProfileId id;
  const char *name;
  float optimalTempMinC;
  float optimalTempMaxC;
  float acceptableTempMinC;
  float acceptableTempMaxC;
  float optimalHumidityMinPct;
  float optimalHumidityMaxPct;
  float acceptableHumidityMinPct;
  float acceptableHumidityMaxPct;
  float optimalVpdMinKPa;
  float optimalVpdMaxKPa;
  float acceptableVpdMinKPa;
  float acceptableVpdMaxKPa;
};

struct GreenhouseMetrics {
  float vaporPressureDeficitKPa = 0.0F;
  float dewPointC = 0.0F;
  bool vpdAvailable = false;
  bool dewPointAvailable = false;
  bool frostRisk = false;
  CropStatus cropStatus = CropStatus::unavailable;
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

inline bool resolveDaylight(const SensorSnapshot &snapshot,
                           const ClimateConfig &climate,
                           bool hasTime,
                           uint32_t currentMinute) {
  if (snapshot.lightAvailable) {
    return snapshot.lightLux >= climate.growLightLuxThreshold;
  }

  if (hasTime) {
    return inGrowLightWindow(climate, hasTime, currentMinute);
  }

  return false;
}

inline const CropProfile &cropProfile(CropProfileId id) {
  static constexpr CropProfile profiles[] = {
      {CropProfileId::tomato, "Tomatoes", 21.0F, 27.0F, 16.0F, 32.0F, 60.0F, 75.0F, 50.0F, 85.0F, 0.8F, 1.2F, 0.6F, 1.5F},
      {CropProfileId::lettuce, "Lettuce", 12.0F, 20.0F, 5.0F, 26.0F, 55.0F, 75.0F, 45.0F, 85.0F, 0.4F, 0.8F, 0.2F, 1.0F},
      {CropProfileId::pepper, "Peppers", 20.0F, 29.0F, 16.0F, 33.0F, 55.0F, 75.0F, 45.0F, 85.0F, 0.8F, 1.3F, 0.6F, 1.6F},
      {CropProfileId::herbs, "Herbs", 18.0F, 24.0F, 12.0F, 29.0F, 45.0F, 65.0F, 35.0F, 75.0F, 0.6F, 1.0F, 0.4F, 1.3F},
  };

  const uint8_t index = static_cast<uint8_t>(id);
  return index < (sizeof(profiles) / sizeof(profiles[0])) ? profiles[index] : profiles[0];
}

inline float saturationVaporPressureKPa(float tempC) {
  return 0.6108F * expf((17.27F * tempC) / (tempC + 237.3F));
}

inline bool canCalculateClimateMetrics(const SensorSnapshot &snapshot) {
  return snapshot.airAvailable && !isnan(snapshot.airTempC) && !isnan(snapshot.humidityPct) &&
         snapshot.humidityPct >= 0.0F && snapshot.humidityPct <= 100.0F;
}

inline float calculateDewPointC(float tempC, float humidityPct) {
  const float clampedHumidity = humidityPct < 1.0F ? 1.0F : humidityPct;
  const float gamma = logf(clampedHumidity / 100.0F) + ((17.62F * tempC) / (243.12F + tempC));
  return (243.12F * gamma) / (17.62F - gamma);
}

inline float calculateVpdKPa(float tempC, float humidityPct) {
  const float saturation = saturationVaporPressureKPa(tempC);
  const float relative = humidityPct / 100.0F;
  return saturation * (1.0F - relative);
}

inline CropStatus evaluateCropStatus(const SensorSnapshot &snapshot,
                                     const GreenhouseMetrics &metrics,
                                     const CropProfile &profile) {
  if (!canCalculateClimateMetrics(snapshot) || !metrics.vpdAvailable) {
    return CropStatus::unavailable;
  }

  const bool optimal = snapshot.airTempC >= profile.optimalTempMinC &&
                       snapshot.airTempC <= profile.optimalTempMaxC &&
                       snapshot.humidityPct >= profile.optimalHumidityMinPct &&
                       snapshot.humidityPct <= profile.optimalHumidityMaxPct &&
                       metrics.vaporPressureDeficitKPa >= profile.optimalVpdMinKPa &&
                       metrics.vaporPressureDeficitKPa <= profile.optimalVpdMaxKPa;
  if (optimal) {
    return CropStatus::optimal;
  }

  const bool acceptable = snapshot.airTempC >= profile.acceptableTempMinC &&
                          snapshot.airTempC <= profile.acceptableTempMaxC &&
                          snapshot.humidityPct >= profile.acceptableHumidityMinPct &&
                          snapshot.humidityPct <= profile.acceptableHumidityMaxPct &&
                          metrics.vaporPressureDeficitKPa >= profile.acceptableVpdMinKPa &&
                          metrics.vaporPressureDeficitKPa <= profile.acceptableVpdMaxKPa;
  return acceptable ? CropStatus::acceptable : CropStatus::stressed;
}

inline GreenhouseMetrics evaluateMetrics(const SensorSnapshot &snapshot, CropProfileId cropId) {
  GreenhouseMetrics metrics{};
  if (!canCalculateClimateMetrics(snapshot)) {
    return metrics;
  }

  metrics.dewPointC = calculateDewPointC(snapshot.airTempC, snapshot.humidityPct);
  metrics.vaporPressureDeficitKPa = calculateVpdKPa(snapshot.airTempC, snapshot.humidityPct);
  metrics.dewPointAvailable = true;
  metrics.vpdAvailable = true;
  metrics.frostRisk = snapshot.airTempC <= 2.0F || metrics.dewPointC <= 0.5F ||
                      (snapshot.airTempC <= 4.0F && (snapshot.airTempC - metrics.dewPointC) <= 2.0F);
  metrics.cropStatus = evaluateCropStatus(snapshot, metrics, cropProfile(cropId));
  return metrics;
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