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
#include "PinMap.h"
#include "Settings.h"

namespace {

using GreenhouseLogic::ActuatorState;
using GreenhouseLogic::ControlMode;
using GreenhouseLogic::CropProfile;
using GreenhouseLogic::CropStatus;
using GreenhouseLogic::GreenhouseMetrics;
using GreenhouseLogic::SensorSnapshot;

struct BatteryState {
  float voltageV = 0.0F;
  int percent = 0;
  bool available = false;
  bool low = false;
  bool critical = false;
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
    initReliability();
    initSensors();
    initDisplay();
    initServos();
    initWifi();
    initMqtt();

    digitalWrite(PinMap::STATUS_LED, LOW);
    writeCsvHeaderIfNeeded();
    snapshot_ = readSensors();
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
    pollButtons();
    if (now - lastControlAt_ >= Settings::LOGGING.controlIntervalMs) {
      lastControlAt_ = now;
      snapshot_ = readSensors();
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

  void initServos() {
    topServo_.setPeriodHertz(50);
    bottomServo_.setPeriodHertz(50);
    topServo_.attach(PinMap::SERVO_TOP_VENT, 500, 2400);
    bottomServo_.attach(PinMap::SERVO_BOTTOM_VENT, 500, 2400);
    topServo_.write(Settings::SERVOS.topClosedDegrees);
    bottomServo_.write(Settings::SERVOS.bottomClosedDegrees);
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

    if (manualSafeModeRequested()) {
      safeMode_ = true;
      safeModeReason_ = "MANUAL";
    } else if (bootFailures_ >= Settings::RELIABILITY.safeModeAfterBootFailures) {
      safeMode_ = true;
      safeModeReason_ = "BOOT";
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

  void pollButtons() {
    if (safeMode_ && modeButton_.pressed()) {
      clearBootFailureState();
      ESP.restart();
      return;
    }
    if (modeButton_.pressed()) {
      cycleMode();
    }
    if (forceOpenButton_.pressed()) {
      setMode(ControlMode::forceOpen);
    }
    if (forceCloseButton_.pressed()) {
      setMode(ControlMode::forceClosed);
    }
  }

  void cycleMode() {
    setMode(GreenhouseLogic::nextMode(mode_));
  }

  void setMode(ControlMode mode) {
    mode_ = mode;
    preferences_.putUChar("mode", static_cast<uint8_t>(mode_));
    renderDisplay(true);
  }

  SensorSnapshot readSensors() {
    SensorSnapshot result;

    if (bmeReady_) {
      result.airTempC = bme_.readTemperature();
      result.humidityPct = bme_.readHumidity();
      result.airAvailable = !isnan(result.airTempC) && !isnan(result.humidityPct);
    }

    if (!result.airAvailable && dhtReady_) {
      result.airTempC = dht_.readTemperature();
      result.humidityPct = dht_.readHumidity();
      result.airAvailable = !isnan(result.airTempC) && !isnan(result.humidityPct);
    }

    if (lightReady_) {
      result.lightLux = lightSensor_.readLightLevel();
      result.lightAvailable = result.lightLux >= 0.0F;
    }

    if (waterReady_) {
      waterSensor_.requestTemperatures();
      result.waterTempC = waterSensor_.getTempCByIndex(0);
      result.waterAvailable = result.waterTempC > -100.0F && result.waterTempC < 125.0F;
    }

    return result;
  }

  bool currentLocalTime(struct tm *timeInfo) const {
    return getLocalTime(timeInfo, 10);
  }

  void computeOutputs() {
    if (safeMode_) {
      actuators_ = safeActuators();
      return;
    }

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
      topServo_.write(Settings::SERVOS.topClosedDegrees);
      bottomServo_.write(Settings::SERVOS.bottomClosedDegrees);
      digitalWrite(PinMap::FAN_EXHAUST, LOW);
      digitalWrite(PinMap::FAN_INTAKE, LOW);
      digitalWrite(PinMap::HEATER_DEFOGGER, LOW);
      digitalWrite(PinMap::GROW_LIGHT, LOW);
      digitalWrite(PinMap::CIRCULATION_FANS, LOW);
      return;
    }

    effectiveActuators_ = actuators_;
    effectiveActuators_.defoggerOn = Settings::SYSTEM.enableDefogger && effectiveActuators_.defoggerOn;
    effectiveActuators_.growLightOn = Settings::SYSTEM.enableGrowLight && effectiveActuators_.growLightOn;
    effectiveActuators_.circulationFansOn = Settings::SYSTEM.enableCirculationFans && effectiveActuators_.circulationFansOn;

    topServo_.write(actuators_.topVentOpen ? Settings::SERVOS.topOpenDegrees : Settings::SERVOS.topClosedDegrees);
    bottomServo_.write(actuators_.bottomVentOpen ? Settings::SERVOS.bottomOpenDegrees : Settings::SERVOS.bottomClosedDegrees);

    digitalWrite(PinMap::FAN_EXHAUST, actuators_.exhaustFanOn ? HIGH : LOW);
    digitalWrite(PinMap::FAN_INTAKE, actuators_.intakeFanOn ? HIGH : LOW);
    digitalWrite(PinMap::HEATER_DEFOGGER, effectiveActuators_.defoggerOn ? HIGH : LOW);
    digitalWrite(PinMap::GROW_LIGHT, effectiveActuators_.growLightOn ? HIGH : LOW);
    digitalWrite(PinMap::CIRCULATION_FANS, effectiveActuators_.circulationFansOn ? HIGH : LOW);
  }

  void renderDisplay(bool fullRefresh) {
    if (!displayReady_) {
      return;
    }

    char airBuffer[24];
    char waterBuffer[16];
    char lightBuffer[16];
    char statusBuffer[16];
    char vpdBuffer[16];
    char dewBuffer[16];
    char batteryBuffer[16];
    formatMeasurement(airBuffer,
                      sizeof(airBuffer),
                      snapshot_.airAvailable,
                      snapshot_.airTempC,
                      1,
                      "C");
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
      snprintf(batteryBuffer, sizeof(batteryBuffer), "%d%% %.2fV", battery_.percent, static_cast<double>(battery_.voltageV));
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
      display_.printf("Reason:%s\n", safeModeReason_);
      display_.printf("Reset:%s\n", resetReasonLabel(lastResetReason_));
      display_.printf("Boots:%u Fail:%u\n", static_cast<unsigned>(bootCount_), static_cast<unsigned>(bootFailures_));
      display_.printf("Batt:%s\n", batteryBuffer);
      display_.printf("Air: %s\n", airBuffer);
      display_.printf("MODE=resume");
    } else if (((millis() / Settings::LOGGING.displayIntervalMs) % 2U) == 0U) {
      display_.printf("Mode: %s\n", modeLabel());
      display_.printf("Air:  %s\n", airBuffer);
      display_.printf("Water: %s\n", waterBuffer);
      display_.printf("Light: %s\n", lightBuffer);
      display_.printf("Vent: %s/%s\n", effectiveActuators_.topVentOpen ? "OPEN" : "SHUT", effectiveActuators_.bottomVentOpen ? "OPEN" : "SHUT");
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
    if (safeMode_) {
      return "SAFE";
    }
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
    file.println("millis,mode,safe_mode,reset_reason,boot_failures,air_available,air_temp_c,humidity_pct,water_available,water_temp_c,light_available,light_lux,vpd_kpa,dew_point_c,frost_risk,crop_status,battery_available,battery_v,battery_pct,health_score,top_open,bottom_open,fan_exhaust,fan_intake,defogger,grow_light,circulation");
    file.close();
  }

  void appendBootEvent() {
    if (!filesystemReady_) {
      return;
    }
    File file = LittleFS.open("/boot_log.csv", FILE_APPEND);
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
                safeModeReason_);
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
    file.printf("%lu,%s,%u,%s,%u,%u,%.2f,%.2f,%u,%.2f,%u,%.2f,%.2f,%.2f,%u,%s,%u,%.2f,%d,%d,%u,%u,%u,%u,%u,%u,%u\n",
                millis(),
                modeLabel(),
          safeMode_,
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
                effectiveActuators_.circulationFansOn);
    file.close();
  }

  bool manualSafeModeRequested() const {
    return digitalRead(PinMap::BUTTON_FORCE_OPEN) == LOW && digitalRead(PinMap::BUTTON_FORCE_CLOSE) == LOW;
  }

  ActuatorState safeActuators() const {
    return ActuatorState{};
  }

  void clearBootFailureState() {
    preferences_.putBool("boot_pending", false);
    preferences_.putUChar("boot_fail", 0U);
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

    uint32_t totalMilliVolts = 0;
    for (uint8_t index = 0; index < Settings::BATTERY.samplesPerRead; ++index) {
      totalMilliVolts += static_cast<uint32_t>(analogReadMilliVolts(pin));
    }

    const float measuredV = (static_cast<float>(totalMilliVolts) /
                             static_cast<float>(Settings::BATTERY.samplesPerRead)) /
                            1000.0F;
    state.voltageV = (measuredV * Settings::BATTERY.dividerRatio) + Settings::BATTERY.calibrationOffsetVolts;
    state.available = state.voltageV > 0.1F;
    if (!state.available) {
      return state;
    }

    const float percent = ((state.voltageV - Settings::BATTERY.emptyVoltage) /
                           (Settings::BATTERY.fullVoltage - Settings::BATTERY.emptyVoltage)) * 100.0F;
    const float clamped = percent < 0.0F ? 0.0F : (percent > 100.0F ? 100.0F : percent);
    state.percent = static_cast<int>(clamped + 0.5F);
    state.low = state.voltageV <= Settings::BATTERY.lowVoltage;
    state.critical = state.voltageV <= Settings::BATTERY.criticalVoltage;
    return state;
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
    if (now - lastTelemetryPublishAt_ >= Settings::MQTT.publishIntervalMs) {
      lastTelemetryPublishAt_ = now;
      publishTelemetry();
      markProgress(now);
    }
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
    publishTelemetry();
  }

  void publishHomeAssistantDiscovery() {
    if (!Settings::MQTT.enableHomeAssistantDiscovery || discoveryPublished_) {
      return;
    }

    publishHomeAssistantSensorDiscovery("sensor", "air_temp", "Air Temperature", "{{ value_json.air.temp_c }}", "temperature", "measurement", "°C");
    publishHomeAssistantSensorDiscovery("sensor", "humidity", "Humidity", "{{ value_json.air.humidity_pct }}", "humidity", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "vpd", "VPD", "{{ value_json.metrics.vpd_kpa }}", "", "measurement", "kPa");
    publishHomeAssistantSensorDiscovery("sensor", "dew_point", "Dew Point", "{{ value_json.metrics.dew_point_c }}", "temperature", "measurement", "°C");
    publishHomeAssistantSensorDiscovery("sensor", "battery_pct", "Battery", "{{ value_json.battery.percent }}", "battery", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "health_score", "Health Score", "{{ value_json.health.score }}", "", "measurement", "%");
    publishHomeAssistantSensorDiscovery("sensor", "crop_status", "Crop Status", "{{ value_json.crop.status }}", "", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "frost_risk", "Frost Risk", "{{ value_json.metrics.frost_risk }}", "cold", "", "");
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

  void publishTelemetry() {
    if (!mqttClient_.connected()) {
      return;
    }

    char topic[96];
    char payload[1400];
    const CropProfile &profile = GreenhouseLogic::cropProfile(Settings::CROP.profile);
    snprintf(topic, sizeof(topic), "%s/state", Settings::MQTT.baseTopic);
    snprintf(payload,
             sizeof(payload),
             "{\"mode\":\"%s\",\"safe_mode\":{\"active\":%s,\"reason\":\"%s\",\"boot_failures\":%u},\"reset_reason\":\"%s\",\"crop\":{\"profile\":\"%s\",\"status\":\"%s\"},\"air\":{\"available\":%s,\"temp_c\":%.2f,\"humidity_pct\":%.2f},\"water\":{\"available\":%s,\"temp_c\":%.2f},\"light\":{\"available\":%s,\"lux\":%.2f},\"metrics\":{\"vpd_kpa\":%.2f,\"dew_point_c\":%.2f,\"frost_risk\":%s},\"battery\":{\"available\":%s,\"voltage_v\":%.2f,\"percent\":%d,\"low\":%s,\"critical\":%s},\"health\":{\"score\":%d},\"actuators\":{\"top_open\":%s,\"bottom_open\":%s,\"fan_exhaust\":%s,\"fan_intake\":%s,\"defogger\":%s,\"grow_light\":%s,\"circulation\":%s},\"connectivity\":{\"wifi\":%s,\"mqtt\":%s,\"ota\":%s},\"storage\":{\"filesystem_ready\":%s},\"uptime_ms\":%lu}",
             modeLabel(),
             safeMode_ ? "true" : "false",
             safeModeReason_,
             static_cast<unsigned>(bootFailures_),
             resetReasonLabel(lastResetReason_),
             profile.name,
             cropStatusLabel(metrics_.cropStatus),
             snapshot_.airAvailable ? "true" : "false",
             snapshot_.airTempC,
             snapshot_.humidityPct,
             snapshot_.waterAvailable ? "true" : "false",
             snapshot_.waterTempC,
             snapshot_.lightAvailable ? "true" : "false",
             snapshot_.lightLux,
             metrics_.vpdAvailable ? metrics_.vaporPressureDeficitKPa : 0.0F,
             metrics_.dewPointAvailable ? metrics_.dewPointC : 0.0F,
             metrics_.frostRisk ? "true" : "false",
             battery_.available ? "true" : "false",
             battery_.voltageV,
             battery_.percent,
             battery_.low ? "true" : "false",
             battery_.critical ? "true" : "false",
             computeHealthScore(),
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
             filesystemReady_ ? "true" : "false",
             static_cast<unsigned long>(millis()));
    mqttClient_.publish(topic, payload, true);
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
  Servo topServo_;
  Servo bottomServo_;
  Button modeButton_{PinMap::BUTTON_MODE};
  Button forceOpenButton_{PinMap::BUTTON_FORCE_OPEN};
  Button forceCloseButton_{PinMap::BUTTON_FORCE_CLOSE};

  SensorSnapshot snapshot_;
  GreenhouseMetrics metrics_;
  BatteryState battery_;
  ActuatorState actuators_;
  ActuatorState effectiveActuators_;
  ControlMode mode_ = ControlMode::automatic;

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
  const char *safeModeReason_ = "NONE";
  esp_reset_reason_t lastResetReason_ = ESP_RST_UNKNOWN;
  uint32_t bootCount_ = 0;
  uint8_t bootFailures_ = 0;

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