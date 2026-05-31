#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include <BH1750.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>

#include "ControlLogic.h"
#include "ControllerRuntime.h"
#include "LoRaLink.h"
#include "PinMap.h"
#include "Settings.h"

namespace {

using GreenhouseLogic::ActuatorState;
using GreenhouseLogic::ControlMode;
using GreenhouseLogic::CropProfile;
using GreenhouseLogic::CropStatus;
using GreenhouseLogic::GreenhouseMetrics;
using GreenhouseLogic::SensorSnapshot;
using GreenhouseRuntime::ConfigFault;
using GreenhouseRuntime::ControllerState;
using GreenhouseRuntime::PowerBudget;

struct BatteryState {
  float voltageV = 0.0F;
  int percent = 0;
  bool available = false;
  bool calibrated = false;
  bool low = false;
  bool critical = false;
};

enum class SafeModeReason : uint8_t {
  none = 0,
  manual,
  bootLoop,
  brownout,
  powerRecovery,
  configFault,
  sensorFault,
  servoProtect,
};

struct SensorFaultState {
  uint8_t consecutiveAirFaults = 0;
  uint32_t lastAirFaultAt = 0;
  uint32_t lastAirRecoveryAt = 0;
};

struct ServoProtectionState {
  int topTargetDegrees = 0;
  int bottomTargetDegrees = 0;
  bool motionActive = false;
  uint32_t motionEndsAt = 0;
  uint32_t cooldownEndsAt = 0;
  uint8_t protectionTrips = 0;
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

const char *resetReasonLabel(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXTERNAL";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "OTHER";
  }
}

const char *cropStatusLabel(CropStatus status) {
  switch (status) {
    case CropStatus::optimal:
      return "OPTIMAL";
    case CropStatus::acceptable:
      return "ACCEPTABLE";
    case CropStatus::stressed:
      return "STRESSED";
    case CropStatus::unavailable:
      return "UNAVAILABLE";
  }
  return "UNKNOWN";
}

const char *safeModeReasonLabel(SafeModeReason reason) {
  switch (reason) {
    case SafeModeReason::none:
      return "NONE";
    case SafeModeReason::manual:
      return "MANUAL";
    case SafeModeReason::bootLoop:
      return "BOOT";
    case SafeModeReason::brownout:
      return "BROWNOUT";
    case SafeModeReason::powerRecovery:
      return "RECOVERY";
    case SafeModeReason::configFault:
      return "CONFIG";
    case SafeModeReason::sensorFault:
      return "SENSOR";
    case SafeModeReason::servoProtect:
      return "SERVO";
  }
  return "NONE";
}

bool validAirReading(float tempC, float humidityPct) {
  return !isnan(tempC) && !isnan(humidityPct) && tempC > -40.0F && tempC < 85.0F && humidityPct >= 0.0F && humidityPct <= 100.0F;
}

class Button {
 public:
  explicit Button(gpio_num_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, INPUT_PULLUP);
    lastState_ = digitalRead(pin_);
  }

  bool pressed() {
    const bool current = digitalRead(pin_);
    const uint32_t now = millis();
    if (current != lastState_) {
      lastBounceAt_ = now;
      lastState_ = current;
    }
    if ((now - lastBounceAt_) > debounceMs_ && current == LOW && !latched_) {
      latched_ = true;
      return true;
    }
    if (current == HIGH) {
      latched_ = false;
    }
    return false;
  }

 private:
  gpio_num_t pin_;
  bool lastState_ = HIGH;
  bool latched_ = false;
  uint32_t lastBounceAt_ = 0;
  static constexpr uint32_t debounceMs_ = 40;
};

class GreenhouseController {
 public:
  void begin() {
    Serial.begin(115200);
    delay(200);
    pinMode(PinMap::STATUS_LED, OUTPUT);
    digitalWrite(PinMap::STATUS_LED, HIGH);
    lastResetReason_ = esp_reset_reason();

    modeButton_.begin();
    forceOpenButton_.begin();
    forceCloseButton_.begin();

    configureOutputs();
    Wire.begin(PinMap::I2C_SDA, PinMap::I2C_SCL);
    initFilesystem();
    initPreferences();
    applyConfigurationSafety();
    initReliability();
    initSensors();
    initDisplay();
    primeServoTargets();
    if (!holdsServosForInspection()) {
      initServos();
    }
    initWifi();
    initMqtt();
    initLoRa();

    digitalWrite(PinMap::STATUS_LED, LOW);
    writeCsvHeaderIfNeeded();
    snapshot_ = readSensors(millis());
    evaluateSensorFaults(millis());
    battery_ = readBatteryState();
    metrics_ = GreenhouseLogic::evaluateMetrics(snapshot_, Settings::CROP.profile);
    computeOutputs();
    applyActuatorState();
    appendBootEvent();
    markProgress(millis());
    renderDisplay(true);
  }

  void update() {
    const uint32_t now = millis();
    feedHardwareWatchdog();
    verifySoftwareWatchdog(now);
    serviceServoProtection(now);
    pollButtons();

    if (now - lastControlAt_ >= Settings::LOGGING.controlIntervalMs) {
      lastControlAt_ = now;
      snapshot_ = readSensors(now);
      evaluateSensorFaults(now);
      battery_ = readBatteryState();
      metrics_ = GreenhouseLogic::evaluateMetrics(snapshot_, Settings::CROP.profile);
      computeOutputs();
      applyActuatorState();
      markProgress(now);
    }

    if (now - lastDisplayAt_ >= Settings::LOGGING.displayIntervalMs) {
      lastDisplayAt_ = now;
      renderDisplay(false);
      markProgress(now);
    }

    if (now - lastLogAt_ >= Settings::LOGGING.sampleIntervalMs) {
      lastLogAt_ = now;
      appendLogRecord();
      markProgress(now);
    }

    if (otaReady_ && now - lastOtaAt_ >= Settings::LOGGING.otaPollIntervalMs) {
      lastOtaAt_ = now;
      ArduinoOTA.handle();
      markProgress(now);
    }

    serviceMqtt(now);
    serviceLoRa(now);

    if (now - lastTelemetryPublishAt_ >= Settings::MQTT.publishIntervalMs) {
      lastTelemetryPublishAt_ = now;
      publishTelemetry(now);
      markProgress(now);
    }

    if (!bootHealthy_ && now >= Settings::RELIABILITY.bootHealthyAfterMs) {
      markBootHealthy();
    }
  }

