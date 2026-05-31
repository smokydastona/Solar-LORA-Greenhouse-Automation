#include "runtime/SensorHal.h"

#include <Wire.h>

#include "ControllerRuntime.h"
#include "Settings.h"

namespace GreenhouseHal {

namespace {

bool validAirReading(float tempC, float humidityPct) {
  return !isnan(tempC) && !isnan(humidityPct) && tempC > -40.0F && tempC < 85.0F && humidityPct >= 0.0F && humidityPct <= 100.0F;
}

}  // namespace

SensorHardwareState initSensors(Adafruit_BME280 &bme,
                                DHT &dht,
                                BH1750 &lightSensor,
                                DallasTemperature &waterSensor) {
  SensorHardwareState state{};

  if (Settings::SYSTEM.enableBme280) {
    state.bmeReady = bme.begin(Settings::BME280_I2C_ADDRESS, &Wire);
  }

  if (Settings::SYSTEM.enableDht22) {
    dht.begin();
    state.dhtReady = true;
  }

  if (Settings::SYSTEM.enableBh1750) {
    state.lightReady = lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, Settings::BH1750_ADDRESS, &Wire);
  }

  if (Settings::SYSTEM.enableWaterProbe) {
    waterSensor.begin();
    state.waterReady = waterSensor.getDeviceCount() > 0;
  }

  return state;
}

GreenhouseLogic::SensorSnapshot readSensors(uint32_t now,
                                            Adafruit_BME280 &bme,
                                            DHT &dht,
                                            BH1750 &lightSensor,
                                            DallasTemperature &waterSensor,
                                            const SensorHardwareState &hardwareState,
                                            SensorFaultState &faultState,
                                            SensorCacheState &cacheState) {
  GreenhouseLogic::SensorSnapshot result;
  bool airValidNow = false;

  if (hardwareState.bmeReady) {
    result.airTempC = bme.readTemperature();
    result.humidityPct = bme.readHumidity();
    airValidNow = validAirReading(result.airTempC, result.humidityPct);
    if (!airValidNow && faultState.bmeReadFailures < UINT32_MAX) {
      ++faultState.bmeReadFailures;
    }
  }

  if (!airValidNow && hardwareState.dhtReady) {
    result.airTempC = dht.readTemperature();
    result.humidityPct = dht.readHumidity();
    airValidNow = validAirReading(result.airTempC, result.humidityPct);
    if (!airValidNow && faultState.dhtReadFailures < UINT32_MAX) {
      ++faultState.dhtReadFailures;
    }
  }

  if (!airValidNow && (hardwareState.bmeReady || hardwareState.dhtReady) && faultState.airReadFailures < UINT32_MAX) {
    ++faultState.airReadFailures;
  }

  if (airValidNow) {
    cacheState.lastAirTempC = result.airTempC;
    cacheState.lastHumidityPct = result.humidityPct;
    cacheState.lastAirAt = now;
  }
  result.airAgeMs = GreenhouseRuntime::sampleAgeMs(now, cacheState.lastAirAt);
  if (GreenhouseRuntime::sampleFresh(now, cacheState.lastAirAt, Settings::RELIABILITY.airDataMaxAgeMs)) {
    result.airTempC = cacheState.lastAirTempC;
    result.humidityPct = cacheState.lastHumidityPct;
    result.airAvailable = true;
  }

  if (hardwareState.lightReady) {
    result.lightLux = lightSensor.readLightLevel();
    if (result.lightLux >= 0.0F && result.lightLux < 200000.0F) {
      cacheState.lastLightLux = result.lightLux;
      cacheState.lastLightAt = now;
    } else if (faultState.lightReadFailures < UINT32_MAX) {
      ++faultState.lightReadFailures;
    }
  }
  result.lightAgeMs = GreenhouseRuntime::sampleAgeMs(now, cacheState.lastLightAt);
  if (GreenhouseRuntime::sampleFresh(now, cacheState.lastLightAt, Settings::RELIABILITY.lightDataMaxAgeMs)) {
    result.lightLux = cacheState.lastLightLux;
    result.lightAvailable = true;
  }

  if (hardwareState.waterReady) {
    waterSensor.requestTemperatures();
    result.waterTempC = waterSensor.getTempCByIndex(0);
    if (result.waterTempC > -100.0F && result.waterTempC < 125.0F) {
      cacheState.lastWaterTempC = result.waterTempC;
      cacheState.lastWaterAt = now;
    } else if (faultState.waterReadFailures < UINT32_MAX) {
      ++faultState.waterReadFailures;
    }
  }
  result.waterAgeMs = GreenhouseRuntime::sampleAgeMs(now, cacheState.lastWaterAt);
  if (GreenhouseRuntime::sampleFresh(now, cacheState.lastWaterAt, Settings::RELIABILITY.waterDataMaxAgeMs)) {
    result.waterTempC = cacheState.lastWaterTempC;
    result.waterAvailable = true;
  }

  return result;
}

}  // namespace GreenhouseHal