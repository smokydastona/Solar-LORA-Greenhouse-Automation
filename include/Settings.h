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
  const char *bootLogPath;
  uint32_t sampleIntervalMs;
  uint32_t displayIntervalMs;
  uint32_t controlIntervalMs;
  uint32_t otaPollIntervalMs;
};

struct DiagnosticsConfig {
  uint8_t recentFaultCapacity;
  uint8_t loraQueueWarningDepth;
  uint32_t servoSlowMoveThresholdMs;
  uint32_t eventRateLimitMs;
};

struct ReliabilityConfig {
  bool enableHardwareWatchdog;
  bool enableSoftwareWatchdog;
  uint32_t hardwareWatchdogTimeoutMs;
  uint32_t softwareWatchdogTimeoutMs;
  uint32_t bootHealthyAfterMs;
  uint8_t safeModeAfterBootFailures;
  bool brownoutEntersSafeMode;
  bool airSensorFaultEntersSafeMode;
  uint8_t maxConsecutiveAirSensorFaults;
  uint32_t sensorFaultRecoveryDelayMs;
  uint32_t airDataMaxAgeMs;
  uint32_t waterDataMaxAgeMs;
  uint32_t lightDataMaxAgeMs;
  uint32_t servoDriveWindowMs;
  uint32_t servoCooldownMs;
  uint8_t servoProtectionTripsBeforeSafeMode;
};

struct BatteryMonitorConfig {
  bool enableVoltageMonitor;
  int analogPin;
  float dividerRatio;
  float calibrationOffsetVolts;
  bool calibrationVerified;
  float lowVoltage;
  float criticalVoltage;
  float fullVoltage;
  float emptyVoltage;
  uint8_t samplesPerRead;
};

struct MqttConfig {
  const char *host;
  uint16_t port;
  const char *username;
  const char *password;
  const char *clientId;
  const char *baseTopic;
  const char *discoveryPrefix;
  bool enableHomeAssistantDiscovery;
  uint32_t reconnectIntervalMs;
  uint32_t publishIntervalMs;
  bool enableInboundModeCommands;
};

struct LoRaConfig {
  bool enableTransport;
  const char *nodeId;
  uint8_t maxRetries;
  uint32_t retryBackoffMs;
  float frequencyMHz;
  float bandwidthKHz;
  uint8_t spreadingFactor;
  uint8_t codingRate;
  uint8_t syncWord;
  int8_t outputPowerDbm;
  uint16_t preambleLength;
  uint32_t ackTimeoutMs;
};

struct CropConfig {
  GreenhouseLogic::CropProfileId profile;
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
  "/boot_log.csv",
    300000UL,
    2000UL,
    5000UL,
    10000UL,
};

constexpr DiagnosticsConfig DIAGNOSTICS{
  4,
  6,
  1500UL,
  30000UL,
};

constexpr ReliabilityConfig RELIABILITY{
  true,
  true,
  15000UL,
  20000UL,
  60000UL,
  3,
  true,
  true,
  3,
  30000UL,
  20000UL,
  900000UL,
  60000UL,
  1200UL,
  4000UL,
  2,
};

constexpr BatteryMonitorConfig BATTERY{
  true,
  1,
  4.9F * 1.045F,
  0.0F,
  false,
  3.45F,
  3.30F,
  4.20F,
  3.20F,
  16,
};

constexpr MqttConfig MQTT{
  "",
  1883,
  "",
  "",
  "mini-greenhouse",
  "greenhouse/mini",
  "homeassistant",
  true,
  10000UL,
  30000UL,
  true,
};

constexpr LoRaConfig LORA{
  false,
  "mini-greenhouse-01",
  3,
  5000UL,
  915.0F,
  125.0F,
  9,
  7,
  0x12,
  10,
  8,
  250UL,
};

constexpr CropConfig CROP{
  GreenhouseLogic::CropProfileId::tomato,
};

constexpr SystemConfig SYSTEM{
  false,
  true,
  false,
  false,
  false,
  false,
  false,
};

constexpr GreenhouseLogic::SystemConfig controlSystemConfig() {
  return GreenhouseLogic::SystemConfig{
      SYSTEM.enableDefogger,
      SYSTEM.enableGrowLight,
      SYSTEM.enableCirculationFans,
  };
}

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t BME280_I2C_ADDRESS = 0x76;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 64;

}  // namespace Settings