 private:
  void configureOutputs() {
    const gpio_num_t outputs[] = {
        PinMap::FAN_EXHAUST,
        PinMap::FAN_INTAKE,
        PinMap::HEATER_DEFOGGER,
        PinMap::GROW_LIGHT,
        PinMap::CIRCULATION_FANS,
    };
    for (const auto pin : outputs) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }

  void initFilesystem() {
    filesystemReady_ = LittleFS.begin(false);
    if (!filesystemReady_) {
      Serial.println("LittleFS mount failed; leaving logs disabled until storage is repaired.");
    }
  }

  void initSensors() {
    initBatteryMonitor();

    if (Settings::SYSTEM.enableBme280) {
      bmeReady_ = bme_.begin(Settings::BME280_I2C_ADDRESS, &Wire);
    }

    if (Settings::SYSTEM.enableDht22) {
      dht_.begin();
      dhtReady_ = true;
    }

    if (Settings::SYSTEM.enableBh1750) {
      lightReady_ = lightSensor_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, Settings::BH1750_ADDRESS, &Wire);
    }

    if (Settings::SYSTEM.enableWaterProbe) {
      waterSensor_.begin();
      waterReady_ = waterSensor_.getDeviceCount() > 0;
    }
  }

  void initDisplay() {
    pinMode(PinMap::OLED_VEXT, OUTPUT);
    digitalWrite(PinMap::OLED_VEXT, LOW);
    delay(50);

    pinMode(PinMap::OLED_RESET, OUTPUT);
    digitalWrite(PinMap::OLED_RESET, LOW);
    delay(20);
    digitalWrite(PinMap::OLED_RESET, HIGH);
    delay(20);

    displayReady_ = display_.begin(SSD1306_SWITCHCAPVCC, Settings::OLED_ADDRESS);
    if (displayReady_) {
      display_.clearDisplay();
      display_.setTextColor(SSD1306_WHITE);
      display_.setTextSize(1);
      display_.display();
    }
  }

  void primeServoTargets() {
    servoProtection_.topTargetDegrees = Settings::SERVOS.topClosedDegrees;
    servoProtection_.bottomTargetDegrees = Settings::SERVOS.bottomClosedDegrees;
  }

  void initServos() {
    topServo_.setPeriodHertz(50);
    bottomServo_.setPeriodHertz(50);
    topServo_.attach(PinMap::SERVO_TOP_VENT, 500, 2400);
    bottomServo_.attach(PinMap::SERVO_BOTTOM_VENT, 500, 2400);

    topServo_.write(servoProtection_.topTargetDegrees);
    bottomServo_.write(servoProtection_.bottomTargetDegrees);
    servoProtection_.motionActive = true;
    servoProtection_.motionEndsAt = millis() + Settings::RELIABILITY.servoDriveWindowMs;
    servoProtection_.cooldownEndsAt = servoProtection_.motionEndsAt + Settings::RELIABILITY.servoCooldownMs;
    preferences_.putBool("servo_pending", true);
    preferences_.putBool("servo_top_open", false);
    preferences_.putBool("servo_bottom_open", false);
  }

  void initPreferences() {
    preferences_.begin("greenhouse", false);
    mode_ = static_cast<ControlMode>(preferences_.getUChar("mode", static_cast<uint8_t>(ControlMode::automatic)));

    bootCount_ = preferences_.getUInt("boot_count", 0U) + 1U;
    preferences_.putUInt("boot_count", bootCount_);

    const bool priorBootPending = preferences_.getBool("boot_pending", false);
    bootFailures_ = preferences_.getUChar("boot_fail", 0U);
    if (priorBootPending && bootFailures_ < 255U) {
      ++bootFailures_;
    } else if (!priorBootPending) {
      bootFailures_ = 0U;
    }

    preferences_.putBool("boot_pending", true);
    preferences_.putUChar("boot_fail", bootFailures_);
    preferences_.putUInt("last_reset", static_cast<uint32_t>(lastResetReason_));

    const bool servoPending = preferences_.getBool("servo_pending", false);
    if (servoPending) {
      rehomeAfterUnsafeReset_ = true;
      safeMode_ = true;
      safeModeReason_ = SafeModeReason::powerRecovery;
    }

    if (manualSafeModeRequested()) {
      enterSafeMode(SafeModeReason::manual);
    } else if (bootFailures_ >= Settings::RELIABILITY.safeModeAfterBootFailures) {
      enterSafeMode(SafeModeReason::bootLoop);
    } else if (Settings::RELIABILITY.brownoutEntersSafeMode && lastResetReason_ == ESP_RST_BROWNOUT) {
      rehomeAfterUnsafeReset_ = true;
      enterSafeMode(SafeModeReason::brownout);
    }
  }

  void initReliability() {
    markProgress(millis());
    if (!Settings::RELIABILITY.enableHardwareWatchdog) {
      return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t config{};
    config.timeout_ms = Settings::RELIABILITY.hardwareWatchdogTimeoutMs;
    config.idle_core_mask = 0;
    config.trigger_panic = true;
    esp_task_wdt_init(&config);
#else
    esp_task_wdt_init((Settings::RELIABILITY.hardwareWatchdogTimeoutMs + 999UL) / 1000UL, true);
#endif
    esp_task_wdt_add(nullptr);
  }

  void initBatteryMonitor() {
    if (!Settings::BATTERY.enableVoltageMonitor) {
      return;
    }

    pinMode(PinMap::BATTERY_ADC_CTRL, OUTPUT);
    digitalWrite(PinMap::BATTERY_ADC_CTRL, HIGH);
  }

  void initWifi() {
    if (strlen(Settings::WIFI.ssid) == 0) {
      return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(Settings::WIFI.hostname);
    WiFi.begin(Settings::WIFI.ssid, Settings::WIFI.password);

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 10000UL) {
      feedHardwareWatchdog();
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      configTzTime(Settings::WIFI.tzInfo, "pool.ntp.org", "time.nist.gov");
      if (Settings::WIFI.enableOta) {
        ArduinoOTA.setHostname(Settings::WIFI.hostname);
        ArduinoOTA.begin();
        otaReady_ = true;
      }
    }
  }

  void initMqtt() {
    mqttEnabled_ = strlen(Settings::MQTT.host) > 0;
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setBufferSize(1536);
    if (mqttEnabled_) {
      mqttClient_.setServer(Settings::MQTT.host, Settings::MQTT.port);
    }
  }

  void initLoRa() {
    loraLink_.begin(Settings::LORA.enableTransport,
                    Settings::LORA.nodeId,
                    Settings::LORA.maxRetries,
                    Settings::LORA.retryBackoffMs,
                    bootCount_ ^ static_cast<uint32_t>(lastResetReason_));
  }

  void pollButtons() {
    if (safeMode_ && modeButton_.pressed()) {
      clearBootFailureState();
      ESP.restart();
      return;
    }
    if (modeButton_.pressed()) {
      setMode(ControlMode::automatic);
    }
    if (forceOpenButton_.pressed()) {
      setMode(ControlMode::forceOpen);
    }
    if (forceCloseButton_.pressed()) {
      setMode(ControlMode::forceClosed);
    }
  }

  void setMode(ControlMode mode) {
    mode_ = mode;
    preferences_.putUChar("mode", static_cast<uint8_t>(mode_));
    renderDisplay(true);
  }

  SensorSnapshot readSensors(uint32_t now) {
    SensorSnapshot result;
    bool airValidNow = false;

    if (bmeReady_) {
      result.airTempC = bme_.readTemperature();
      result.humidityPct = bme_.readHumidity();
      airValidNow = validAirReading(result.airTempC, result.humidityPct);
    }

    if (!airValidNow && dhtReady_) {
      result.airTempC = dht_.readTemperature();
      result.humidityPct = dht_.readHumidity();
      airValidNow = validAirReading(result.airTempC, result.humidityPct);
    }

    if (airValidNow) {
      sensorCache_.lastAirTempC = result.airTempC;
      sensorCache_.lastHumidityPct = result.humidityPct;
      sensorCache_.lastAirAt = now;
    }
    result.airAgeMs = GreenhouseRuntime::sampleAgeMs(now, sensorCache_.lastAirAt);
    if (GreenhouseRuntime::sampleFresh(now, sensorCache_.lastAirAt, Settings::RELIABILITY.airDataMaxAgeMs)) {
      result.airTempC = sensorCache_.lastAirTempC;
      result.humidityPct = sensorCache_.lastHumidityPct;
      result.airAvailable = true;
    }

    if (lightReady_) {
      result.lightLux = lightSensor_.readLightLevel();
      if (result.lightLux >= 0.0F && result.lightLux < 200000.0F) {
        sensorCache_.lastLightLux = result.lightLux;
        sensorCache_.lastLightAt = now;
      }
    }
    result.lightAgeMs = GreenhouseRuntime::sampleAgeMs(now, sensorCache_.lastLightAt);
    if (GreenhouseRuntime::sampleFresh(now, sensorCache_.lastLightAt, Settings::RELIABILITY.lightDataMaxAgeMs)) {
      result.lightLux = sensorCache_.lastLightLux;
      result.lightAvailable = true;
    }

    if (waterReady_) {
      waterSensor_.requestTemperatures();
      result.waterTempC = waterSensor_.getTempCByIndex(0);
      if (result.waterTempC > -100.0F && result.waterTempC < 125.0F) {
        sensorCache_.lastWaterTempC = result.waterTempC;
        sensorCache_.lastWaterAt = now;
      }
    }
    result.waterAgeMs = GreenhouseRuntime::sampleAgeMs(now, sensorCache_.lastWaterAt);
    if (GreenhouseRuntime::sampleFresh(now, sensorCache_.lastWaterAt, Settings::RELIABILITY.waterDataMaxAgeMs)) {
      result.waterTempC = sensorCache_.lastWaterTempC;
      result.waterAvailable = true;
    }

    return result;
  }

  void applyConfigurationSafety() {
    GreenhouseRuntime::ValidationInput input{};
    input.climate = Settings::CLIMATE;
    input.freshness.airMaxAgeMs = Settings::RELIABILITY.airDataMaxAgeMs;
    input.freshness.waterMaxAgeMs = Settings::RELIABILITY.waterDataMaxAgeMs;
    input.freshness.lightMaxAgeMs = Settings::RELIABILITY.lightDataMaxAgeMs;
    input.sampleIntervalMs = Settings::LOGGING.sampleIntervalMs;
    input.displayIntervalMs = Settings::LOGGING.displayIntervalMs;
    input.controlIntervalMs = Settings::LOGGING.controlIntervalMs;
    input.otaPollIntervalMs = Settings::LOGGING.otaPollIntervalMs;
    input.mqttPublishIntervalMs = Settings::MQTT.publishIntervalMs;
    input.loraMaxRetries = Settings::LORA.maxRetries;
    input.loraRetryBackoffMs = Settings::LORA.retryBackoffMs;
    input.batteryLowVoltage = Settings::BATTERY.lowVoltage;
    input.batteryCriticalVoltage = Settings::BATTERY.criticalVoltage;
    input.batteryFullVoltage = Settings::BATTERY.fullVoltage;
    input.batteryEmptyVoltage = Settings::BATTERY.emptyVoltage;
    input.topClosedDegrees = Settings::SERVOS.topClosedDegrees;
    input.topOpenDegrees = Settings::SERVOS.topOpenDegrees;
    input.bottomClosedDegrees = Settings::SERVOS.bottomClosedDegrees;
    input.bottomOpenDegrees = Settings::SERVOS.bottomOpenDegrees;

    configFault_ = GreenhouseRuntime::validateConfiguration(input);
    if (configFault_ != ConfigFault::none) {
      enterSafeMode(SafeModeReason::configFault);
    }
  }

  void evaluateSensorFaults(uint32_t now) {
    if (snapshot_.airAvailable) {
      if (sensorFaults_.lastAirFaultAt > 0 &&
          now - sensorFaults_.lastAirFaultAt >= Settings::RELIABILITY.sensorFaultRecoveryDelayMs) {
        sensorFaults_.consecutiveAirFaults = 0;
      }
      sensorFaults_.lastAirRecoveryAt = now;
      return;
    }

    sensorFaults_.lastAirFaultAt = now;
    if (sensorFaults_.consecutiveAirFaults < 255U) {
      ++sensorFaults_.consecutiveAirFaults;
    }

    if (!safeMode_ &&
        Settings::RELIABILITY.airSensorFaultEntersSafeMode &&
        sensorFaults_.consecutiveAirFaults >= Settings::RELIABILITY.maxConsecutiveAirSensorFaults) {
      enterSafeMode(SafeModeReason::sensorFault);
    }
  }

  bool currentLocalTime(struct tm *timeInfo) const {
    return getLocalTime(timeInfo, 10);
  }

  void computeOutputs() {
    if (safeMode_) {
      controllerState_ = GreenhouseRuntime::resolveControllerState(true, battery_.available, battery_.low, battery_.critical, mode_);
      powerBudget_ = GreenhouseRuntime::evaluatePowerBudget(battery_.available, battery_.low, battery_.critical);
      actuators_ = safeActuators();
      return;
    }

    controllerState_ = GreenhouseRuntime::resolveControllerState(false, battery_.available, battery_.low, battery_.critical, mode_);
    powerBudget_ = GreenhouseRuntime::evaluatePowerBudget(battery_.available, battery_.low, battery_.critical);

    struct tm timeInfo {};
    const bool hasTime = currentLocalTime(&timeInfo);
    const uint32_t currentMinute = hasTime ? static_cast<uint32_t>(timeInfo.tm_hour * 60U + timeInfo.tm_min) : 0U;
    const bool daylight = GreenhouseLogic::resolveDaylight(snapshot_, Settings::CLIMATE, hasTime, currentMinute);

    actuators_ = GreenhouseLogic::evaluateActuators({
        snapshot_,
        actuators_,
        mode_,
        Settings::CLIMATE,
        Settings::controlSystemConfig(),
        daylight,
        hasTime,
        currentMinute,
    });
  }

  void applyActuatorState() {
    if (safeMode_) {
      effectiveActuators_ = safeActuators();
      if (safeModeAllowsServoMovement()) {
        requestServoTargets(effectiveActuators_.topVentOpen, effectiveActuators_.bottomVentOpen, true);
      } else {
        holdServosForInspection();
      }
      digitalWrite(PinMap::FAN_EXHAUST, LOW);
      digitalWrite(PinMap::FAN_INTAKE, LOW);
      digitalWrite(PinMap::HEATER_DEFOGGER, LOW);
      digitalWrite(PinMap::GROW_LIGHT, LOW);
      digitalWrite(PinMap::CIRCULATION_FANS, LOW);
      return;
    }

    effectiveActuators_ = actuators_;
    effectiveActuators_.exhaustFanOn = powerBudget_.allowVentFans && effectiveActuators_.exhaustFanOn;
    effectiveActuators_.intakeFanOn = powerBudget_.allowVentFans && effectiveActuators_.intakeFanOn;
    effectiveActuators_.defoggerOn = Settings::SYSTEM.enableDefogger && powerBudget_.allowDefogger && effectiveActuators_.defoggerOn;
    effectiveActuators_.growLightOn = Settings::SYSTEM.enableGrowLight && powerBudget_.allowGrowLight && effectiveActuators_.growLightOn;
    effectiveActuators_.circulationFansOn = Settings::SYSTEM.enableCirculationFans && powerBudget_.allowCirculation && effectiveActuators_.circulationFansOn;

    if (powerBudget_.allowServoMoves) {
      requestServoTargets(effectiveActuators_.topVentOpen, effectiveActuators_.bottomVentOpen, false);
    } else {
      holdServosForInspection();
    }

    digitalWrite(PinMap::FAN_EXHAUST, effectiveActuators_.exhaustFanOn ? HIGH : LOW);
    digitalWrite(PinMap::FAN_INTAKE, effectiveActuators_.intakeFanOn ? HIGH : LOW);
    digitalWrite(PinMap::HEATER_DEFOGGER, effectiveActuators_.defoggerOn ? HIGH : LOW);
    digitalWrite(PinMap::GROW_LIGHT, effectiveActuators_.growLightOn ? HIGH : LOW);
    digitalWrite(PinMap::CIRCULATION_FANS, effectiveActuators_.circulationFansOn ? HIGH : LOW);
  }

  void requestServoTargets(bool topOpen, bool bottomOpen, bool force) {
    const uint32_t now = millis();
    const int topTarget = topOpen ? Settings::SERVOS.topOpenDegrees : Settings::SERVOS.topClosedDegrees;
    const int bottomTarget = bottomOpen ? Settings::SERVOS.bottomOpenDegrees : Settings::SERVOS.bottomClosedDegrees;

    if (topTarget == servoProtection_.topTargetDegrees &&
        bottomTarget == servoProtection_.bottomTargetDegrees &&
        !rehomeAfterUnsafeReset_) {
      return;
    }

    if (!force && (servoProtection_.motionActive || now < servoProtection_.cooldownEndsAt)) {
      if (servoProtection_.protectionTrips < 255U) {
        ++servoProtection_.protectionTrips;
      }
      if (servoProtection_.protectionTrips >= Settings::RELIABILITY.servoProtectionTripsBeforeSafeMode) {
        enterSafeMode(SafeModeReason::servoProtect);
      }
      return;
    }

    servoProtection_.protectionTrips = 0;
    if (!topServo_.attached()) {
      topServo_.attach(PinMap::SERVO_TOP_VENT, 500, 2400);
    }
    if (!bottomServo_.attached()) {
      bottomServo_.attach(PinMap::SERVO_BOTTOM_VENT, 500, 2400);
    }

    topServo_.write(topTarget);
    bottomServo_.write(bottomTarget);
    servoProtection_.topTargetDegrees = topTarget;
    servoProtection_.bottomTargetDegrees = bottomTarget;
    servoProtection_.motionActive = true;
    servoProtection_.motionEndsAt = now + Settings::RELIABILITY.servoDriveWindowMs;
    servoProtection_.cooldownEndsAt = servoProtection_.motionEndsAt + Settings::RELIABILITY.servoCooldownMs;
    preferences_.putBool("servo_pending", true);
    preferences_.putBool("servo_top_open", topOpen);
    preferences_.putBool("servo_bottom_open", bottomOpen);
    rehomeAfterUnsafeReset_ = false;
  }

  void serviceServoProtection(uint32_t now) {
    if (!servoProtection_.motionActive || now < servoProtection_.motionEndsAt) {
      return;
    }

    if (topServo_.attached()) {
      topServo_.detach();
    }
    if (bottomServo_.attached()) {
      bottomServo_.detach();
    }
    servoProtection_.motionActive = false;
    preferences_.putBool("servo_pending", false);
  }

  void renderDisplay(bool fullRefresh) {
    if (!displayReady_) {
      return;
    }

    char airBuffer[24];
    char waterBuffer[16];
    char lightBuffer[16];
    char statusBuffer[20];
    char vpdBuffer[16];
    char dewBuffer[16];
    char batteryBuffer[16];
    formatMeasurement(airBuffer, sizeof(airBuffer), snapshot_.airAvailable, snapshot_.airTempC, 1, "C");
    if (snapshot_.airAvailable) {
      snprintf(airBuffer,
               sizeof(airBuffer),
               "%.1fC %.0f%%",
               static_cast<double>(snapshot_.airTempC),
               static_cast<double>(snapshot_.humidityPct));
    }
    formatMeasurement(waterBuffer, sizeof(waterBuffer), snapshot_.waterAvailable, snapshot_.waterTempC, 1, "C");
    formatMeasurement(lightBuffer, sizeof(lightBuffer), snapshot_.lightAvailable, snapshot_.lightLux, 0, " lx");
    formatMeasurement(vpdBuffer, sizeof(vpdBuffer), metrics_.vpdAvailable, metrics_.vaporPressureDeficitKPa, 2, "kPa");
    formatMeasurement(dewBuffer, sizeof(dewBuffer), metrics_.dewPointAvailable, metrics_.dewPointC, 1, "C");
    if (battery_.available) {
      snprintf(batteryBuffer,
               sizeof(batteryBuffer),
               "%d%% %.2fV%s",
               battery_.percent,
               static_cast<double>(battery_.voltageV),
               battery_.calibrated ? "" : "?");
    } else {
      snprintf(batteryBuffer, sizeof(batteryBuffer), "N/A");
    }
    snprintf(statusBuffer,
             sizeof(statusBuffer),
             "%s%s%s",
             WiFi.status() == WL_CONNECTED ? "WiFi" : "OFF",
             mqttClient_.connected() ? "/MQTT" : "",
             filesystemReady_ ? "" : " FS!");

    if (fullRefresh) {
      display_.clearDisplay();
    }

    display_.clearDisplay();
    display_.setCursor(0, 0);
    if (safeMode_) {
      display_.printf("SAFE MODE\n");
      display_.printf("Reason:%s\n", safeModeReasonLabel(safeModeReason_));
      display_.printf("Reset:%s\n", resetReasonLabel(lastResetReason_));
      display_.printf("Boots:%u Fail:%u\n", static_cast<unsigned>(bootCount_), static_cast<unsigned>(bootFailures_));
      display_.printf("Batt:%s\n", batteryBuffer);
      display_.printf("Air: %s\n", airBuffer);
      display_.printf("MODE=resume");
    } else if (((millis() / Settings::LOGGING.displayIntervalMs) % 2U) == 0U) {
      display_.printf("State:%s\n", GreenhouseRuntime::controllerStateLabel(controllerState_));
      display_.printf("Air:  %s\n", airBuffer);
      display_.printf("Water: %s\n", waterBuffer);
      display_.printf("Light: %s\n", lightBuffer);
      display_.printf("Vent: %s/%s\n",
                      effectiveActuators_.topVentOpen ? "OPEN" : "SHUT",
                      effectiveActuators_.bottomVentOpen ? "OPEN" : "SHUT");
      display_.printf("Fans: %s%s%s\n",
                      effectiveActuators_.intakeFanOn ? "I" : "-",
                      effectiveActuators_.exhaustFanOn ? "E" : "-",
                      effectiveActuators_.circulationFansOn ? "C" : "-");
      display_.printf("Heat:%c Light:%c %s",
                      effectiveActuators_.defoggerOn ? 'Y' : 'N',
                      effectiveActuators_.growLightOn ? 'Y' : 'N',
                      statusBuffer);
    } else {
      const CropProfile &profile = GreenhouseLogic::cropProfile(Settings::CROP.profile);
      display_.printf("Crop: %s\n", profile.name);
      display_.printf("State:%s\n", cropStatusLabel(metrics_.cropStatus));
      display_.printf("VPD:  %s\n", vpdBuffer);
      display_.printf("Dew:  %s\n", dewBuffer);
      display_.printf("Frost:%s\n", metrics_.frostRisk ? "YES" : "NO");
      display_.printf("Batt: %s\n", batteryBuffer);
      display_.printf("Health:%d %s", computeHealthScore(), statusBuffer);
    }
    display_.display();
  }

  void formatMeasurement(char *buffer,
                         size_t bufferSize,
                         bool available,
                         float value,
                         uint8_t decimals,
                         const char *suffix) const {
    if (!available) {
      snprintf(buffer, bufferSize, "N/A");
      return;
    }

    snprintf(buffer,
             bufferSize,
             "%.*f%s",
             static_cast<int>(decimals),
             static_cast<double>(value),
             suffix);
  }

  const char *modeLabel() const {
    switch (mode_) {
      case ControlMode::automatic:
        return "AUTO";
      case ControlMode::forceOpen:
        return "OPEN";
      case ControlMode::forceClosed:
        return "CLOSED";
    }
    return "?";
  }

  void writeCsvHeaderIfNeeded() {
    if (!filesystemReady_) {
      return;
    }
    if (LittleFS.exists(Settings::LOGGING.path)) {
      return;
    }
    File file = LittleFS.open(Settings::LOGGING.path, FILE_WRITE);
    if (!file) {
      return;
    }
    file.println("millis,mode,controller_state,safe_mode,safe_reason,reset_reason,boot_failures,air_available,air_temp_c,humidity_pct,water_available,water_temp_c,light_available,light_lux,vpd_kpa,dew_point_c,frost_risk,crop_status,battery_available,battery_v,battery_pct,health_score,top_open,bottom_open,fan_exhaust,fan_intake,defogger,grow_light,circulation,lora_queue,lora_sent,lora_dropped");
    file.close();
  }

  void appendBootEvent() {
    if (!filesystemReady_) {
      return;
    }
    File file = LittleFS.open(Settings::LOGGING.bootLogPath, FILE_APPEND);
    if (!file) {
      return;
    }
    if (file.size() == 0) {
      file.println("millis,boot_count,boot_failures,reset_reason,safe_mode,safe_reason");
    }
    file.printf("%lu,%lu,%u,%s,%u,%s\n",
                millis(),
                static_cast<unsigned long>(bootCount_),
                static_cast<unsigned>(bootFailures_),
                resetReasonLabel(lastResetReason_),
                safeMode_,
                safeModeReasonLabel(safeModeReason_));
    file.close();
  }

  void appendLogRecord() {
    if (!filesystemReady_) {
      return;
    }
    File file = LittleFS.open(Settings::LOGGING.path, FILE_APPEND);
    if (!file) {
      return;
    }
    file.printf("%lu,%s,%s,%u,%s,%s,%u,%u,%.2f,%.2f,%u,%.2f,%u,%.2f,%.2f,%.2f,%u,%s,%u,%.2f,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu\n",
                millis(),
                modeLabel(),
                GreenhouseRuntime::controllerStateLabel(controllerState_),
                safeMode_,
                safeModeReasonLabel(safeModeReason_),
                resetReasonLabel(lastResetReason_),
                static_cast<unsigned>(bootFailures_),
                snapshot_.airAvailable,
                snapshot_.airTempC,
                snapshot_.humidityPct,
                snapshot_.waterAvailable,
                snapshot_.waterTempC,
                snapshot_.lightAvailable,
                snapshot_.lightLux,
                metrics_.vpdAvailable ? metrics_.vaporPressureDeficitKPa : 0.0F,
                metrics_.dewPointAvailable ? metrics_.dewPointC : 0.0F,
                metrics_.frostRisk,
                cropStatusLabel(metrics_.cropStatus),
                battery_.available,
                battery_.voltageV,
                battery_.percent,
                computeHealthScore(),
                effectiveActuators_.topVentOpen,
                effectiveActuators_.bottomVentOpen,
                effectiveActuators_.exhaustFanOn,
                effectiveActuators_.intakeFanOn,
                effectiveActuators_.defoggerOn,
                effectiveActuators_.growLightOn,
                effectiveActuators_.circulationFansOn,
                static_cast<unsigned>(loraLink_.stats().queueDepth),
                static_cast<unsigned long>(loraLink_.stats().sentFrames),
                static_cast<unsigned long>(loraLink_.stats().droppedFrames));
    file.close();
  }

  bool manualSafeModeRequested() const {
    return digitalRead(PinMap::BUTTON_FORCE_OPEN) == LOW && digitalRead(PinMap::BUTTON_FORCE_CLOSE) == LOW;
  }

  bool safeModeAllowsServoMovement() const {
    return safeModeReason_ == SafeModeReason::sensorFault;
  }

  bool holdsServosForInspection() const {
    return safeMode_ && !safeModeAllowsServoMovement();
  }

  void holdServosForInspection() {
    if (topServo_.attached()) {
      topServo_.detach();
    }
    if (bottomServo_.attached()) {
      bottomServo_.detach();
    }
    servoProtection_.motionActive = false;
    servoProtection_.motionEndsAt = 0;
    servoProtection_.cooldownEndsAt = 0;
    preferences_.putBool("servo_pending", false);
  }

  ActuatorState safeActuators() const {
    ActuatorState actuators{};
    if (safeModeReason_ == SafeModeReason::sensorFault) {
      actuators.topVentOpen = true;
      actuators.bottomVentOpen = true;
    }
    return actuators;
  }

  void enterSafeMode(SafeModeReason reason) {
    safeMode_ = true;
    safeModeReason_ = reason;
  }

  void clearBootFailureState() {
    preferences_.putBool("boot_pending", false);
    preferences_.putUChar("boot_fail", 0U);
    preferences_.putBool("servo_pending", false);
  }

  void markBootHealthy() {
    preferences_.putBool("boot_pending", false);
    preferences_.putUChar("boot_fail", 0U);
    bootHealthy_ = true;
    bootFailures_ = 0U;
  }

  void markProgress(uint32_t now) {
    lastProgressAt_ = now;
    feedHardwareWatchdog();
  }

  void feedHardwareWatchdog() {
    if (Settings::RELIABILITY.enableHardwareWatchdog) {
      esp_task_wdt_reset();
    }
  }

  void verifySoftwareWatchdog(uint32_t now) {
    if (Settings::RELIABILITY.enableSoftwareWatchdog &&
        lastProgressAt_ > 0 &&
        now - lastProgressAt_ > Settings::RELIABILITY.softwareWatchdogTimeoutMs) {
      Serial.println("Software watchdog timeout; restarting controller.");
      ESP.restart();
    }
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

  void batteryAdcEnable() {
    digitalWrite(PinMap::BATTERY_ADC_CTRL, LOW);
    delay(10);
  }

  void batteryAdcDisable() {
    digitalWrite(PinMap::BATTERY_ADC_CTRL, HIGH);
  }

  int computeHealthScore() const {
    int score = 100;
    if (!snapshot_.airAvailable) {
      score -= 25;
    }
    if (!filesystemReady_) {
      score -= 15;
    }
    if (safeMode_) {
      score -= 35;
    }
    if (battery_.available && battery_.low) {
      score -= 15;
    }
    if (battery_.available && battery_.critical) {
      score -= 25;
    }
    if (WiFi.status() != WL_CONNECTED && strlen(Settings::WIFI.ssid) > 0) {
      score -= 10;
    }
    if (loraLink_.stats().enabled && !loraLink_.stats().backendReady) {
      score -= 10;
    }
    return score < 0 ? 0 : score;
  }

  void serviceMqtt(uint32_t now) {
    if (!mqttEnabled_) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
    if (!mqttClient_.connected()) {
      if (now - lastMqttConnectAttemptAt_ < Settings::MQTT.reconnectIntervalMs) {
        return;
      }
      lastMqttConnectAttemptAt_ = now;
      connectMqtt();
      return;
    }

    mqttClient_.loop();
  }

  void connectMqtt() {
    char availabilityTopic[96];
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability", Settings::MQTT.baseTopic);

    const bool connected = mqttClient_.connect(Settings::MQTT.clientId,
                                               Settings::MQTT.username,
                                               Settings::MQTT.password,
                                               availabilityTopic,
                                               1,
                                               true,
                                               "offline");
    if (!connected) {
      return;
    }

    mqttClient_.publish(availabilityTopic, "online", true);
    publishHomeAssistantDiscovery();
    lastTelemetryPublishAt_ = millis();
    publishTelemetry(lastTelemetryPublishAt_);
  }

  void publishHomeAssistantDiscovery() {
    if (!Settings::MQTT.enableHomeAssistantDiscovery || discoveryPublished_) {
      return;
    }

    publishHomeAssistantSensorDiscovery("sensor", "air_temp", "Air Temperature", "{{ value_json.air.temp_c }}", "temperature", "measurement", "°C");
    publishHomeAssistantSensorDiscovery("sensor", "humidity", "Humidity", "{{ value_json.air.humidity_pct }}", "humidity", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "vpd", "VPD", "{{ value_json.metrics.vpd_kpa }}", "", "measurement", "kPa");
    publishHomeAssistantSensorDiscovery("sensor", "dew_point", "Dew Point", "{{ value_json.metrics.dew_point_c }}", "temperature", "measurement", "°C");
    publishHomeAssistantSensorDiscovery("sensor", "battery_percentage", "Battery Percentage", "{{ value_json.battery.percent }}", "battery", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "health_score", "Health Score", "{{ value_json.health.score }}", "", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "crop_status", "Crop Status", "{{ value_json.crop.status }}", "", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "frost_risk", "Frost Risk", "{{ value_json.metrics.frost_risk }}", "cold", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "fan_exhaust", "Fan Exhaust", "{{ value_json.actuators.fan_exhaust }}", "", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "fan_intake", "Fan Intake", "{{ value_json.actuators.fan_intake }}", "", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "defogger", "Defogger", "{{ value_json.actuators.defogger }}", "heat", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "grow_light", "Grow Light", "{{ value_json.actuators.grow_light }}", "light", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "circulation", "Circulation", "{{ value_json.actuators.circulation }}", "running", "", "");
    discoveryPublished_ = true;
  }

  void publishHomeAssistantSensorDiscovery(const char *component,
                                           const char *objectId,
                                           const char *name,
                                           const char *valueTemplate,
                                           const char *deviceClass,
                                           const char *stateClass,
                                           const char *unit) {
    char topic[192];
    char payload[768];
    char stateTopic[96];
    char availabilityTopic[96];

    snprintf(topic,
             sizeof(topic),
             "%s/%s/%s_%s/config",
             Settings::MQTT.discoveryPrefix,
             component,
             Settings::MQTT.clientId,
             objectId);
    snprintf(stateTopic, sizeof(stateTopic), "%s/state", Settings::MQTT.baseTopic);
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability", Settings::MQTT.baseTopic);
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"%s\",\"uniq_id\":\"%s_%s\",\"stat_t\":\"%s\",\"avty_t\":\"%s\",\"val_tpl\":\"%s\",\"dev_cla\":\"%s\",\"stat_cla\":\"%s\",\"unit_of_meas\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Mini Greenhouse\",\"mdl\":\"ESP32-S3 Greenhouse Controller\",\"mf\":\"Solar LoRa Greenhouse Automation\"}}",
             name,
             Settings::MQTT.clientId,
             objectId,
             stateTopic,
             availabilityTopic,
             valueTemplate,
             deviceClass,
             stateClass,
             unit,
             Settings::MQTT.clientId);
    mqttClient_.publish(topic, payload, true);
  }

  void buildTelemetryPayload(char *payload, size_t payloadSize) {
    const CropProfile &profile = GreenhouseLogic::cropProfile(Settings::CROP.profile);
    snprintf(payload,
             payloadSize,
             "{\"mode\":\"%s\",\"controller_state\":\"%s\",\"safe_mode\":{\"active\":%s,\"reason\":\"%s\",\"boot_failures\":%u},\"reset_reason\":\"%s\",\"crop\":{\"profile\":\"%s\",\"status\":\"%s\"},\"air\":{\"available\":%s,\"age_ms\":%lu,\"temp_c\":%.2f,\"humidity_pct\":%.2f},\"water\":{\"available\":%s,\"age_ms\":%lu,\"temp_c\":%.2f},\"light\":{\"available\":%s,\"age_ms\":%lu,\"lux\":%.2f},\"metrics\":{\"vpd_kpa\":%.2f,\"dew_point_c\":%.2f,\"frost_risk\":%s},\"battery\":{\"available\":%s,\"calibrated\":%s,\"voltage_v\":%.2f,\"percent\":%d,\"low\":%s,\"critical\":%s},\"health\":{\"score\":%d},\"power_budget\":{\"servo_moves\":%s,\"vent_fans\":%s,\"defogger\":%s,\"grow_light\":%s,\"circulation\":%s},\"actuators\":{\"top_open\":%s,\"bottom_open\":%s,\"fan_exhaust\":%s,\"fan_intake\":%s,\"defogger\":%s,\"grow_light\":%s,\"circulation\":%s},\"connectivity\":{\"wifi\":%s,\"mqtt\":%s,\"ota\":%s,\"lora\":%s},\"storage\":{\"filesystem_ready\":%s},\"lora\":{\"session_id\":%lu,\"queue_depth\":%u,\"sent\":%lu,\"dropped\":%lu,\"duplicate_inbound\":%lu,\"invalid_inbound\":%lu},\"uptime_ms\":%lu}",
             modeLabel(),
             GreenhouseRuntime::controllerStateLabel(controllerState_),
             safeMode_ ? "true" : "false",
             safeModeReasonLabel(safeModeReason_),
             static_cast<unsigned>(bootFailures_),
             resetReasonLabel(lastResetReason_),
             profile.name,
             cropStatusLabel(metrics_.cropStatus),
             snapshot_.airAvailable ? "true" : "false",
             static_cast<unsigned long>(snapshot_.airAgeMs),
             snapshot_.airTempC,
             snapshot_.humidityPct,
             snapshot_.waterAvailable ? "true" : "false",
             static_cast<unsigned long>(snapshot_.waterAgeMs),
             snapshot_.waterTempC,
             snapshot_.lightAvailable ? "true" : "false",
             static_cast<unsigned long>(snapshot_.lightAgeMs),
             snapshot_.lightLux,
             metrics_.vpdAvailable ? metrics_.vaporPressureDeficitKPa : 0.0F,
             metrics_.dewPointAvailable ? metrics_.dewPointC : 0.0F,
             metrics_.frostRisk ? "true" : "false",
             battery_.available ? "true" : "false",
             battery_.calibrated ? "true" : "false",
             battery_.voltageV,
             battery_.percent,
             battery_.low ? "true" : "false",
             battery_.critical ? "true" : "false",
             computeHealthScore(),
             powerBudget_.allowServoMoves ? "true" : "false",
             powerBudget_.allowVentFans ? "true" : "false",
             powerBudget_.allowDefogger ? "true" : "false",
             powerBudget_.allowGrowLight ? "true" : "false",
             powerBudget_.allowCirculation ? "true" : "false",
             effectiveActuators_.topVentOpen ? "true" : "false",
             effectiveActuators_.bottomVentOpen ? "true" : "false",
             effectiveActuators_.exhaustFanOn ? "true" : "false",
             effectiveActuators_.intakeFanOn ? "true" : "false",
             effectiveActuators_.defoggerOn ? "true" : "false",
             effectiveActuators_.growLightOn ? "true" : "false",
             effectiveActuators_.circulationFansOn ? "true" : "false",
             WiFi.status() == WL_CONNECTED ? "true" : "false",
             mqttClient_.connected() ? "true" : "false",
             otaReady_ ? "true" : "false",
             loraLink_.stats().backendReady ? "true" : "false",
             filesystemReady_ ? "true" : "false",
             static_cast<unsigned long>(loraLink_.stats().sessionId),
             static_cast<unsigned>(loraLink_.stats().queueDepth),
             static_cast<unsigned long>(loraLink_.stats().sentFrames),
             static_cast<unsigned long>(loraLink_.stats().droppedFrames),
             static_cast<unsigned long>(loraLink_.stats().duplicateInboundFrames),
             static_cast<unsigned long>(loraLink_.stats().invalidInboundFrames),
             static_cast<unsigned long>(millis()));
  }

  void publishTelemetry(uint32_t now) {
    char payload[1400];
    buildTelemetryPayload(payload, sizeof(payload));

    if (mqttClient_.connected()) {
      char topic[96];
      snprintf(topic, sizeof(topic), "%s/state", Settings::MQTT.baseTopic);
      mqttClient_.publish(topic, payload, true);
    }

    char compactPayload[GreenhouseLoRa::kPayloadSize];
    GreenhouseLoRa::buildCompactTelemetry(compactPayload,
                                          sizeof(compactPayload),
                                          Settings::LORA.nodeId,
                                          modeLabel(),
                                          safeMode_,
                                          computeHealthScore(),
                                          snapshot_.airAvailable,
                                          snapshot_.airTempC,
                                          snapshot_.humidityPct,
                                          battery_.available,
                                          battery_.voltageV,
                                          battery_.percent);
    loraLink_.queueTelemetry(compactPayload, now);
  }

  void serviceLoRa(uint32_t now) {
    loraLink_.poll(now);
  }

  Preferences preferences_;
  Adafruit_BME280 bme_;
  DHT dht_{PinMap::TEMP_AIR_DHT, DHT22};
  BH1750 lightSensor_;
  OneWire oneWire_{PinMap::TEMP_WATER};
  DallasTemperature waterSensor_{&oneWire_};
  Adafruit_SSD1306 display_{Settings::DISPLAY_WIDTH, Settings::DISPLAY_HEIGHT, &Wire, -1};
  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  GreenhouseLoRa::DisabledTransport loraTransport_;
  GreenhouseLoRa::LinkService loraLink_{loraTransport_};
  Servo topServo_;
  Servo bottomServo_;
  Button modeButton_{PinMap::BUTTON_MODE};
  Button forceOpenButton_{PinMap::BUTTON_FORCE_OPEN};
  Button forceCloseButton_{PinMap::BUTTON_FORCE_CLOSE};

  SensorSnapshot snapshot_;
  SensorCacheState sensorCache_{};
  GreenhouseMetrics metrics_;
  BatteryState battery_;
  ActuatorState actuators_;
  ActuatorState effectiveActuators_;
  ControlMode mode_ = ControlMode::automatic;
  ControllerState controllerState_ = ControllerState::automatic;
  PowerBudget powerBudget_{};

  bool bmeReady_ = false;
  bool dhtReady_ = false;
  bool lightReady_ = false;
  bool waterReady_ = false;
  bool displayReady_ = false;
  bool filesystemReady_ = false;
  bool otaReady_ = false;
  bool mqttEnabled_ = false;
  bool discoveryPublished_ = false;
  bool safeMode_ = false;
  bool bootHealthy_ = false;
  bool rehomeAfterUnsafeReset_ = false;
  SafeModeReason safeModeReason_ = SafeModeReason::none;
  ConfigFault configFault_ = ConfigFault::none;
  esp_reset_reason_t lastResetReason_ = ESP_RST_UNKNOWN;
  uint32_t bootCount_ = 0;
  uint8_t bootFailures_ = 0;
  SensorFaultState sensorFaults_{};
  ServoProtectionState servoProtection_{};

  uint32_t lastControlAt_ = 0;
  uint32_t lastDisplayAt_ = 0;
  uint32_t lastLogAt_ = 0;
  uint32_t lastOtaAt_ = 0;
  uint32_t lastMqttConnectAttemptAt_ = 0;
  uint32_t lastTelemetryPublishAt_ = 0;
  uint32_t lastProgressAt_ = 0;
};

GreenhouseController controller;

}  // namespace

void setup() {
  controller.begin();
}

void loop() {
  controller.update();
}
