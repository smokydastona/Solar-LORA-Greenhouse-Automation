#pragma once

#include <Arduino.h>

namespace Settings {

struct WifiConfig {
  const char *ssid;
  const char *password;
  const char *hostname;
  const char *tzInfo;
  bool enableOta;
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

constexpr ClimateConfig CLIMATE{
    28.0F,
    23.0F,
    29.0F,
    24.0F,
    82.0F,
    72.0F,
    5.0F,
    8.0F,
    30.0F,
    10.0F,
    8U * 60U,
    16U * 60U,
    12000.0F,
};

constexpr ServoConfig SERVOS{
    8,
    74,
    12,
    78,
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

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t BME280_I2C_ADDRESS = 0x76;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 64;

}  // namespace Settings
