#pragma once

#include <Arduino.h>

#include "ControlLogic.h"
#include "ControlLogicDefaults.h"

namespace Settings {

struct WifiConfig {
  const char *ssid;
  const char *password;
  const char *hostname;
  const char *tzInfo;
  bool enableOta;
};

using ClimateConfig = GreenhouseLogic::ClimateConfig;

struct ServoConfig {
  int topClosedDegrees;
  int topOpenDegrees;
  int bottomClosedDegrees;
  int bottomOpenDegrees;
};

struct LoggingConfig {
  const char *path;
  uint32_t sampleIntervalMs;
  uint32_t displayIntervalMs;
  uint32_t controlIntervalMs;
  uint32_t otaPollIntervalMs;
};

struct SystemConfig {
  bool enableBme280;
  bool enableDht22;
  bool enableBh1750;
  bool enableWaterProbe;
  bool enableDefogger;
  bool enableGrowLight;
  bool enableCirculationFans;
};

constexpr WifiConfig WIFI{
    "",  // Leave empty to run in offline mode.
    "",
    "mini-greenhouse",
  "PST8PDT,M3.2.0/2,M11.1.0/2",
    false,
};

constexpr ClimateConfig CLIMATE = GreenhouseDefaults::CLIMATE;

constexpr ServoConfig SERVOS{
  28,
  52,
  30,
  54,
};

constexpr LoggingConfig LOGGING{
    "/climate_log.csv",
    300000UL,
    2000UL,
    5000UL,
    10000UL,
};

constexpr SystemConfig SYSTEM{
  false,
  true,
  false,
  false,
    true,
    true,
    true,
};

constexpr GreenhouseLogic::SystemConfig CONTROL_SYSTEM = GreenhouseDefaults::CONTROL_SYSTEM;

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t BME280_I2C_ADDRESS = 0x76;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 64;

}  // namespace Settings
