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
#include <time.h>
#include <WiFi.h>
#include <Wire.h>

#include "ControlLogic.h"
#include "PinMap.h"
#include "Settings.h"

namespace {

using GreenhouseLogic::ActuatorState;
using GreenhouseLogic::ControlMode;
using GreenhouseLogic::SensorSnapshot;

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

    modeButton_.begin();
    forceOpenButton_.begin();
    forceCloseButton_.begin();

    configureOutputs();
    Wire.begin(PinMap::I2C_SDA, PinMap::I2C_SCL);
    initFilesystem();
    initSensors();
    initDisplay();
    initServos();
    initPreferences();
    initWifi();

    digitalWrite(PinMap::STATUS_LED, LOW);
    writeCsvHeaderIfNeeded();
    snapshot_ = readSensors();
    computeOutputs();
    applyActuatorState();
    renderDisplay(true);
  }

  void update() {
    pollButtons();

    const uint32_t now = millis();
    if (now - lastControlAt_ >= Settings::LOGGING.controlIntervalMs) {
      lastControlAt_ = now;
      snapshot_ = readSensors();
      computeOutputs();
      applyActuatorState();
    }

    if (now - lastDisplayAt_ >= Settings::LOGGING.displayIntervalMs) {
      lastDisplayAt_ = now;
      renderDisplay(false);
    }

    if (now - lastLogAt_ >= Settings::LOGGING.sampleIntervalMs) {
      lastLogAt_ = now;
      appendLogRecord();
    }

    if (otaReady_) {
      ArduinoOTA.handle();
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
    LittleFS.begin(true);
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
      waterSensor_.setWaitForConversion(false);
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

  void pollButtons() {
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

  bool isDaylight(bool hasTime, uint32_t currentMinute) const {
    if (snapshot_.lightAvailable) {
      return snapshot_.lightLux >= Settings::CLIMATE.growLightLuxThreshold;
    }
    if (hasTime) {
      return currentMinute >= Settings::CLIMATE.growLightStartMinutes &&
             currentMinute < Settings::CLIMATE.growLightStopMinutes;
    }
    return true;
  }

  bool currentLocalTime(struct tm *timeInfo) const {
    return getLocalTime(timeInfo, 10);
  }

  void computeOutputs() {
    struct tm timeInfo {};
    const bool hasTime = currentLocalTime(&timeInfo);
    const uint32_t currentMinute = hasTime ? static_cast<uint32_t>(timeInfo.tm_hour * 60U + timeInfo.tm_min) : 0U;
    const bool daylight = isDaylight(hasTime, currentMinute);

    const GreenhouseLogic::ClimateConfig climateConfig{
        Settings::CLIMATE.ventOpenTempC,
        Settings::CLIMATE.ventCloseTempC,
        Settings::CLIMATE.fanOnTempC,
        Settings::CLIMATE.fanOffTempC,
        Settings::CLIMATE.humidityFanOnPct,
        Settings::CLIMATE.humidityFanOffPct,
        Settings::CLIMATE.heaterOnTempC,
        Settings::CLIMATE.heaterOffTempC,
        Settings::CLIMATE.waterHighTempC,
        Settings::CLIMATE.waterLowTempC,
        Settings::CLIMATE.growLightStartMinutes,
        Settings::CLIMATE.growLightStopMinutes,
        Settings::CLIMATE.growLightLuxThreshold,
    };

    const GreenhouseLogic::SystemConfig systemConfig{
        Settings::SYSTEM.enableDefogger,
        Settings::SYSTEM.enableGrowLight,
        Settings::SYSTEM.enableCirculationFans,
    };

    actuators_ = GreenhouseLogic::evaluateActuators({
        snapshot_,
        actuators_,
        mode_,
        climateConfig,
        systemConfig,
        daylight,
        hasTime,
        currentMinute,
    });
  }

  void applyActuatorState() {
    topServo_.write(actuators_.topVentOpen ? Settings::SERVOS.topOpenDegrees : Settings::SERVOS.topClosedDegrees);
    bottomServo_.write(actuators_.bottomVentOpen ? Settings::SERVOS.bottomOpenDegrees : Settings::SERVOS.bottomClosedDegrees);

    digitalWrite(PinMap::FAN_EXHAUST, actuators_.exhaustFanOn ? HIGH : LOW);
    digitalWrite(PinMap::FAN_INTAKE, actuators_.intakeFanOn ? HIGH : LOW);
    digitalWrite(PinMap::HEATER_DEFOGGER, Settings::SYSTEM.enableDefogger && actuators_.defoggerOn ? HIGH : LOW);
    digitalWrite(PinMap::GROW_LIGHT, Settings::SYSTEM.enableGrowLight && actuators_.growLightOn ? HIGH : LOW);
    digitalWrite(PinMap::CIRCULATION_FANS, Settings::SYSTEM.enableCirculationFans && actuators_.circulationFansOn ? HIGH : LOW);
  }

  void renderDisplay(bool fullRefresh) {
    if (!displayReady_) {
      return;
    }

    if (fullRefresh) {
      display_.clearDisplay();
    }

    display_.clearDisplay();
    display_.setCursor(0, 0);
    display_.printf("Mode: %s\n", modeLabel());
    display_.printf("Air:  %.1fC %.0f%%\n", snapshot_.airTempC, snapshot_.humidityPct);
    display_.printf("Water: %.1fC\n", snapshot_.waterTempC);
    display_.printf("Light: %.0f lx\n", snapshot_.lightLux);
    display_.printf("Vent: %s/%s\n", actuators_.topVentOpen ? "OPEN" : "SHUT", actuators_.bottomVentOpen ? "OPEN" : "SHUT");
    display_.printf("Fans: %s%s%s\n",
                    actuators_.intakeFanOn ? "I" : "-",
                    actuators_.exhaustFanOn ? "E" : "-",
                    actuators_.circulationFansOn ? "C" : "-");
    display_.printf("Heat:%c Light:%c %s",
                    actuators_.defoggerOn ? 'Y' : 'N',
                    actuators_.growLightOn ? 'Y' : 'N',
                    WiFi.status() == WL_CONNECTED ? "WiFi" : "OFF");
    display_.display();
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
    if (LittleFS.exists(Settings::LOGGING.path)) {
      return;
    }
    File file = LittleFS.open(Settings::LOGGING.path, FILE_WRITE);
    if (!file) {
      return;
    }
    file.println("millis,mode,air_temp_c,humidity_pct,water_temp_c,light_lux,top_open,bottom_open,fan_exhaust,fan_intake,defogger,grow_light,circulation");
    file.close();
  }

  void appendLogRecord() {
    File file = LittleFS.open(Settings::LOGGING.path, FILE_APPEND);
    if (!file) {
      return;
    }
    file.printf("%lu,%s,%.2f,%.2f,%.2f,%.2f,%u,%u,%u,%u,%u,%u,%u\n",
                millis(),
                modeLabel(),
                snapshot_.airTempC,
                snapshot_.humidityPct,
                snapshot_.waterTempC,
                snapshot_.lightLux,
                actuators_.topVentOpen,
                actuators_.bottomVentOpen,
                actuators_.exhaustFanOn,
                actuators_.intakeFanOn,
                actuators_.defoggerOn,
                actuators_.growLightOn,
                actuators_.circulationFansOn);
    file.close();
  }

  Preferences preferences_;
  Adafruit_BME280 bme_;
  DHT dht_{PinMap::TEMP_AIR_DHT, DHT22};
  BH1750 lightSensor_;
  OneWire oneWire_{PinMap::TEMP_WATER};
  DallasTemperature waterSensor_{&oneWire_};
  Adafruit_SSD1306 display_{Settings::DISPLAY_WIDTH, Settings::DISPLAY_HEIGHT, &Wire, -1};
  Servo topServo_;
  Servo bottomServo_;
  Button modeButton_{PinMap::BUTTON_MODE};
  Button forceOpenButton_{PinMap::BUTTON_FORCE_OPEN};
  Button forceCloseButton_{PinMap::BUTTON_FORCE_CLOSE};

  SensorSnapshot snapshot_;
  ActuatorState actuators_;
  ControlMode mode_ = ControlMode::automatic;

  bool bmeReady_ = false;
  bool dhtReady_ = false;
  bool lightReady_ = false;
  bool waterReady_ = false;
  bool displayReady_ = false;
  bool otaReady_ = false;

  uint32_t lastControlAt_ = 0;
  uint32_t lastDisplayAt_ = 0;
  uint32_t lastLogAt_ = 0;
};

GreenhouseController controller;

}  // namespace

void setup() {
  controller.begin();
}

void loop() {
  controller.update();
}