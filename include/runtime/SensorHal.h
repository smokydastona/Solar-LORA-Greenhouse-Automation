#pragma once

#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <DallasTemperature.h>
#include <DHT.h>

#include "ControlLogic.h"

namespace GreenhouseHal {

struct SensorHardwareState {
  bool bmeReady = false;
  bool dhtReady = false;
  bool lightReady = false;
  bool waterReady = false;
};

struct SensorFaultState {
  uint8_t consecutiveAirFaults = 0;
  uint32_t lastAirFaultAt = 0;
  uint32_t lastAirRecoveryAt = 0;
  uint32_t airReadFailures = 0;
  uint32_t bmeReadFailures = 0;
  uint32_t dhtReadFailures = 0;
  uint32_t lightReadFailures = 0;
  uint32_t waterReadFailures = 0;
  bool airUnavailableLogged = false;
};

struct SensorCacheState {
  float lastAirTempC = 0.0F;
  float lastHumidityPct = 0.0F;
  float lastWaterTempC = 0.0F;
  float lastLightLux = 0.0F;
  uint32_t lastAirAt = 0;
  uint32_t lastWaterAt = 0;
  uint32_t lastLightAt = 0;
};

SensorHardwareState initSensors(Adafruit_BME280 &bme,
                                DHT &dht,
                                BH1750 &lightSensor,
                                DallasTemperature &waterSensor);

GreenhouseLogic::SensorSnapshot readSensors(uint32_t now,
                                            Adafruit_BME280 &bme,
                                            DHT &dht,
                                            BH1750 &lightSensor,
                                            DallasTemperature &waterSensor,
                                            const SensorHardwareState &hardwareState,
                                            SensorFaultState &faultState,
                                            SensorCacheState &cacheState);

}  // namespace GreenhouseHal