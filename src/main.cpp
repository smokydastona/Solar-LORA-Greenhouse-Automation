#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include <BH1750.h>
#include <DNSServer.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <esp32-hal-ledc.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>

#include "ControlLogic.h"
#include "ControllerRuntime.h"
#include "LoRaLink.h"
#include "LoRaSX1262Transport.h"
#include "PinMap.h"
#include "RemoteControl.h"
#include "Settings.h"
#include "Version.h"
#include "runtime/BatteryHal.h"
#include "runtime/DisplayHal.h"
#include "runtime/SensorHal.h"

namespace {

using GreenhouseLogic::ActuatorState;
using GreenhouseLogic::ClimateConfig;
using GreenhouseLogic::ControlMode;
using GreenhouseLogic::CropProfile;
using GreenhouseLogic::CropStatus;
using GreenhouseLogic::GreenhouseMetrics;
using GreenhouseLogic::SensorSnapshot;
using GreenhouseHal::BatteryState;
using GreenhouseHal::SensorCacheState;
using GreenhouseHal::SensorFaultState;
using GreenhouseHal::SensorHardwareState;
using GreenhouseRemote::CommandAudit;
using GreenhouseRemote::CommandStatus;
using GreenhouseRuntime::ConfigFault;
using GreenhouseRuntime::ControllerState;
using GreenhouseRuntime::DiagnosticCode;
using GreenhouseRuntime::PowerBudget;

constexpr const char *kCommandLogPath = "/command_log.csv";
constexpr size_t kRecentFaultCapacity = 4U;
constexpr size_t kWifiScanResultCapacity = 16U;
constexpr uint16_t kHttpPort = 80U;
constexpr uint16_t kDnsPort = 53U;
constexpr uint32_t kWifiReconnectIntervalMs = 30000UL;
constexpr uint32_t kWifiScanCooldownMs = 30000UL;
constexpr uint32_t kWifiScanRetryDelayMs = 5000UL;
constexpr const char *kSetupApIp = "192.168.4.1";
constexpr uint8_t kScreenSaverGrassCount = 18U;
constexpr uint8_t kScreenSaverParticleCount = 6U;
constexpr uint32_t kScreenSaverRegenerationMs = 5000UL;
static_assert(kRecentFaultCapacity == Settings::DIAGNOSTICS.recentFaultCapacity,
              "Recent fault capacity must match diagnostics settings.");

struct WifiRuntimeConfig {
  char ssid[33]{};
  char password[65]{};
  char hostname[33]{};
  bool hasCredentials = false;
  bool usingStoredCredentials = false;
};

struct WifiScanEntry {
  char ssid[33]{};
  int32_t rssi = 0;
  bool secure = false;
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

struct ServoProtectionState {
  int topTargetDegrees = 0;
  int bottomTargetDegrees = 0;
  bool motionActive = false;
  uint32_t motionEndsAt = 0;
  uint32_t cooldownEndsAt = 0;
  uint8_t protectionTrips = 0;
  uint32_t commandBlockedCount = 0;
  uint32_t lastMoveStartedAt = 0;
  uint32_t lastMoveCompletedAt = 0;
  uint32_t lastMoveDurationMs = 0;
  uint32_t longestMoveDurationMs = 0;
};

struct RecentFaultEntry {
  DiagnosticCode code = DiagnosticCode::none;
  uint32_t atMs = 0;
  int32_t detail = 0;
  uint32_t bootCount = 0;
};

enum class ScreenSaverMode : uint8_t {
  night = 0,
  growth,
};

struct ScreenSaverBlade {
  int16_t baseX = 0;
  uint8_t height = 0;
  int8_t phase = 0;
};

struct ScreenSaverParticle {
  int16_t x8 = 0;
  int16_t y8 = 0;
  int8_t vx8 = 0;
  uint8_t vy8 = 1;
  uint8_t twinkle = 0;
};

struct ScreenSaverPlant {
  int16_t baseX = Settings::DISPLAY_WIDTH / 2;
  uint16_t ageFrames = 0;
  int8_t leanBias = 0;
  uint8_t leafSpan = 4;
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

const char *batteryBandLabel(const BatteryState &battery) {
  if (!battery.available) {
    return "UNAVAILABLE";
  }
  if (!battery.calibrated) {
    return "UNCALIBRATED";
  }
  if (battery.critical) {
    return "CRITICAL";
  }
  if (battery.low) {
    return "LOW";
  }
  return "NORMAL";
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

class LedcServo {
 public:
  explicit LedcServo(uint8_t channel) : channel_(channel) {}

  int attach(int pin, int minPulseUs, int maxPulseUs) {
    if (pin < 0) {
      return 0;
    }
    minPulseUs_ = constrain(minPulseUs, kMinPulseUs, kMaxPulseUs);
    maxPulseUs_ = constrain(maxPulseUs, kMinPulseUs, kMaxPulseUs);
    pin_ = pin;
    ledcSetup(channel_, kFrequencyHz, kResolutionBits);
    ledcAttachPin(static_cast<uint8_t>(pin_), channel_);
    attached_ = true;
    return channel_;
  }

  void detach() {
    if (!attached_) {
      return;
    }
    ledcDetachPin(static_cast<uint8_t>(pin_));
    attached_ = false;
  }

  bool attached() const {
    return attached_;
  }

  void write(int value) {
    if (!attached_) {
      return;
    }

    int pulseUs = value;
    if (value < kMinPulseUs) {
      pulseUs = map(constrain(value, 0, 180), 0, 180, minPulseUs_, maxPulseUs_);
    } else {
      pulseUs = constrain(value, minPulseUs_, maxPulseUs_);
    }

    const uint32_t duty = static_cast<uint32_t>(
        (static_cast<uint64_t>(pulseUs) * kMaxDuty + (kPeriodUs / 2U)) / kPeriodUs);
    ledcWrite(channel_, duty);
  }

 private:
  static constexpr uint8_t kFrequencyHz = 50;
  static constexpr uint8_t kResolutionBits = 14;
  static constexpr uint32_t kPeriodUs = 20000U;
  static constexpr int kMinPulseUs = 500;
  static constexpr int kMaxPulseUs = 2400;
  static constexpr uint32_t kMaxDuty = (1UL << kResolutionBits) - 1UL;

  uint8_t channel_;
  int pin_ = -1;
  int minPulseUs_ = kMinPulseUs;
  int maxPulseUs_ = kMaxPulseUs;
  bool attached_ = false;
};

class GreenhouseController {
 public:
  void begin() {
    activeInstance_ = this;
    Serial.begin(115200);
    delay(200);
    Serial.printf("Mini Greenhouse firmware %s (%s)\n",
                  GreenhouseVersion::kFirmwareVersion,
                  GreenhouseVersion::kFirmwareCodename);
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
    showBootBanner();
    primeServoTargets();
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
    serviceWifi(now);
    updateDisplaySleep(now);

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

    const uint32_t displayInterval = displaySleeping_
                                         ? Settings::OLED.screenSaverFrameIntervalMs
                                         : Settings::LOGGING.displayIntervalMs;
    if (now - lastDisplayAt_ >= displayInterval) {
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

  static void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    if (activeInstance_ != nullptr) {
      activeInstance_->handleMqttMessage(topic, payload, length);
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
    sensorHardware_ = GreenhouseHal::initSensors(bme_, dht_, lightSensor_, waterSensor_);
  }

  void initDisplay() {
    displayReady_ = GreenhouseHal::initDisplay(display_);
    displaySleeping_ = false;
    lastDisplayActivityAt_ = millis();
    GreenhouseHal::setDisplayPower(display_, true);
    GreenhouseHal::setDisplayContrast(display_, Settings::OLED.activeContrast);
    randomSeed(static_cast<uint32_t>(esp_random()));
    initScreenSaver(millis());
  }

  void showBootBanner() {
    if (!displayReady_) {
      return;
    }

    GreenhouseHal::showBootBanner(display_,
                                  lastResetReason_,
                                  GreenhouseVersion::kFirmwareVersion,
                                  GreenhouseVersion::kFirmwareCodename,
                                  resetReasonLabel);
  }

  void primeServoTargets() {
    servoProtection_.topTargetDegrees = Settings::SERVOS.topClosedDegrees;
    servoProtection_.bottomTargetDegrees = Settings::SERVOS.bottomClosedDegrees;
  }

  void initPreferences() {
    preferences_.begin("greenhouse", false);
    mode_ = static_cast<ControlMode>(preferences_.getUChar("mode", static_cast<uint8_t>(ControlMode::automatic)));
    loadClimateConfig();

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

  void loadWifiConfig() {
    memset(&wifiConfig_, 0, sizeof(wifiConfig_));
    copySettingString(wifiConfig_.hostname, sizeof(wifiConfig_.hostname), Settings::WIFI.hostname);

    if (preferences_.isKey("wifi_ssid")) {
      preferences_.getString("wifi_ssid", wifiConfig_.ssid, sizeof(wifiConfig_.ssid));
      preferences_.getString("wifi_pass", wifiConfig_.password, sizeof(wifiConfig_.password));
      if (preferences_.isKey("wifi_host")) {
        preferences_.getString("wifi_host", wifiConfig_.hostname, sizeof(wifiConfig_.hostname));
      }
      wifiConfig_.usingStoredCredentials = strlen(wifiConfig_.ssid) > 0;
    } else {
      copySettingString(wifiConfig_.ssid, sizeof(wifiConfig_.ssid), Settings::WIFI.ssid);
      copySettingString(wifiConfig_.password, sizeof(wifiConfig_.password), Settings::WIFI.password);
    }

    if (strlen(wifiConfig_.hostname) == 0) {
      copySettingString(wifiConfig_.hostname, sizeof(wifiConfig_.hostname), "mini-greenhouse");
    }
    wifiConfig_.hasCredentials = strlen(wifiConfig_.ssid) > 0;
  }

  void loadClimateConfig() {
    runtimeClimate_ = Settings::CLIMATE;
    climateOverrideActive_ = preferences_.getBool("cl_override", false);
    if (!climateOverrideActive_) {
      return;
    }

    ClimateConfig candidate = runtimeClimate_;
    candidate.ventOpenTempC = preferences_.getFloat("cl_v_open", candidate.ventOpenTempC);
    candidate.ventCloseTempC = preferences_.getFloat("cl_v_close", candidate.ventCloseTempC);
    candidate.fanOnTempC = preferences_.getFloat("cl_fan_on", candidate.fanOnTempC);
    candidate.fanOffTempC = preferences_.getFloat("cl_fan_off", candidate.fanOffTempC);
    candidate.humidityFanOnPct = preferences_.getFloat("cl_hum_on", candidate.humidityFanOnPct);
    candidate.humidityFanOffPct = preferences_.getFloat("cl_hum_off", candidate.humidityFanOffPct);
    candidate.heaterOnTempC = preferences_.getFloat("cl_heat_on", candidate.heaterOnTempC);
    candidate.heaterOffTempC = preferences_.getFloat("cl_heat_off", candidate.heaterOffTempC);
    candidate.waterHighTempC = preferences_.getFloat("cl_water_hi", candidate.waterHighTempC);
    candidate.waterLowTempC = preferences_.getFloat("cl_water_lo", candidate.waterLowTempC);
    candidate.growLightLuxThreshold = preferences_.getFloat("cl_light_lux", candidate.growLightLuxThreshold);

    ConfigFault fault = ConfigFault::none;
    if (!validateClimateConfig(candidate, fault)) {
      clearClimateConfigOverride();
      Serial.printf("Invalid stored climate override ignored: %s\n", configFaultLabel(fault));
      return;
    }

    runtimeClimate_ = candidate;
  }

  void saveClimateConfig(const ClimateConfig &climate) {
    preferences_.putBool("cl_override", true);
    preferences_.putFloat("cl_v_open", climate.ventOpenTempC);
    preferences_.putFloat("cl_v_close", climate.ventCloseTempC);
    preferences_.putFloat("cl_fan_on", climate.fanOnTempC);
    preferences_.putFloat("cl_fan_off", climate.fanOffTempC);
    preferences_.putFloat("cl_hum_on", climate.humidityFanOnPct);
    preferences_.putFloat("cl_hum_off", climate.humidityFanOffPct);
    preferences_.putFloat("cl_heat_on", climate.heaterOnTempC);
    preferences_.putFloat("cl_heat_off", climate.heaterOffTempC);
    preferences_.putFloat("cl_water_hi", climate.waterHighTempC);
    preferences_.putFloat("cl_water_lo", climate.waterLowTempC);
    preferences_.putFloat("cl_light_lux", climate.growLightLuxThreshold);
    runtimeClimate_ = climate;
    climateOverrideActive_ = true;
  }

  void clearClimateConfigOverride() {
    preferences_.remove("cl_override");
    preferences_.remove("cl_v_open");
    preferences_.remove("cl_v_close");
    preferences_.remove("cl_fan_on");
    preferences_.remove("cl_fan_off");
    preferences_.remove("cl_hum_on");
    preferences_.remove("cl_hum_off");
    preferences_.remove("cl_heat_on");
    preferences_.remove("cl_heat_off");
    preferences_.remove("cl_water_hi");
    preferences_.remove("cl_water_lo");
    preferences_.remove("cl_light_lux");
    runtimeClimate_ = Settings::CLIMATE;
    climateOverrideActive_ = false;
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
    GreenhouseHal::initBatteryMonitor();
  }

  void initWifi() {
    loadWifiConfig();
    buildSetupAccessPointIdentity();

    if (wifiConfig_.hasCredentials) {
      connectToConfiguredWifi(true);
    }
    if (WiFi.status() != WL_CONNECTED && Settings::WIFI.enableSetupPortal) {
      startSetupPortal();
    }
    startWebServer();
  }

  void initMqtt() {
    mqttEnabled_ = strlen(Settings::MQTT.host) > 0;
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setBufferSize(1536);
    mqttClient_.setCallback(GreenhouseController::mqttCallback);
    if (mqttEnabled_) {
      mqttClient_.setServer(Settings::MQTT.host, Settings::MQTT.port);
    }
  }

  void serviceWifi(uint32_t now) {
    if (webServerStarted_) {
      webServer_.handleClient();
    }
    if (setupApActive_) {
      dnsServer_.processNextRequest();
    }
    serviceWifiScan(now);

    if (WiFi.status() == WL_CONNECTED) {
      if (!wifiConnected_) {
        onWifiConnected();
      }
      wifiReconnectFailures_ = 0;
      return;
    }

    wifiConnected_ = false;
    otaReady_ = false;
    if (!wifiConfig_.hasCredentials) {
      return;
    }
    if (now - lastWifiConnectAttemptAt_ < kWifiReconnectIntervalMs) {
      return;
    }

    ++wifiReconnectFailures_;
    lastWifiConnectAttemptAt_ = now;
    WiFi.disconnect(false, false);
    WiFi.begin(wifiConfig_.ssid, wifiConfig_.password);
    if (wifiReconnectFailures_ >= 3U && Settings::WIFI.enableSetupPortal && !setupApActive_) {
      startSetupPortal();
    }
  }

  void initLoRa() {
    loraLink_.begin(Settings::LORA.enableTransport,
                    Settings::LORA.nodeId,
                    Settings::LORA.maxRetries,
                    Settings::LORA.retryBackoffMs,
                    bootCount_ ^ static_cast<uint32_t>(lastResetReason_),
                    Settings::DIAGNOSTICS.loraQueueWarningDepth);
  }

  void handleMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
    if (!Settings::MQTT.enableInboundModeCommands) {
      return;
    }

    char expectedTopic[GreenhouseRemote::kTopicSize];
    GreenhouseRemote::buildModeCommandTopic(expectedTopic, sizeof(expectedTopic), Settings::MQTT.baseTopic);

    char payloadBuffer[32]{};
    const size_t copyLength = length < (sizeof(payloadBuffer) - 1U) ? length : (sizeof(payloadBuffer) - 1U);
    memcpy(payloadBuffer, payload, copyLength);
    payloadBuffer[copyLength] = '\0';

    lastCommandAudit_ = GreenhouseRemote::evaluateModeCommand(safeMode_, topic, expectedTopic, payloadBuffer, millis());
    if (lastCommandAudit_.status == CommandStatus::accepted) {
      setMode(lastCommandAudit_.applied);
    }

    appendCommandAudit(topic, payloadBuffer, lastCommandAudit_);
    publishModeCommandState();
    publishModeCommandResult();
  }

  void pollButtons() {
    const bool modePressed = modeButton_.pressed();
    const bool forceOpenPressed = forceOpenButton_.pressed();
    const bool forceClosePressed = forceCloseButton_.pressed();

    if (modePressed || forceOpenPressed || forceClosePressed) {
      noteDisplayActivity(millis());
    }

    if (safeMode_ && modePressed) {
      clearBootFailureState();
      ESP.restart();
      return;
    }
    if (modePressed) {
      setMode(ControlMode::automatic);
    }
    if (forceOpenPressed) {
      setMode(ControlMode::forceOpen);
    }
    if (forceClosePressed) {
      setMode(ControlMode::forceClosed);
    }
  }

  bool connectToConfiguredWifi(bool waitForResult) {
    if (!wifiConfig_.hasCredentials) {
      return false;
    }

    WiFi.mode(setupApActive_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.setHostname(wifiConfig_.hostname);
    WiFi.begin(wifiConfig_.ssid, wifiConfig_.password);
    lastWifiConnectAttemptAt_ = millis();

    if (!waitForResult) {
      return false;
    }

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < Settings::WIFI.connectTimeoutMs) {
      feedHardwareWatchdog();
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      onWifiConnected();
      return true;
    }

    Serial.printf("Wi-Fi join failed for '%s'.\n", wifiConfig_.ssid);
    return false;
  }

  void onWifiConnected() {
    wifiConnected_ = true;
    if (setupApActive_) {
      stopSetupPortal();
    }
    configTzTime(Settings::WIFI.tzInfo, "pool.ntp.org", "time.nist.gov");
    if (!mdnsActive_) {
      mdnsActive_ = MDNS.begin(wifiConfig_.hostname);
      if (mdnsActive_) {
        MDNS.addService("http", "tcp", kHttpPort);
      }
    }
    if (Settings::WIFI.enableOta) {
      ArduinoOTA.setHostname(wifiConfig_.hostname);
      ArduinoOTA.begin();
      otaReady_ = true;
    }
    Serial.printf("Wi-Fi connected to '%s'. Dashboard: http://%s.local/\n",
                  wifiConfig_.ssid,
                  wifiConfig_.hostname);
    Serial.printf("Dashboard IP fallback: http://%s/\n", WiFi.localIP().toString().c_str());
  }

  void startSetupPortal() {
    if (setupApActive_ || !Settings::WIFI.enableSetupPortal) {
      return;
    }

    if (mdnsActive_) {
      MDNS.end();
      mdnsActive_ = false;
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(setupApSsid_, setupApPassword_);
    dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());
    setupApActive_ = true;
    queueWifiScan(true);
    Serial.printf("Wi-Fi setup AP active. SSID: %s Password: %s Portal: http://%s/\n",
                  setupApSsid_,
                  setupApPassword_,
                  kSetupApIp);
  }

  void stopSetupPortal() {
    dnsServer_.stop();
    if (wifiScanInProgress_) {
      WiFi.scanDelete();
    }
    wifiScanInProgress_ = false;
    wifiScanRequested_ = false;
    WiFi.softAPdisconnect(true);
    setupApActive_ = false;
  }

  void startWebServer() {
    if (webServerStarted_) {
      return;
    }

    webServer_.on("/", HTTP_GET, [this]() { handleDashboardRequest(); });
    webServer_.on("/api/state", HTTP_GET, [this]() { handleStateApiRequest(); });
    webServer_.on("/api/control", HTTP_GET, [this]() { handleControlApiRequest(); });
    webServer_.on("/api/control/mode", HTTP_POST, [this]() { handleModeControlRequest(); });
    webServer_.on("/api/control/manual", HTTP_POST, [this]() { handleManualOverrideRequest(); });
    webServer_.on("/api/control/actuator", HTTP_POST, [this]() { handleActuatorControlRequest(); });
    webServer_.on("/api/settings/climate", HTTP_GET, [this]() { handleClimateSettingsRequest(); });
    webServer_.on("/api/settings/climate", HTTP_POST, [this]() { handleClimateSettingsSaveRequest(); });
    webServer_.on("/api/settings/climate/reset", HTTP_POST, [this]() { handleClimateSettingsResetRequest(); });
    webServer_.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScanRequest(); });
    webServer_.on("/wifi", HTTP_POST, [this]() { handleWifiSaveRequest(); });
    webServer_.on("/wifi/reset", HTTP_POST, [this]() { handleWifiResetRequest(); });
    webServer_.on(
        "/update",
        HTTP_POST,
        [this]() { handleFirmwareUpdateRequest(); },
        [this]() { handleFirmwareUpload(); });
    webServer_.on("/generate_204", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/hotspot-detect.html", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/connecttest.txt", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/ncsi.txt", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/redirect", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/success.txt", HTTP_GET, [this]() { handlePortalProbeRequest(); });
    webServer_.on("/favicon.ico", HTTP_GET, [this]() { handleFaviconRequest(); });
    webServer_.onNotFound([this]() { handleNotFoundRequest(); });
    webServer_.begin();
    webServerStarted_ = true;
  }

  void handleDashboardRequest() {
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(200, "text/html; charset=utf-8", buildDashboardHtml());
  }

  void handleStateApiRequest() {
    char payload[2200];
    buildTelemetryPayload(payload, sizeof(payload));
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(200, "application/json; charset=utf-8", payload);
  }

  void handleControlApiRequest() {
    String payload;
    buildControlApiPayload(payload, true, "");
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(200, "application/json; charset=utf-8", payload);
  }

  void handleClimateSettingsRequest() {
    String payload;
    buildClimateSettingsPayload(payload, true, "");
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(200, "application/json; charset=utf-8", payload);
  }

  void handleClimateSettingsSaveRequest() {
    String authError;
    if (!authorizeClimateSettingsEdit(authError)) {
      sendClimateSettingsResponse(403, false, authError.c_str());
      return;
    }

    ClimateConfig candidate = runtimeClimate_;
    if (!parseClimateSettingsFromRequest(candidate, authError)) {
      sendClimateSettingsResponse(400, false, authError.c_str());
      return;
    }

    ConfigFault fault = ConfigFault::none;
    if (!validateClimateConfig(candidate, fault)) {
      authError = String("Rejected climate settings: ") + configFaultLabel(fault) + '.';
      sendClimateSettingsResponse(400, false, authError.c_str());
      return;
    }

    saveClimateConfig(candidate);
    applyConfigurationSafety();
    refreshControlOutputs();
    sendClimateSettingsResponse(200, true, "Climate thresholds saved.");
  }

  void handleClimateSettingsResetRequest() {
    String authError;
    if (!authorizeClimateSettingsEdit(authError)) {
      sendClimateSettingsResponse(403, false, authError.c_str());
      return;
    }

    clearClimateConfigOverride();
    applyConfigurationSafety();
    refreshControlOutputs();
    sendClimateSettingsResponse(200, true, "Climate thresholds restored to firmware defaults.");
  }

  void handleModeControlRequest() {
    if (safeMode_) {
      sendControlApiResponse(409, false, "Safe mode is active. Live controls are locked.");
      return;
    }

    String mode = webServer_.arg("mode");
    mode.trim();
    mode.toUpperCase();

    if (mode == F("MANUAL")) {
      enableManualOverrideFromCurrentState();
      refreshControlOutputs();
      sendControlApiResponse(200, true, "Manual output override enabled.");
      return;
    }

    ControlMode nextMode = ControlMode::automatic;
    if (mode == F("OPEN")) {
      nextMode = ControlMode::forceOpen;
    } else if (mode == F("CLOSED")) {
      nextMode = ControlMode::forceClosed;
    } else if (mode != F("AUTO")) {
      sendControlApiResponse(400, false, "Unsupported mode. Use AUTO, OPEN, CLOSED, or MANUAL.");
      return;
    }

    setMode(nextMode);
    refreshControlOutputs();
    sendControlApiResponse(200, true, "Controller mode updated.");
  }

  void handleManualOverrideRequest() {
    if (safeMode_) {
      sendControlApiResponse(409, false, "Safe mode is active. Manual override is unavailable.");
      return;
    }

    bool enabled = false;
    if (!parseToggleValue(webServer_.arg("enabled"), enabled)) {
      sendControlApiResponse(400, false, "Invalid manual override value. Use true or false.");
      return;
    }

    if (enabled) {
      enableManualOverrideFromCurrentState();
    } else {
      manualOverrideActive_ = false;
    }
    refreshControlOutputs();
    sendControlApiResponse(200, true, enabled ? "Manual override enabled." : "Manual override released.");
  }

  void handleActuatorControlRequest() {
    if (safeMode_) {
      sendControlApiResponse(409, false, "Safe mode is active. Actuator commands are locked.");
      return;
    }

    bool requestedState = false;
    if (!parseToggleValue(webServer_.arg("state"), requestedState)) {
      sendControlApiResponse(400, false, "Invalid actuator state. Use on/off, open/closed, true/false, or 1/0.");
      return;
    }

    const String actuator = webServer_.arg("actuator");
    if (!applyManualActuatorCommand(actuator, requestedState)) {
      sendControlApiResponse(400, false, "Unknown actuator.");
      return;
    }

    refreshControlOutputs();
    sendControlApiResponse(200, true, "Actuator command queued.");
  }

  bool parseToggleValue(String value, bool &enabled) const {
    value.trim();
    value.toUpperCase();
    if (value == F("1") || value == F("TRUE") || value == F("ON") || value == F("OPEN") || value == F("ENABLE") || value == F("ENABLED")) {
      enabled = true;
      return true;
    }
    if (value == F("0") || value == F("FALSE") || value == F("OFF") || value == F("CLOSED") || value == F("CLOSE") || value == F("DISABLE") || value == F("DISABLED")) {
      enabled = false;
      return true;
    }
    return false;
  }

  void enableManualOverrideFromCurrentState() {
    setMode(ControlMode::automatic);
    manualActuators_ = effectiveActuators_;
    manualOverrideActive_ = true;
  }

  bool applyManualActuatorCommand(String actuator, bool enabled) {
    actuator.trim();
    actuator.toLowerCase();
    if (!manualOverrideActive_) {
      enableManualOverrideFromCurrentState();
    }

    if (actuator == F("top_open") || actuator == F("top") || actuator == F("top_vent")) {
      manualActuators_.topVentOpen = enabled;
      return true;
    }
    if (actuator == F("bottom_open") || actuator == F("bottom") || actuator == F("bottom_vent")) {
      manualActuators_.bottomVentOpen = enabled;
      return true;
    }
    if (actuator == F("fan_exhaust") || actuator == F("exhaust") || actuator == F("exhaust_fan")) {
      manualActuators_.exhaustFanOn = enabled;
      return true;
    }
    if (actuator == F("fan_intake") || actuator == F("intake") || actuator == F("intake_fan")) {
      manualActuators_.intakeFanOn = enabled;
      return true;
    }
    if (actuator == F("defogger") || actuator == F("heater")) {
      manualActuators_.defoggerOn = enabled;
      return true;
    }
    if (actuator == F("grow_light") || actuator == F("light")) {
      manualActuators_.growLightOn = enabled;
      return true;
    }
    if (actuator == F("circulation") || actuator == F("circulation_fans") || actuator == F("circulation_fan")) {
      manualActuators_.circulationFansOn = enabled;
      return true;
    }
    return false;
  }

  void refreshControlOutputs() {
    computeOutputs();
    applyActuatorState();
    renderDisplay(true);
  }

  bool authorizeClimateSettingsEdit(String &message) {
    if (WiFi.status() != WL_CONNECTED) {
      message = "Climate settings can only be edited from the station dashboard after the node joins Wi-Fi.";
      return false;
    }

    const String password = webServer_.arg("password");
    if (password.length() == 0) {
      message = "Enter the station Wi-Fi password to save climate settings.";
      return false;
    }

    const bool matchesStationPassword = strlen(wifiConfig_.password) > 0 && password == wifiConfig_.password;
    const bool matchesSetupPassword = strlen(setupApPassword_) > 0 && password == setupApPassword_;
    if (!matchesStationPassword && !matchesSetupPassword) {
      message = "Authentication failed. Use the station Wi-Fi password or the node setup password.";
      return false;
    }

    return true;
  }

  bool parseClimateSettingsFromRequest(ClimateConfig &climate, String &message) {
    return parseFloatArg("vent_open_temp_c", climate.ventOpenTempC, message) &&
           parseFloatArg("vent_close_temp_c", climate.ventCloseTempC, message) &&
           parseFloatArg("fan_on_temp_c", climate.fanOnTempC, message) &&
           parseFloatArg("fan_off_temp_c", climate.fanOffTempC, message) &&
           parseFloatArg("humidity_fan_on_pct", climate.humidityFanOnPct, message) &&
           parseFloatArg("humidity_fan_off_pct", climate.humidityFanOffPct, message) &&
           parseFloatArg("heater_on_temp_c", climate.heaterOnTempC, message) &&
           parseFloatArg("heater_off_temp_c", climate.heaterOffTempC, message) &&
           parseFloatArg("water_high_temp_c", climate.waterHighTempC, message) &&
           parseFloatArg("water_low_temp_c", climate.waterLowTempC, message) &&
           parseFloatArg("grow_light_lux_threshold", climate.growLightLuxThreshold, message);
  }

  bool parseFloatArg(const char *name, float &value, String &message) {
    const String rawValue = webServer_.arg(name);
    String trimmed = rawValue;
    trimmed.trim();
    if (trimmed.length() == 0) {
      message = String("Missing setting: ") + name + '.';
      return false;
    }

    char buffer[32]{};
    trimmed.toCharArray(buffer, sizeof(buffer));
    char *end = nullptr;
    const float parsed = strtof(buffer, &end);
    if (end == buffer || *end != '\0' || isnan(parsed) || isinf(parsed)) {
      message = String("Invalid numeric value for ") + name + '.';
      return false;
    }

    value = parsed;
    return true;
  }

  bool validateClimateConfig(const ClimateConfig &climate, ConfigFault &fault) const {
    GreenhouseRuntime::ValidationInput input{};
    populateValidationInput(input, climate);
    fault = GreenhouseRuntime::validateConfiguration(input);
    return fault == ConfigFault::none;
  }

  void populateValidationInput(GreenhouseRuntime::ValidationInput &input, const ClimateConfig &climate) const {
    input.climate = climate;
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
  }

  void buildClimateSettingsPayload(String &payload, bool ok, const char *message) const {
    payload.remove(0);
    payload.reserve(640);
    payload += F("{\"ok\":");
    payload += ok ? F("true") : F("false");
    payload += F(",\"message\":\"");
    payload += jsonEscape(message == nullptr ? "" : message);
    payload += F("\",\"override_active\":");
    payload += climateOverrideActive_ ? F("true") : F("false");
    payload += F(",\"settings_auth\":\"station_or_setup_password\",\"climate\":{");
    payload += F("\"vent_open_temp_c\":");
    payload += String(runtimeClimate_.ventOpenTempC, 2);
    payload += F(",\"vent_close_temp_c\":");
    payload += String(runtimeClimate_.ventCloseTempC, 2);
    payload += F(",\"fan_on_temp_c\":");
    payload += String(runtimeClimate_.fanOnTempC, 2);
    payload += F(",\"fan_off_temp_c\":");
    payload += String(runtimeClimate_.fanOffTempC, 2);
    payload += F(",\"humidity_fan_on_pct\":");
    payload += String(runtimeClimate_.humidityFanOnPct, 2);
    payload += F(",\"humidity_fan_off_pct\":");
    payload += String(runtimeClimate_.humidityFanOffPct, 2);
    payload += F(",\"heater_on_temp_c\":");
    payload += String(runtimeClimate_.heaterOnTempC, 2);
    payload += F(",\"heater_off_temp_c\":");
    payload += String(runtimeClimate_.heaterOffTempC, 2);
    payload += F(",\"water_high_temp_c\":");
    payload += String(runtimeClimate_.waterHighTempC, 2);
    payload += F(",\"water_low_temp_c\":");
    payload += String(runtimeClimate_.waterLowTempC, 2);
    payload += F(",\"grow_light_start_minutes\":");
    payload += String(runtimeClimate_.growLightStartMinutes);
    payload += F(",\"grow_light_stop_minutes\":");
    payload += String(runtimeClimate_.growLightStopMinutes);
    payload += F(",\"grow_light_lux_threshold\":");
    payload += String(runtimeClimate_.growLightLuxThreshold, 2);
    payload += F("}}");
  }

  void sendClimateSettingsResponse(int statusCode, bool ok, const char *message) {
    String payload;
    buildClimateSettingsPayload(payload, ok, message);
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(statusCode, "application/json; charset=utf-8", payload);
  }

  void buildControlApiPayload(String &payload, bool ok, const char *message) const {
    payload.remove(0);
    payload.reserve(640);
    payload += F("{\"ok\":");
    payload += ok ? F("true") : F("false");
    payload += F(",\"message\":\"");
    payload += jsonEscape(message == nullptr ? "" : message);
    payload += F("\",\"manual_override\":{\"active\":");
    payload += manualOverrideActive_ ? F("true") : F("false");
    payload += F("},\"mode\":\"");
    payload += modeLabel();
    payload += F("\",\"wifi\":{\"connected\":");
    payload += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
    payload += F(",\"ssid\":\"");
    payload += jsonEscape(wifiConfig_.ssid);
    payload += F("\",\"hostname\":\"");
    payload += jsonEscape(wifiConfig_.hostname);
    payload += F("\"},\"actuators\":{\"top_open\":");
    payload += effectiveActuators_.topVentOpen ? F("true") : F("false");
    payload += F(",\"bottom_open\":");
    payload += effectiveActuators_.bottomVentOpen ? F("true") : F("false");
    payload += F(",\"fan_exhaust\":");
    payload += effectiveActuators_.exhaustFanOn ? F("true") : F("false");
    payload += F(",\"fan_intake\":");
    payload += effectiveActuators_.intakeFanOn ? F("true") : F("false");
    payload += F(",\"defogger\":");
    payload += effectiveActuators_.defoggerOn ? F("true") : F("false");
    payload += F(",\"grow_light\":");
    payload += effectiveActuators_.growLightOn ? F("true") : F("false");
    payload += F(",\"circulation\":");
    payload += effectiveActuators_.circulationFansOn ? F("true") : F("false");
    payload += F("}}}");
  }

  void sendControlApiResponse(int statusCode, bool ok, const char *message) {
    String payload;
    buildControlApiPayload(payload, ok, message);
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.send(statusCode, "application/json; charset=utf-8", payload);
  }

  void handleWifiScanRequest() {
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");

    if (webServer_.hasArg("refresh")) {
      queueWifiScan(true);
    } else if ((wifiScanResultsJson_ == F("[]") && !wifiScanInProgress_) ||
               (millis() - lastWifiScanCompletedAt_ >= kWifiScanCooldownMs && !wifiScanInProgress_)) {
      queueWifiScan(false);
    }

    String payload;
    payload.reserve(1600);
    payload += F("{\"networks\":");
    payload += wifiScanResultsJson_;
    payload += F(",\"connected\":\"");
    payload += jsonEscape(wifiConfig_.ssid);
    payload += F("\",\"refreshing\":");
    payload += wifiScanInProgress_ ? F("true") : F("false");
    payload += F(",\"lastScanAgeMs\":");
    payload += String(lastWifiScanCompletedAt_ == 0U ? 0U : millis() - lastWifiScanCompletedAt_);
    if (wifiScanError_[0] != '\0') {
      payload += F(",\"error\":\"");
      payload += jsonEscape(wifiScanError_);
      payload += '"';
    }
    payload += '}';
    webServer_.send(200, "application/json; charset=utf-8", payload);
  }

  void queueWifiScan(bool force) {
    if (!setupApActive_) {
      return;
    }
    if (force) {
      lastWifiScanCompletedAt_ = 0;
    }
    wifiScanRequested_ = true;
  }

  void serviceWifiScan(uint32_t now) {
    if (!setupApActive_) {
      return;
    }

    if (wifiScanInProgress_) {
      const int scanStatus = WiFi.scanComplete();
      if (scanStatus == WIFI_SCAN_RUNNING) {
        return;
      }

      wifiScanInProgress_ = false;
      lastWifiScanCompletedAt_ = now;
      if (scanStatus < 0) {
        copySettingString(wifiScanError_, sizeof(wifiScanError_), "Wi-Fi scan failed. Try again.");
        WiFi.scanDelete();
        return;
      }

      wifiScanResultsJson_.remove(0);
      wifiScanResultCount_ = 0U;
      wifiScanResultsJson_ += '[';

      size_t emitted = 0U;
      for (int index = 0; index < scanStatus && emitted < kWifiScanResultCapacity; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.length() == 0) {
          continue;
        }

        bool duplicate = false;
        for (int previous = 0; previous < index; ++previous) {
          if (ssid == WiFi.SSID(previous)) {
            duplicate = true;
            break;
          }
        }
        if (duplicate) {
          continue;
        }

        if (emitted > 0U) {
          wifiScanResultsJson_ += ',';
        }
        wifiScanResultsJson_ += F("{\"ssid\":\"");
        wifiScanResultsJson_ += jsonEscape(ssid);
        wifiScanResultsJson_ += F("\",\"rssi\":");
        wifiScanResultsJson_ += String(WiFi.RSSI(index));
        wifiScanResultsJson_ += F(",\"secure\":");
        wifiScanResultsJson_ += (WiFi.encryptionType(index) == WIFI_AUTH_OPEN) ? F("false") : F("true");
        wifiScanResultsJson_ += '}';
        copySettingString(wifiScanEntries_[emitted].ssid, sizeof(wifiScanEntries_[emitted].ssid), ssid.c_str());
        wifiScanEntries_[emitted].rssi = WiFi.RSSI(index);
        wifiScanEntries_[emitted].secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        ++emitted;
      }

      wifiScanResultCount_ = emitted;
      wifiScanResultsJson_ += ']';
      wifiScanError_[0] = '\0';
      WiFi.scanDelete();
      return;
    }

    const bool scanCacheExpired = lastWifiScanCompletedAt_ == 0U ||
                                  (now - lastWifiScanCompletedAt_ >= kWifiScanCooldownMs);
    if (!wifiScanRequested_ && !scanCacheExpired) {
      return;
    }
    if (now - lastWifiScanStartedAt_ < kWifiScanRetryDelayMs) {
      return;
    }

    const int scanStatus = WiFi.scanNetworks(true, true);
    lastWifiScanStartedAt_ = now;
    if (scanStatus == WIFI_SCAN_FAILED) {
      copySettingString(wifiScanError_, sizeof(wifiScanError_), "Wi-Fi scan failed. Try again.");
      lastWifiScanCompletedAt_ = now;
      wifiScanRequested_ = false;
      return;
    }

    wifiScanInProgress_ = true;
    wifiScanRequested_ = false;
  }

  void handleWifiSaveRequest() {
    const String ssid = webServer_.arg("ssid");
    const String password = webServer_.arg("password");
    const String hostname = webServer_.arg("hostname");
    if (ssid.length() == 0) {
      webServer_.send(400, "text/plain; charset=utf-8", "SSID is required.");
      return;
    }

    const bool sameStoredNetwork = wifiConfig_.usingStoredCredentials && ssid == wifiConfig_.ssid;
    String resolvedPassword = password;
    bool preservedPassword = false;

    if (resolvedPassword.length() == 0 && sameStoredNetwork && strlen(wifiConfig_.password) > 0) {
      resolvedPassword = wifiConfig_.password;
      preservedPassword = true;
    }

    if (resolvedPassword.length() == 0 && cachedNetworkIsSecure(ssid)) {
      webServer_.send(
          400,
          "text/html; charset=utf-8",
          String("<html><body><h1>Password required</h1><p>The nearby network <strong>") +
              htmlEscape(ssid.c_str()) +
              "</strong> is secured. Enter the Wi-Fi password before saving.</p><p><a href='/'>Return to the setup portal</a></p></body></html>");
      return;
    }

    saveWifiConfig(ssid.c_str(), resolvedPassword.c_str(), hostname.c_str());
    webServer_.send(200,
                    "text/html; charset=utf-8",
                    String("<html><body><h1>Wi-Fi saved</h1><p>Saved SSID: <strong>") +
                        htmlEscape(ssid.c_str()) +
                        "</strong></p><p>Password stored: <strong>" +
                        String(resolvedPassword.length() > 0 ? "yes" : "no") +
                        "</strong>" +
                        (preservedPassword ? " (kept the existing saved password)." : ".") +
                        "</p><p>The node is restarting and will try the saved network. If it cannot join, reconnect to the setup AP and open http://192.168.4.1/ again.</p></body></html>");
    delay(300);
    ESP.restart();
  }

  void handleWifiResetRequest() {
    preferences_.remove("wifi_ssid");
    preferences_.remove("wifi_pass");
    preferences_.remove("wifi_host");
    webServer_.send(200,
                    "text/html; charset=utf-8",
                    "<html><body><h1>Wi-Fi cleared</h1><p>The node is restarting into setup mode. Reconnect to the setup AP and open http://192.168.4.1/.</p></body></html>");
    delay(300);
    ESP.restart();
  }

  void handleFirmwareUpdateRequest() {
    if (firmwareUploadSucceeded_) {
      webServer_.send(200,
                      "text/html; charset=utf-8",
                      "<html><body><h1>Firmware uploaded</h1><p>The node is restarting into the new firmware. Reconnect to the dashboard in about 20 seconds.</p></body></html>");
      delay(500);
      ESP.restart();
      return;
    }

    webServer_.send(500,
                    "text/html; charset=utf-8",
                    String("<html><body><h1>Firmware update failed</h1><p>") +
                        htmlEscape(firmwareUploadStatus_) +
                        "</p><p><a href='/'>Return to the dashboard</a></p></body></html>");
  }

  void handleFirmwareUpload() {
    HTTPUpload &upload = webServer_.upload();

    if (upload.status == UPLOAD_FILE_START) {
      firmwareUploadSucceeded_ = false;
      copySettingString(firmwareUploadStatus_, sizeof(firmwareUploadStatus_), "Firmware upload did not complete.");
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        snprintf(firmwareUploadStatus_,
                 sizeof(firmwareUploadStatus_),
                 "Firmware upload could not start (error %u).",
                 static_cast<unsigned int>(Update.getError()));
      }
      return;
    }

    if (!Update.isRunning()) {
      return;
    }

    if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        snprintf(firmwareUploadStatus_,
                 sizeof(firmwareUploadStatus_),
                 "Firmware write failed (error %u).",
                 static_cast<unsigned int>(Update.getError()));
        Update.abort();
      }
      return;
    }

    if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        firmwareUploadSucceeded_ = true;
        copySettingString(firmwareUploadStatus_,
                          sizeof(firmwareUploadStatus_),
                          "Firmware update is ready. Restarting into the new image.");
      } else {
        snprintf(firmwareUploadStatus_,
                 sizeof(firmwareUploadStatus_),
                 "Firmware finalize failed (error %u).",
                 static_cast<unsigned int>(Update.getError()));
      }
      return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      copySettingString(firmwareUploadStatus_,
                        sizeof(firmwareUploadStatus_),
                        "Firmware upload was canceled before completion.");
    }
  }

  void handlePortalProbeRequest() {
    if (setupApActive_) {
      webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
      webServer_.send(204, "text/plain; charset=utf-8", "");
      return;
    }
    handleDashboardRequest();
  }

  void handleFaviconRequest() {
    webServer_.send(204, "image/x-icon", "");
  }

  void handleNotFoundRequest() {
    if (setupApActive_) {
      sendPortalRedirect();
      return;
    }
    webServer_.send(404, "text/plain; charset=utf-8", "Not found.");
  }

  void sendPortalRedirect() {
    webServer_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    webServer_.sendHeader("Location", String("http://") + kSetupApIp + "/", true);
    webServer_.send(302,
                    "text/html; charset=utf-8",
                    "<html><body><p>Redirecting to the greenhouse setup portal.</p><p><a href='http://192.168.4.1/'>Open portal</a></p></body></html>");
  }

  String buildDashboardHtml() const {
    String html;
    html.reserve(15000);
    html += F("<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>Mini Greenhouse Node</title><style>");
    html += F("body{font-family:Arial,sans-serif;background:#eef3e5;color:#203020;margin:0;padding:20px;}main{max-width:860px;margin:0 auto;}section{background:#fff;border-radius:16px;padding:18px;margin-bottom:16px;box-shadow:0 8px 24px rgba(0,0,0,.08);}h1,h2{margin:0 0 12px;} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;} .card{background:#f6f8f1;border-radius:12px;padding:12px;} label{display:block;font-weight:700;margin:10px 0 6px;} input,textarea,select{width:100%;padding:10px;border:1px solid #b9c6af;border-radius:10px;box-sizing:border-box;font:inherit;background:#fff;} textarea{min-height:88px;resize:vertical;} button{margin-top:12px;padding:10px 14px;border:none;border-radius:10px;background:#305c2b;color:#fff;font-weight:700;cursor:pointer;} button.secondary{background:#70836b;} .action-row{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px;} .action-row button{margin-top:0;} .muted{color:#5b6956;} .notice{background:#f6f8f1;border-left:4px solid #769166;border-radius:12px;padding:12px 14px;margin:12px 0;} .status-note{min-height:1.4em;margin-top:10px;} a{color:#2f6f3e;} code{background:#edf3e7;padding:2px 6px;border-radius:6px;}</style></head><body><main>");
    html += F("<section><h1>Mini Greenhouse Node</h1><p class='muted'>Local setup and dashboard for standalone greenhouse nodes.</p>");
    html += F("<div class='grid'>");
    html += String("<div class='card'><strong>Firmware</strong><br>") + GreenhouseVersion::kFirmwareVersion + "</div>";
    html += String("<div class='card'><strong>Mode</strong><br>") + modeLabel() + "</div>";
    html += String("<div class='card'><strong>Controller</strong><br>") + GreenhouseRuntime::controllerStateLabel(controllerState_) + "</div>";
    html += String("<div class='card'><strong>Health</strong><br>") + String(computeHealthScore()) + "%</div>";
    html += F("</div></section>");

    html += F("<section><h2>Access</h2>");
    if (WiFi.status() == WL_CONNECTED) {
      html += String("<p><strong>Connected Wi-Fi:</strong> ") + htmlEscape(wifiConfig_.ssid) + "</p>";
      html += String("<p><strong>Dashboard:</strong> <a href='http://") + htmlEscape(wifiConfig_.hostname) + F(".local/'>http://") + htmlEscape(wifiConfig_.hostname) + F(".local/</a></p>");
      html += String("<p><strong>IP Fallback:</strong> <a href='http://") + WiFi.localIP().toString() + F("/'>http://") + WiFi.localIP().toString() + F("/</a></p>");
    } else {
      html += F("<p><strong>Station Wi-Fi:</strong> not connected</p>");
    }
    if (wifiConfig_.hasCredentials) {
      html += String("<p><strong>Configured station target:</strong> ") + htmlEscape(wifiConfig_.ssid) +
              "<br><strong>Credential source:</strong> " +
              String(wifiConfig_.usingStoredCredentials ? "saved on this node" : "firmware defaults") +
              "<br><strong>Password stored:</strong> " +
              String(strlen(wifiConfig_.password) > 0 ? "yes" : "no") + "</p>";
    }
    if (setupApActive_) {
      html += String("<p><strong>Setup AP SSID:</strong> ") + htmlEscape(setupApSsid_) + "<br><strong>Password:</strong> " + htmlEscape(setupApPassword_) + "<br><strong>Portal:</strong> <a href='http://192.168.4.1/'>http://192.168.4.1/</a></p>";
    }
    html += F("<p><strong>JSON API:</strong> <a href='/api/state'>/api/state</a></p></section>");

    html += F("<section><h2>Status</h2><div class='grid'>");
    html += String("<div class='card'><strong>Air</strong><br>") + (snapshot_.airAvailable ? String(snapshot_.airTempC, 1) + "C / " + String(snapshot_.humidityPct, 0) + "%" : String("N/A")) + "</div>";
    html += String("<div class='card'><strong>Battery</strong><br>") + (battery_.available ? String(battery_.percent) + "% / " + String(battery_.voltageV, 2) + "V" : String("N/A")) + "</div>";
    html += String("<div class='card'><strong>Safe Mode</strong><br>") + (safeMode_ ? safeModeReasonLabel(safeModeReason_) : "OFF") + "</div>";
    html += String("<div class='card'><strong>Storage</strong><br>") + (filesystemReady_ ? "LittleFS OK" : "LittleFS OFF") + "</div>";
    html += F("</div></section>");

    if (WiFi.status() == WL_CONNECTED) {
      html += F("<section><h2>Live Dashboard</h2><p class='muted'>This station-mode dashboard polls the controller in place, shows live telemetry, and sends direct LAN control requests. The firmware does not proxy, filter, or block VPN traffic on your other clients. Only the temporary setup AP uses local DNS capture before the node joins Wi-Fi.</p>");
      html += F("<div class='grid'>");
      html += F("<div class='card'><strong>Live Mode</strong><br><span id='live-mode'>Loading...</span></div>");
      html += F("<div class='card'><strong>Controller State</strong><br><span id='live-controller'>Loading...</span></div>");
      html += F("<div class='card'><strong>Manual Override</strong><br><span id='manual-flag'>Loading...</span></div>");
      html += F("<div class='card'><strong>Air</strong><br><span id='live-air'>Loading...</span></div>");
      html += F("<div class='card'><strong>Water</strong><br><span id='live-water'>Loading...</span></div>");
      html += F("<div class='card'><strong>Light</strong><br><span id='live-light'>Loading...</span></div>");
      html += F("<div class='card'><strong>Battery</strong><br><span id='live-battery'>Loading...</span></div>");
      html += F("<div class='card'><strong>Uptime</strong><br><span id='live-uptime'>Loading...</span></div>");
      html += F("</div>");
      html += F("<div class='notice'><strong>Safe control model:</strong> AUTO returns to climate logic. OPEN and CLOSED force whole-greenhouse vent behavior. Any per-output button below enables manual override and holds that exact mix until you return to AUTO.</div>");
      html += F("<div class='action-row'><button type='button' class='secondary' data-mode='AUTO'>Return to Auto</button><button type='button' data-mode='OPEN'>Force Open</button><button type='button' data-mode='CLOSED'>Force Closed</button><button type='button' class='secondary' data-mode='MANUAL'>Hold Current Output Mix</button></div>");
      html += F("<p id='control-status' class='muted status-note'>Loading live state...</p>");
      html += F("<div class='grid'>");
      html += F("<div class='card'><strong>Top Vent</strong><br><span id='actuator-top'>Loading...</span><div class='action-row'><button type='button' data-actuator='top_open' data-state='OPEN'>Open</button><button type='button' class='secondary' data-actuator='top_open' data-state='CLOSED'>Close</button></div></div>");
      html += F("<div class='card'><strong>Bottom Vent</strong><br><span id='actuator-bottom'>Loading...</span><div class='action-row'><button type='button' data-actuator='bottom_open' data-state='OPEN'>Open</button><button type='button' class='secondary' data-actuator='bottom_open' data-state='CLOSED'>Close</button></div></div>");
      html += F("<div class='card'><strong>Exhaust Fan</strong><br><span id='actuator-exhaust'>Loading...</span><div class='action-row'><button type='button' data-actuator='fan_exhaust' data-state='ON'>On</button><button type='button' class='secondary' data-actuator='fan_exhaust' data-state='OFF'>Off</button></div></div>");
      html += F("<div class='card'><strong>Intake Fan</strong><br><span id='actuator-intake'>Loading...</span><div class='action-row'><button type='button' data-actuator='fan_intake' data-state='ON'>On</button><button type='button' class='secondary' data-actuator='fan_intake' data-state='OFF'>Off</button></div></div>");
      html += F("<div class='card'><strong>Defogger</strong><br><span id='actuator-defogger'>Loading...</span><div class='action-row'><button type='button' data-actuator='defogger' data-state='ON'>On</button><button type='button' class='secondary' data-actuator='defogger' data-state='OFF'>Off</button></div></div>");
      html += F("<div class='card'><strong>Grow Light</strong><br><span id='actuator-grow-light'>Loading...</span><div class='action-row'><button type='button' data-actuator='grow_light' data-state='ON'>On</button><button type='button' class='secondary' data-actuator='grow_light' data-state='OFF'>Off</button></div></div>");
      html += F("<div class='card'><strong>Circulation Fans</strong><br><span id='actuator-circulation'>Loading...</span><div class='action-row'><button type='button' data-actuator='circulation' data-state='ON'>On</button><button type='button' class='secondary' data-actuator='circulation' data-state='OFF'>Off</button></div></div>");
      html += F("</div>");
      html += F("<div class='grid'>");
      html += String("<div class='card'><strong>Crop Profile</strong><br>") + htmlEscape(GreenhouseLogic::cropProfile(Settings::CROP.profile).name) + "</div>";
      html += String("<div class='card'><strong>Vent Window</strong><br>") + String(runtimeClimate_.ventCloseTempC, 1) + "C to " + String(runtimeClimate_.ventOpenTempC, 1) + "C</div>";
      html += String("<div class='card'><strong>Humidity Fan</strong><br>") + String(runtimeClimate_.humidityFanOffPct, 0) + "% to " + String(runtimeClimate_.humidityFanOnPct, 0) + "%</div>";
      html += String("<div class='card'><strong>Defogger Window</strong><br>") + String(runtimeClimate_.heaterOnTempC, 1) + "C to " + String(runtimeClimate_.heaterOffTempC, 1) + "C</div>";
      html += String("<div class='card'><strong>Grow Light Window</strong><br>") + String(runtimeClimate_.growLightStartMinutes / 60U) + ":" + String(runtimeClimate_.growLightStartMinutes % 60U < 10U ? "0" : "") + String(runtimeClimate_.growLightStartMinutes % 60U) + " to " + String(runtimeClimate_.growLightStopMinutes / 60U) + ":" + String(runtimeClimate_.growLightStopMinutes % 60U < 10U ? "0" : "") + String(runtimeClimate_.growLightStopMinutes % 60U) + "</div>";
      html += String("<div class='card'><strong>Power Guards</strong><br>Low: ") + String(Settings::BATTERY.lowVoltage, 2) + "V<br>Critical: " + String(Settings::BATTERY.criticalVoltage, 2) + "V</div>";
      html += F("</div></section>");

      html += F("<section><h2>Climate Settings</h2><p class='muted'>Save updated climate thresholds directly from the station dashboard. Editing requires the current station Wi-Fi password, or the node setup password if the station network is open.</p>");
      html += String("<p class='muted'>Active profile source: <strong>") + (climateOverrideActive_ ? "dashboard override" : "firmware defaults") + "</strong></p>";
      html += F("<div class='grid'>");
      html += F("<div class='card'><label for='settings-password'>Settings password</label><input id='settings-password' type='password' autocomplete='current-password' placeholder='Station Wi-Fi password'></div>");
      html += String("<div class='card'><label for='vent-open-temp'>Vent open temp C</label><input id='vent-open-temp' type='number' step='0.1' value='") + String(runtimeClimate_.ventOpenTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='vent-close-temp'>Vent close temp C</label><input id='vent-close-temp' type='number' step='0.1' value='") + String(runtimeClimate_.ventCloseTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='fan-on-temp'>Fan on temp C</label><input id='fan-on-temp' type='number' step='0.1' value='") + String(runtimeClimate_.fanOnTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='fan-off-temp'>Fan off temp C</label><input id='fan-off-temp' type='number' step='0.1' value='") + String(runtimeClimate_.fanOffTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='humidity-fan-on'>Humidity fan on %</label><input id='humidity-fan-on' type='number' step='0.1' value='") + String(runtimeClimate_.humidityFanOnPct, 1) + "'></div>";
      html += String("<div class='card'><label for='humidity-fan-off'>Humidity fan off %</label><input id='humidity-fan-off' type='number' step='0.1' value='") + String(runtimeClimate_.humidityFanOffPct, 1) + "'></div>";
      html += String("<div class='card'><label for='heater-on-temp'>Defogger on temp C</label><input id='heater-on-temp' type='number' step='0.1' value='") + String(runtimeClimate_.heaterOnTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='heater-off-temp'>Defogger off temp C</label><input id='heater-off-temp' type='number' step='0.1' value='") + String(runtimeClimate_.heaterOffTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='water-high-temp'>Water high temp C</label><input id='water-high-temp' type='number' step='0.1' value='") + String(runtimeClimate_.waterHighTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='water-low-temp'>Water low temp C</label><input id='water-low-temp' type='number' step='0.1' value='") + String(runtimeClimate_.waterLowTempC, 1) + "'></div>";
      html += String("<div class='card'><label for='grow-light-lux'>Grow-light lux threshold</label><input id='grow-light-lux' type='number' step='1' value='") + String(runtimeClimate_.growLightLuxThreshold, 0) + "'></div>";
      html += F("</div><div class='action-row'><button type='button' id='save-climate-settings'>Save Climate Settings</button><button type='button' class='secondary' id='reset-climate-settings'>Restore Firmware Defaults</button></div><p id='settings-status' class='muted status-note'>Climate thresholds are loaded from the active controller profile.</p></section>");
    }

    html += F("<section><h2>Configure Wi-Fi</h2><div class='notice'><strong>Fast import:</strong> browsers cannot read saved Wi-Fi passwords directly, but many phones and PCs can copy or share a saved network as text or a QR payload. Paste that copied Wi-Fi share text here, use the clipboard button below, or pick a nearby SSID from the scan dropdown.</div>");
    html += F("<label for='ssid-select'>Nearby networks</label><select id='ssid-select'><option value=''>Scan to choose a nearby network</option></select><div class='action-row'><button type='button' class='secondary' id='scan-networks'>Refresh nearby networks</button></div>");
    html += F("<label for='wifi-import'>Wi-Fi share text or QR payload</label><textarea id='wifi-import' placeholder='Examples: WIFI:T:WPA;S:GreenhouseNet;P:secret123;; or ssid=GreenhouseNet password=secret123'></textarea>");
    html += F("<div class='action-row'><button type='button' id='import-clipboard'>Use copied Wi-Fi</button><button type='button' class='secondary' id='import-text'>Import pasted text</button></div><p id='import-status' class='muted status-note'>Supports standard Wi-Fi QR text such as <code>WIFI:T:WPA;S:MySSID;P:MyPassword;;</code>.</p>");
    html += F("<form method='post' action='/wifi'>");
    html += F("<label for='ssid'>SSID</label><input id='ssid' name='ssid' maxlength='32' required value='");
    html += htmlEscape(wifiConfig_.ssid);
    html += F("'>");
    html += F("<label for='password'>Password</label><input id='password' name='password' type='password' maxlength='64' value=''><p class='muted'>Leave blank only for open networks. Saving the same SSID again keeps the existing saved password.</p>");
    html += F("<label for='hostname'>Hostname</label><input id='hostname' name='hostname' maxlength='32' value='");
    html += htmlEscape(wifiConfig_.hostname);
    html += F("'><button type='submit'>Save Wi-Fi</button></form>");
    html += F("<form method='post' action='/wifi/reset'><button type='submit'>Forget Saved Wi-Fi</button></form>");
    html += F("<script>(function(){const ssid=document.getElementById('ssid');const password=document.getElementById('password');const networkSelect=document.getElementById('ssid-select');const scanButton=document.getElementById('scan-networks');const importBox=document.getElementById('wifi-import');const importStatus=document.getElementById('import-status');const clipboardButton=document.getElementById('import-clipboard');const importButton=document.getElementById('import-text');let scanRetryTimer=0;function setStatus(message){importStatus.textContent=message;}function applyParsed(parsed,sourceLabel){ssid.value=parsed.ssid||'';password.value=parsed.password||'';if(parsed.ssid){ssid.focus();}setStatus('Imported Wi-Fi details from '+sourceLabel+'. Review and press Save Wi-Fi.');}function unescapeWifiValue(value){let result='';let escaping=false;for(let index=0;index<value.length;index+=1){const ch=value[index];if(escaping){result+=ch;escaping=false;continue;}if(ch==='\\'){escaping=true;continue;}result+=ch;}if(escaping){result+='\\';}return result;}function readValue(text,keyNames){const lower=text.toLowerCase();for(let index=0;index<keyNames.length;index+=1){const keyName=keyNames[index];let marker=keyName+'=';let start=lower.indexOf(marker);if(start>=0){start+=marker.length;let end=start;while(end<text.length&&text[end]!==';'&&text[end]!==','&&text.charCodeAt(end)!==10&&text.charCodeAt(end)!==13){end+=1;}return text.substring(start,end).trim();}marker=keyName+':';start=lower.indexOf(marker);if(start>=0){start+=marker.length;let end=start;while(end<text.length&&text[end]!==';'&&text[end]!==','&&text.charCodeAt(end)!==10&&text.charCodeAt(end)!==13){end+=1;}return text.substring(start,end).trim();}}return '';}function parseWifiPayload(rawText){const text=rawText.trim();if(!text){return {error:'Paste or copy Wi-Fi share text first.'};}if(text.indexOf('WIFI:')===0){const fields=text.substring(5).split(';');let wifiSsid='';let wifiPassword='';for(let index=0;index<fields.length;index+=1){const field=fields[index];const colonIndex=field.indexOf(':');if(colonIndex<=0){continue;}const key=field.substring(0,colonIndex).toUpperCase();const value=unescapeWifiValue(field.substring(colonIndex+1));if(key==='S'){wifiSsid=value;}if(key==='P'){wifiPassword=value;}}if(wifiSsid){return {ssid:wifiSsid,password:wifiPassword};}}try{const json=JSON.parse(text);if(json&&typeof json==='object'&&typeof json.ssid==='string'){return {ssid:json.ssid,password:typeof json.password==='string'?json.password:''};}}catch(_error){}const parsedSsid=readValue(text,['ssid']);const parsedPassword=readValue(text,['password','pass','psk','key']);if(parsedSsid){return {ssid:parsedSsid,password:parsedPassword};}return {error:'Could not parse Wi-Fi details. Paste a standard Wi-Fi QR string like WIFI:T:WPA;S:MySSID;P:MyPassword;; or text with ssid= and password=.'};}function populateNetworks(networks){networkSelect.innerHTML='';const promptOption=new Option('Choose a nearby network','');networkSelect.add(promptOption);for(let index=0;index<networks.length;index+=1){const network=networks[index];const label=network.ssid+(network.secure?'':' (open)')+' ['+network.rssi+' dBm]';const option=new Option(label,network.ssid,false,network.ssid===ssid.value);networkSelect.add(option);}if(ssid.value){networkSelect.value=ssid.value;}}function scheduleScanRetry(){if(scanRetryTimer){return;}scanRetryTimer=window.setTimeout(function(){scanRetryTimer=0;refreshNetworks(false);},1500);}function refreshNetworks(forceRefresh){scanButton.disabled=true;setStatus('Checking nearby Wi-Fi networks...');fetch(forceRefresh?'/api/wifi/scan?refresh=1':'/api/wifi/scan',{cache:'no-store'}).then(function(response){return response.json().then(function(payload){return {response:response,payload:payload};});}).then(function(result){if(!result.response.ok){throw new Error(result.payload&&result.payload.error?result.payload.error:'scan failed');}const payload=result.payload||{};const networks=Array.isArray(payload.networks)?payload.networks:[];populateNetworks(networks);if(payload.connected&&!ssid.value){ssid.value=payload.connected;}if(networks.length){setStatus('Pick a nearby network from the dropdown or import Wi-Fi share text.');return;}if(payload.refreshing){setStatus('Scanning nearby Wi-Fi networks. The list will update automatically.');scheduleScanRetry();return;}setStatus(payload.error||'No nearby networks were found. You can still type or import the Wi-Fi details manually.');}).catch(function(){setStatus('Nearby network scan failed. You can still type or import the Wi-Fi details manually.');}).finally(function(){scanButton.disabled=false;});}function importFromClipboard(){if(!navigator.clipboard||!navigator.clipboard.readText){setStatus('Clipboard import is not available in this browser. Paste the Wi-Fi share text into the box instead.');return;}navigator.clipboard.readText().then(function(text){importBox.value=text;importFromText('clipboard');}).catch(function(){setStatus('Clipboard access was blocked. Paste the Wi-Fi share text into the box instead.');});}function importFromText(sourceLabel){const parsed=parseWifiPayload(importBox.value);if(parsed.error){setStatus(parsed.error);return;}applyParsed(parsed,sourceLabel);}clipboardButton.addEventListener('click',function(){importFromClipboard();});importButton.addEventListener('click',function(){importFromText('pasted text');});scanButton.addEventListener('click',function(){refreshNetworks(true);});networkSelect.addEventListener('change',function(){if(networkSelect.value){ssid.value=networkSelect.value;ssid.focus();setStatus('Selected nearby network. Type or import only the password now.');}});refreshNetworks(false);})();</script></section>");
    html += F("<section><h2>Update Firmware</h2><p class='muted'>Upload the latest <code>firmware.bin</code> over Wi-Fi once the node is installed. Use stable power, keep USB as the recovery path, and do not start an update during weather that may need immediate vent control.</p><form method='post' action='/update' enctype='multipart/form-data'><label for='firmware'>Firmware binary</label><input id='firmware' name='firmware' type='file' accept='.bin,application/octet-stream' required><button type='submit'>Install Firmware Update</button></form></section>");
    if (WiFi.status() == WL_CONNECTED) {
      html += buildLiveDashboardScript();
      html += buildClimateSettingsScript();
    }
    html += F("</main></body></html>");
    return html;
  }

  String buildLiveDashboardScript() const {
    return F("<script>(function(){const statusEl=document.getElementById('control-status');if(!statusEl){return;}const modeButtons=document.querySelectorAll('[data-mode]');const actuatorButtons=document.querySelectorAll('[data-actuator]');function setText(id,text){const el=document.getElementById(id);if(el){el.textContent=text;}}function fmtNumber(value,digits,suffix){return typeof value==='number'&&isFinite(value)?value.toFixed(digits)+suffix:'N/A';}function fmtWhole(value,suffix){return typeof value==='number'&&isFinite(value)?String(Math.round(value))+suffix:'N/A';}function fmtDuration(ms){if(typeof ms!=='number'||!isFinite(ms)){return 'N/A';}const totalSeconds=Math.floor(ms/1000);const hours=Math.floor(totalSeconds/3600);const minutes=Math.floor((totalSeconds%3600)/60);const seconds=totalSeconds%60;return hours+'h '+minutes+'m '+seconds+'s';}async function postForm(url,fields){const response=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:new URLSearchParams(fields).toString(),cache:'no-store'});const payload=await response.json().catch(function(){return {ok:false,message:'Controller returned invalid JSON.'};});if(!response.ok&&(!payload||!payload.message)){payload={ok:false,message:'Request failed.'};}return payload;}function applyState(state){setText('live-mode',state&&state.mode?state.mode:'N/A');setText('live-controller',state&&state.controller_state?state.controller_state:'N/A');setText('manual-flag',state&&state.manual_override&&state.manual_override.active?'ACTIVE':'OFF');setText('live-air',state&&state.air&&state.air.available?fmtNumber(state.air.temp_c,1,' C')+' / '+fmtWhole(state.air.humidity_pct,' %'):'N/A');setText('live-water',state&&state.water&&state.water.available?fmtNumber(state.water.temp_c,1,' C'):'N/A');setText('live-light',state&&state.light&&state.light.available?fmtWhole(state.light.lux,' lux'):'N/A');setText('live-battery',state&&state.battery&&state.battery.available?fmtWhole(state.battery.percent,' %')+' / '+fmtNumber(state.battery.voltage_v,2,' V'):'N/A');setText('live-uptime',state?fmtDuration(state.uptime_ms):'N/A');setText('actuator-top',state&&state.actuators&&state.actuators.top_open?'OPEN':'SHUT');setText('actuator-bottom',state&&state.actuators&&state.actuators.bottom_open?'OPEN':'SHUT');setText('actuator-exhaust',state&&state.actuators&&state.actuators.fan_exhaust?'ON':'OFF');setText('actuator-intake',state&&state.actuators&&state.actuators.fan_intake?'ON':'OFF');setText('actuator-defogger',state&&state.actuators&&state.actuators.defogger?'ON':'OFF');setText('actuator-grow-light',state&&state.actuators&&state.actuators.grow_light?'ON':'OFF');setText('actuator-circulation',state&&state.actuators&&state.actuators.circulation?'ON':'OFF');if(state&&state.safe_mode&&state.safe_mode.active){statusEl.textContent='Safe mode active: '+state.safe_mode.reason+'. Live commands are locked until the fault clears.';return;}if(state&&state.manual_override&&state.manual_override.active){statusEl.textContent='Manual override active. Return to Auto to hand control back to greenhouse logic.';return;}statusEl.textContent='Live dashboard connected. Controls are available over normal LAN access.';}async function refreshState(){try{const response=await fetch('/api/state',{cache:'no-store'});const state=await response.json();applyState(state);}catch(error){statusEl.textContent='Live refresh failed. Reload after the node settles on Wi-Fi.';}}modeButtons.forEach(function(button){button.addEventListener('click',async function(){statusEl.textContent='Sending controller mode command...';const payload=await postForm('/api/control/mode',{mode:button.getAttribute('data-mode')||''});statusEl.textContent=payload&&payload.message?payload.message:'Mode command sent.';refreshState();});});actuatorButtons.forEach(function(button){button.addEventListener('click',async function(){statusEl.textContent='Sending actuator command...';const payload=await postForm('/api/control/actuator',{actuator:button.getAttribute('data-actuator')||'',state:button.getAttribute('data-state')||''});statusEl.textContent=payload&&payload.message?payload.message:'Actuator command sent.';refreshState();});});refreshState();window.setInterval(refreshState,2000);})();</script>");
  }

  String buildClimateSettingsScript() const {
    return F("<script>(function(){const statusEl=document.getElementById('settings-status');if(!statusEl){return;}const saveButton=document.getElementById('save-climate-settings');const resetButton=document.getElementById('reset-climate-settings');const passwordEl=document.getElementById('settings-password');const fields={vent_open_temp_c:document.getElementById('vent-open-temp'),vent_close_temp_c:document.getElementById('vent-close-temp'),fan_on_temp_c:document.getElementById('fan-on-temp'),fan_off_temp_c:document.getElementById('fan-off-temp'),humidity_fan_on_pct:document.getElementById('humidity-fan-on'),humidity_fan_off_pct:document.getElementById('humidity-fan-off'),heater_on_temp_c:document.getElementById('heater-on-temp'),heater_off_temp_c:document.getElementById('heater-off-temp'),water_high_temp_c:document.getElementById('water-high-temp'),water_low_temp_c:document.getElementById('water-low-temp'),grow_light_lux_threshold:document.getElementById('grow-light-lux')};function setValue(id,value,digits){const el=fields[id];if(el&&typeof value==='number'&&isFinite(value)){el.value=digits>=0?value.toFixed(digits):String(value);}}function applyPayload(payload){if(!payload||!payload.climate){return;}setValue('vent_open_temp_c',payload.climate.vent_open_temp_c,1);setValue('vent_close_temp_c',payload.climate.vent_close_temp_c,1);setValue('fan_on_temp_c',payload.climate.fan_on_temp_c,1);setValue('fan_off_temp_c',payload.climate.fan_off_temp_c,1);setValue('humidity_fan_on_pct',payload.climate.humidity_fan_on_pct,1);setValue('humidity_fan_off_pct',payload.climate.humidity_fan_off_pct,1);setValue('heater_on_temp_c',payload.climate.heater_on_temp_c,1);setValue('heater_off_temp_c',payload.climate.heater_off_temp_c,1);setValue('water_high_temp_c',payload.climate.water_high_temp_c,1);setValue('water_low_temp_c',payload.climate.water_low_temp_c,1);setValue('grow_light_lux_threshold',payload.climate.grow_light_lux_threshold,0);}async function postSettings(url,body){const response=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:new URLSearchParams(body).toString(),cache:'no-store'});const payload=await response.json().catch(function(){return {ok:false,message:'Controller returned invalid settings JSON.'};});if(payload){applyPayload(payload);}statusEl.textContent=payload&&payload.message?payload.message:'Settings request finished.';}saveButton.addEventListener('click',async function(){const password=passwordEl&&passwordEl.value?passwordEl.value:'';const body={password:password};Object.keys(fields).forEach(function(key){body[key]=fields[key]&&fields[key].value?fields[key].value:'';});statusEl.textContent='Saving climate thresholds...';await postSettings('/api/settings/climate',body);});resetButton.addEventListener('click',async function(){const password=passwordEl&&passwordEl.value?passwordEl.value:'';statusEl.textContent='Restoring firmware climate defaults...';await postSettings('/api/settings/climate/reset',{password:password});});})();</script>");
  }

  String htmlEscape(const char *value) const {
    String escaped;
    if (value == nullptr) {
      return escaped;
    }
    for (const char *cursor = value; *cursor != '\0'; ++cursor) {
      switch (*cursor) {
        case '&':
          escaped += F("&amp;");
          break;
        case '<':
          escaped += F("&lt;");
          break;
        case '>':
          escaped += F("&gt;");
          break;
        case '"':
          escaped += F("&quot;");
          break;
        case '\'':
          escaped += F("&#39;");
          break;
        default:
          escaped += *cursor;
          break;
      }
    }
    return escaped;
  }

  String jsonEscape(const String &value) const {
    String escaped;
    escaped.reserve(value.length() + 8U);
    for (size_t index = 0; index < value.length(); ++index) {
      switch (value[index]) {
        case '\\':
          escaped += F("\\\\");
          break;
        case '"':
          escaped += F("\\\"");
          break;
        case '\b':
          escaped += F("\\b");
          break;
        case '\f':
          escaped += F("\\f");
          break;
        case '\n':
          escaped += F("\\n");
          break;
        case '\r':
          escaped += F("\\r");
          break;
        case '\t':
          escaped += F("\\t");
          break;
        default:
          escaped += value[index];
          break;
      }
    }
    return escaped;
  }

  void saveWifiConfig(const char *ssid, const char *password, const char *hostname) {
    preferences_.putString("wifi_ssid", ssid == nullptr ? "" : ssid);
    preferences_.putString("wifi_pass", password == nullptr ? "" : password);
    const char *resolvedHostname = (hostname != nullptr && strlen(hostname) > 0) ? hostname : Settings::WIFI.hostname;
    preferences_.putString("wifi_host", resolvedHostname == nullptr ? "mini-greenhouse" : resolvedHostname);
  }

  bool cachedNetworkIsSecure(const String &ssid) const {
    for (size_t index = 0; index < wifiScanResultCount_; ++index) {
      if (ssid == wifiScanEntries_[index].ssid) {
        return wifiScanEntries_[index].secure;
      }
    }
    return false;
  }

  void buildSetupAccessPointIdentity() {
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
    snprintf(setupApSsid_, sizeof(setupApSsid_), "GH-SETUP-%06lX", static_cast<unsigned long>(suffix));
    snprintf(setupApPassword_, sizeof(setupApPassword_), "grow-%06lX", static_cast<unsigned long>(suffix));
  }

  void copySettingString(char *target, size_t targetSize, const char *source) {
    if (targetSize == 0U) {
      return;
    }
    if (source == nullptr) {
      target[0] = '\0';
      return;
    }
    strlcpy(target, source, targetSize);
  }

  void setMode(ControlMode mode) {
    mode_ = mode;
    manualOverrideActive_ = false;
    preferences_.putUChar("mode", static_cast<uint8_t>(mode_));
    publishModeCommandState();
    renderDisplay(true);
  }

  void noteDisplayActivity(uint32_t now) {
    if (!displayReady_) {
      return;
    }

    lastDisplayActivityAt_ = now;
    if (displaySleeping_) {
      GreenhouseHal::setDisplayPower(display_, true);
      GreenhouseHal::setDisplayContrast(display_, Settings::OLED.activeContrast);
      displaySleeping_ = false;
    }
  }

  void updateDisplaySleep(uint32_t now) {
    const uint32_t timeoutMs = safeMode_
                                   ? Settings::OLED.safeModeScreenSaverTimeoutMs
                                   : Settings::OLED.screenSaverTimeoutMs;
    if (!displayReady_ || displaySleeping_ || timeoutMs == 0UL) {
      return;
    }
    if (now - lastDisplayActivityAt_ < timeoutMs) {
      return;
    }

    GreenhouseHal::setDisplayContrast(display_, Settings::OLED.screenSaverContrast);
    displaySleeping_ = true;
    initScreenSaver(now);
    renderDisplay(false);
    lastDisplayAt_ = now;
  }

  void initScreenSaver(uint32_t now) {
    screenSaverFrame_ = 0;
    resetScreenSaverScene(now, static_cast<ScreenSaverMode>(random(0, 2)));
  }

  void resetScreenSaverScene(uint32_t now, ScreenSaverMode mode) {
    screenSaverMode_ = mode;
    screenSaverDriftX_ = static_cast<int8_t>(random(-2, 3));
    screenSaverDriftY_ = static_cast<int8_t>(random(-1, 2));
    resetScreenSaverGrass();
    resetScreenSaverParticles();
    resetScreenSaverPlant();
    lastScreenSaverRefreshAt_ = now;
    const uint32_t jitter = Settings::OLED.screenSaverModeJitterMs == 0UL
                                ? 0UL
                                : static_cast<uint32_t>(random(static_cast<long>(Settings::OLED.screenSaverModeJitterMs)));
    nextScreenSaverModeSwitchAt_ = now + Settings::OLED.screenSaverModeBaseMs + jitter;
  }

  void resetScreenSaverGrass() {
    for (uint8_t index = 0; index < kScreenSaverGrassCount; ++index) {
      screenSaverGrass_[index].baseX = static_cast<int16_t>(random(-4, Settings::DISPLAY_WIDTH + 4));
      screenSaverGrass_[index].height = static_cast<uint8_t>(random(4, 11));
      screenSaverGrass_[index].phase = static_cast<int8_t>(random(0, 20));
    }
  }

  void resetScreenSaverParticle(ScreenSaverParticle &particle, bool highStart) {
    const int16_t minimumY = static_cast<int16_t>(highStart ? Settings::DISPLAY_HEIGHT / 2 : Settings::DISPLAY_HEIGHT);
    particle.x8 = static_cast<int16_t>(random(0, Settings::DISPLAY_WIDTH * 8));
    particle.y8 = static_cast<int16_t>(random(minimumY, Settings::DISPLAY_HEIGHT + 10) * 8);
    particle.vx8 = static_cast<int8_t>(random(-2, 3));
    particle.vy8 = static_cast<uint8_t>(random(1, 4));
    particle.twinkle = static_cast<uint8_t>(random(0, 4));
  }

  void resetScreenSaverParticles() {
    for (uint8_t index = 0; index < kScreenSaverParticleCount; ++index) {
      resetScreenSaverParticle(screenSaverParticles_[index], false);
    }
  }

  void resetScreenSaverPlant() {
    screenSaverPlant_.baseX = static_cast<int16_t>(random(24, Settings::DISPLAY_WIDTH - 24));
    screenSaverPlant_.ageFrames = 0;
    screenSaverPlant_.leanBias = static_cast<int8_t>(random(-3, 4));
    screenSaverPlant_.leafSpan = static_cast<uint8_t>(random(4, 8));
  }

  int16_t triangleWave(uint16_t phase, uint8_t amplitude) const {
    if (amplitude == 0U) {
      return 0;
    }
    const uint8_t period = static_cast<uint8_t>(amplitude * 4U);
    const uint8_t folded = static_cast<uint8_t>(phase % period);
    if (folded < amplitude * 2U) {
      return static_cast<int16_t>(folded) - static_cast<int16_t>(amplitude);
    }
    return static_cast<int16_t>(amplitude * 3U) - static_cast<int16_t>(folded);
  }

  void updateScreenSaver(uint32_t now) {
    ++screenSaverFrame_;
    if (now - lastScreenSaverRefreshAt_ >= kScreenSaverRegenerationMs) {
      screenSaverDriftX_ = static_cast<int8_t>(random(-2, 3));
      screenSaverDriftY_ = static_cast<int8_t>(random(-1, 2));
      resetScreenSaverGrass();
      lastScreenSaverRefreshAt_ = now;
    }
    if (now >= nextScreenSaverModeSwitchAt_) {
      const ScreenSaverMode nextMode = screenSaverMode_ == ScreenSaverMode::night
                                           ? ScreenSaverMode::growth
                                           : ScreenSaverMode::night;
      resetScreenSaverScene(now, nextMode);
    }

    for (uint8_t index = 0; index < kScreenSaverParticleCount; ++index) {
      ScreenSaverParticle &particle = screenSaverParticles_[index];
      particle.x8 += particle.vx8;
      particle.y8 -= particle.vy8;
      if (((screenSaverFrame_ + index) % 7U) == 0U) {
        const int16_t updated = particle.vx8 + static_cast<int8_t>(random(-1, 2));
        particle.vx8 = static_cast<int8_t>(constrain(updated, -3, 3));
      }

      if (particle.x8 < 0) {
        particle.x8 += static_cast<int16_t>(Settings::DISPLAY_WIDTH * 8);
      } else if (particle.x8 >= static_cast<int16_t>(Settings::DISPLAY_WIDTH * 8)) {
        particle.x8 -= static_cast<int16_t>(Settings::DISPLAY_WIDTH * 8);
      }

      if (particle.y8 < 8) {
        resetScreenSaverParticle(particle, screenSaverMode_ == ScreenSaverMode::night);
      }
    }

    if (screenSaverMode_ == ScreenSaverMode::growth) {
      if (screenSaverPlant_.ageFrames < 180U) {
        ++screenSaverPlant_.ageFrames;
      } else {
        resetScreenSaverPlant();
      }
    }
  }

  void drawScreenSaverGrass() {
    const int16_t baseY = static_cast<int16_t>(Settings::DISPLAY_HEIGHT - 2 + screenSaverDriftY_);
    for (uint8_t index = 0; index < kScreenSaverGrassCount; ++index) {
      const ScreenSaverBlade &blade = screenSaverGrass_[index];
      const int16_t startX = static_cast<int16_t>(blade.baseX + screenSaverDriftX_);
      const int16_t sway = triangleWave(static_cast<uint16_t>(screenSaverFrame_ + blade.phase + index), 2U);
      const int16_t tipX = startX + sway;
      const int16_t tipY = baseY - blade.height;
      if (startX < -2 || startX >= Settings::DISPLAY_WIDTH + 2 || tipY < 0) {
        continue;
      }
      display_.drawLine(startX, baseY, tipX, tipY, SSD1306_WHITE);
    }
  }

  void drawScreenSaverParticles(bool fireflies) {
    for (uint8_t index = 0; index < kScreenSaverParticleCount; ++index) {
      const ScreenSaverParticle &particle = screenSaverParticles_[index];
      const int16_t x = particle.x8 / 8;
      const int16_t y = particle.y8 / 8;
      if (x < 0 || x >= Settings::DISPLAY_WIDTH || y < 0 || y >= Settings::DISPLAY_HEIGHT) {
        continue;
      }
      display_.drawPixel(x, y, SSD1306_WHITE);
      if (fireflies && ((screenSaverFrame_ + particle.twinkle + index) % 9U) == 0U && x + 1 < Settings::DISPLAY_WIDTH) {
        display_.drawPixel(x + 1, y, SSD1306_WHITE);
      }
    }
  }

  void drawGrowthPlant() {
    const int16_t baseY = static_cast<int16_t>(Settings::DISPLAY_HEIGHT - 5 + screenSaverDriftY_);
    const int16_t sway = triangleWave(static_cast<uint16_t>(screenSaverFrame_ / 2U + screenSaverPlant_.leanBias), 2U);
    const int16_t x = static_cast<int16_t>(screenSaverPlant_.baseX + screenSaverDriftX_);

    display_.drawPixel(x, baseY, SSD1306_WHITE);
    display_.drawLine(x, baseY, x + sway, baseY - 3, SSD1306_WHITE);

    if (screenSaverPlant_.ageFrames >= 20U) {
      const int16_t stemTopY = baseY - static_cast<int16_t>(min<uint16_t>(12U, 4U + screenSaverPlant_.ageFrames / 10U));
      display_.drawLine(x + sway, baseY - 3, x + sway, stemTopY, SSD1306_WHITE);
      if (screenSaverPlant_.ageFrames >= 70U) {
        const int16_t leafY = stemTopY + 4;
        const uint8_t growthLeafSpan = static_cast<uint8_t>(2U + ((screenSaverPlant_.ageFrames - 70U) / 18U));
        const int16_t leafSpan = static_cast<int16_t>(min<uint8_t>(screenSaverPlant_.leafSpan, growthLeafSpan));
        display_.drawLine(x + sway, leafY, x + sway - leafSpan, leafY - 3, SSD1306_WHITE);
        display_.drawLine(x + sway, leafY, x + sway + leafSpan, leafY - 2, SSD1306_WHITE);
      }
    }
  }

  void renderScreenSaver(uint32_t now) {
    updateScreenSaver(now);
    display_.clearDisplay();
    drawScreenSaverGrass();
    if (screenSaverMode_ == ScreenSaverMode::night) {
      drawScreenSaverParticles(true);
    } else {
      drawGrowthPlant();
      drawScreenSaverParticles(false);
    }
    display_.display();
  }

  SensorSnapshot readSensors(uint32_t now) {
    return GreenhouseHal::readSensors(now,
                                      bme_,
                                      dht_,
                                      lightSensor_,
                                      waterSensor_,
                                      sensorHardware_,
                                      sensorFaults_,
                                      sensorCache_);
  }

  void applyConfigurationSafety() {
    GreenhouseRuntime::ValidationInput input{};
    populateValidationInput(input, runtimeClimate_);

    configFault_ = GreenhouseRuntime::validateConfiguration(input);
    if (configFault_ != ConfigFault::none) {
      enterSafeMode(SafeModeReason::configFault);
    }
  }

  void evaluateSensorFaults(uint32_t now) {
    if (sensorFaults_.lightReadFailures > recordedLightFailures_) {
      recordDiagnosticEvent(DiagnosticCode::lightSensorReadFailure,
                            static_cast<int32_t>(sensorFaults_.lightReadFailures));
      recordedLightFailures_ = sensorFaults_.lightReadFailures;
    }
    if (sensorFaults_.waterReadFailures > recordedWaterFailures_) {
      recordDiagnosticEvent(DiagnosticCode::waterSensorReadFailure,
                            static_cast<int32_t>(sensorFaults_.waterReadFailures));
      recordedWaterFailures_ = sensorFaults_.waterReadFailures;
    }

    if (snapshot_.airAvailable) {
      if (sensorFaults_.lastAirFaultAt > 0 &&
          now - sensorFaults_.lastAirFaultAt >= Settings::RELIABILITY.sensorFaultRecoveryDelayMs) {
        sensorFaults_.consecutiveAirFaults = 0;
      }
      if (sensorFaults_.airUnavailableLogged) {
        recordDiagnosticEvent(DiagnosticCode::airSensorRecovered,
                              static_cast<int32_t>(now - sensorFaults_.lastAirFaultAt));
        sensorFaults_.airUnavailableLogged = false;
      }
      sensorFaults_.lastAirRecoveryAt = now;
      return;
    }

    sensorFaults_.lastAirFaultAt = now;
    if (sensorFaults_.consecutiveAirFaults < 255U) {
      ++sensorFaults_.consecutiveAirFaults;
    }
    if (!sensorFaults_.airUnavailableLogged) {
      recordDiagnosticEvent(DiagnosticCode::airSensorUnavailable,
                            static_cast<int32_t>(sensorFaults_.consecutiveAirFaults));
      sensorFaults_.airUnavailableLogged = true;
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
    const bool daylight = GreenhouseLogic::resolveDaylight(snapshot_, runtimeClimate_, hasTime, currentMinute);

    actuators_ = GreenhouseLogic::evaluateActuators({
        snapshot_,
        actuators_,
        mode_,
      runtimeClimate_,
        Settings::controlSystemConfig(),
        daylight,
        hasTime,
        currentMinute,
    });

    if (manualOverrideActive_) {
      actuators_ = manualActuators_;
    }
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
      if (servoProtection_.commandBlockedCount < UINT32_MAX) {
        ++servoProtection_.commandBlockedCount;
      }
      if (servoProtection_.protectionTrips < 255U) {
        ++servoProtection_.protectionTrips;
      }
      if (now - lastServoBlockedEventAt_ >= Settings::DIAGNOSTICS.eventRateLimitMs) {
        recordDiagnosticEvent(DiagnosticCode::servoCommandBlocked,
                              static_cast<int32_t>(servoProtection_.commandBlockedCount));
        lastServoBlockedEventAt_ = now;
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
    servoProtection_.lastMoveStartedAt = now;
    servoProtection_.motionEndsAt = now + Settings::RELIABILITY.servoDriveWindowMs;
    servoProtection_.cooldownEndsAt = servoProtection_.motionEndsAt + Settings::RELIABILITY.servoCooldownMs;
    preferences_.putBool("servo_pending", true);
    preferences_.putBool("servo_top_open", topOpen);
    preferences_.putBool("servo_bot_open", bottomOpen);
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
    servoProtection_.lastMoveCompletedAt = now;
    servoProtection_.lastMoveDurationMs = now - servoProtection_.lastMoveStartedAt;
    if (servoProtection_.lastMoveDurationMs > servoProtection_.longestMoveDurationMs) {
      servoProtection_.longestMoveDurationMs = servoProtection_.lastMoveDurationMs;
    }
    if (servoProtection_.lastMoveDurationMs > Settings::DIAGNOSTICS.servoSlowMoveThresholdMs) {
      recordDiagnosticEvent(DiagnosticCode::servoMoveSlow,
                            static_cast<int32_t>(servoProtection_.lastMoveDurationMs));
    }
    preferences_.putBool("servo_pending", false);
  }

  void renderDisplay(bool fullRefresh) {
    if (!displayReady_) {
      return;
    }

    if (fullRefresh) {
      noteDisplayActivity(millis());
    }
    if (displaySleeping_) {
      renderScreenSaver(millis());
      return;
    }

    char airBuffer[24];
    char waterBuffer[16];
    char lightBuffer[16];
    char statusBuffer[20];
    char vpdBuffer[16];
    char dewBuffer[16];
    char batteryBuffer[16];
    char loraBuffer[24];
    char commandBuffer[24];
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
             WiFi.status() == WL_CONNECTED ? "WiFi" : (setupApActive_ ? "AP" : "OFF"),
             mqttClient_.connected() ? "/MQTT" : "",
             filesystemReady_ ? "" : " FS!");
    snprintf(loraBuffer,
         sizeof(loraBuffer),
         "%s Q%u A%lu",
         loraLink_.stats().backendReady ? "LORA" : "NO-LORA",
         static_cast<unsigned>(loraLink_.stats().queueDepth),
         static_cast<unsigned long>(loraLink_.stats().acknowledgedFrames));
    snprintf(commandBuffer,
         sizeof(commandBuffer),
         "%s %s",
         GreenhouseRemote::modeCommandLabel(lastCommandAudit_.requested),
         GreenhouseRemote::commandStatusLabel(lastCommandAudit_.status));

    if (fullRefresh) {
      display_.clearDisplay();
    }

    display_.clearDisplay();
    display_.setCursor(0, 0);
    if (safeMode_) {
      display_.printf("SAFE MODE\n");
      display_.printf("FW:%s\n", GreenhouseVersion::kFirmwareVersion);
      display_.printf("Reason:%s\n", safeModeReasonLabel(safeModeReason_));
      display_.printf("Reset:%s\n", resetReasonLabel(lastResetReason_));
      display_.printf("Batt:%s\n", batteryBuffer);
      display_.printf("Air: %s\n", airBuffer);
      display_.printf("Boot:%u/%u\n", static_cast<unsigned>(bootCount_), static_cast<unsigned>(bootFailures_));
      display_.printf("MODE=resume");
    } else if (((millis() / Settings::LOGGING.displayIntervalMs) % 3U) == 0U) {
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
    } else if (((millis() / Settings::LOGGING.displayIntervalMs) % 3U) == 1U) {
      const CropProfile &profile = GreenhouseLogic::cropProfile(Settings::CROP.profile);
      display_.printf("Crop: %s\n", profile.name);
      display_.printf("State:%s\n", cropStatusLabel(metrics_.cropStatus));
      display_.printf("VPD:  %s\n", vpdBuffer);
      display_.printf("Dew:  %s\n", dewBuffer);
      display_.printf("Frost:%s\n", metrics_.frostRisk ? "YES" : "NO");
      display_.printf("Batt: %s\n", batteryBuffer);
      display_.printf("Health:%d %s", computeHealthScore(), statusBuffer);
    } else if (setupApActive_ && WiFi.status() != WL_CONNECTED) {
      display_.printf("WiFi Setup\n");
      display_.printf("AP:%s\n", setupApSsid_);
      display_.printf("PW:%s\n", setupApPassword_);
      display_.printf("Open:%s\n", kSetupApIp);
      display_.printf("Host:%s\n", wifiConfig_.hostname);
      display_.printf("Mode:%s\n", modeLabel());
      display_.printf("FS:%s", filesystemReady_ ? "READY" : "OFF");
    } else {
      display_.printf("FW:%s\n", GreenhouseVersion::kFirmwareVersion);
      display_.printf("Diag:%s\n", statusBuffer);
      display_.printf("Host:%s\n", wifiConfig_.hostname);
      display_.printf("Power:%s\n", batteryBandLabel(battery_));
      display_.printf("Raw:%lumV\n", static_cast<unsigned long>(battery_.rawMilliVolts));
      display_.printf("%s\n", loraBuffer);
      display_.printf("ACK T/O:%lu\n", static_cast<unsigned long>(loraLink_.stats().ackTimeouts));
      display_.printf("Cmd:%s\n", commandBuffer);
      display_.printf("RSSI:%d SNR:%.1f", loraLink_.stats().lastRssi, static_cast<double>(loraLink_.stats().lastSnr));
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
    file.println("millis,mode,controller_state,safe_mode,safe_reason,reset_reason,boot_failures,air_available,air_temp_c,humidity_pct,water_available,water_temp_c,light_available,light_lux,vpd_kpa,dew_point_c,frost_risk,crop_status,battery_available,battery_raw_mv,battery_v,battery_pct,battery_band,health_score,top_open,bottom_open,fan_exhaust,fan_intake,defogger,grow_light,circulation,lora_queue,lora_sent,lora_acked,lora_ack_timeouts,lora_dropped,lora_queue_warns,lora_max_queue,air_faults,bme_errors,dht_errors,light_errors,water_errors,servo_blocked,servo_last_move_ms,servo_longest_move_ms,diag_last_code,diag_last_detail");
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
    const RecentFaultEntry latestFault = latestRecentFault();
    if (file.size() == 0) {
      file.println("millis,boot_count,boot_failures,reset_reason,safe_mode,safe_reason,diag_last_code,diag_last_detail");
    }
    file.printf("%lu,%lu,%u,%s,%u,%s,%s,%ld\n",
                millis(),
                static_cast<unsigned long>(bootCount_),
                static_cast<unsigned>(bootFailures_),
                resetReasonLabel(lastResetReason_),
                safeMode_,
                safeModeReasonLabel(safeModeReason_),
                GreenhouseRuntime::diagnosticCodeLabel(latestFault.code),
                static_cast<long>(latestFault.detail));
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
    const RecentFaultEntry latestFault = latestRecentFault();
    file.printf("%lu,%s,%s,%u,%s,%s,%u,%u,%.2f,%.2f,%u,%.2f,%u,%.2f,%.2f,%.2f,%u,%s,%u,%lu,%.2f,%d,%s,%d,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s,%ld\n",
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
                static_cast<unsigned long>(battery_.rawMilliVolts),
                battery_.voltageV,
                battery_.percent,
                batteryBandLabel(battery_),
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
                static_cast<unsigned long>(loraLink_.stats().acknowledgedFrames),
                static_cast<unsigned long>(loraLink_.stats().ackTimeouts),
                static_cast<unsigned long>(loraLink_.stats().droppedFrames),
                static_cast<unsigned long>(loraLink_.stats().queueWarningCount),
                static_cast<unsigned long>(loraLink_.stats().maxQueueDepthObserved),
                static_cast<unsigned long>(sensorFaults_.airReadFailures),
                static_cast<unsigned long>(sensorFaults_.bmeReadFailures),
                static_cast<unsigned long>(sensorFaults_.dhtReadFailures),
                static_cast<unsigned long>(sensorFaults_.lightReadFailures),
                static_cast<unsigned long>(sensorFaults_.waterReadFailures),
                static_cast<unsigned long>(servoProtection_.commandBlockedCount),
                static_cast<unsigned long>(servoProtection_.lastMoveDurationMs),
                static_cast<unsigned long>(servoProtection_.longestMoveDurationMs),
                GreenhouseRuntime::diagnosticCodeLabel(latestFault.code),
                static_cast<long>(latestFault.detail));
    file.close();
  }

  void appendCommandAudit(const char *topic, const char *payload, const CommandAudit &audit) {
    if (!filesystemReady_) {
      return;
    }
    File file = LittleFS.open(kCommandLogPath, FILE_APPEND);
    if (!file) {
      return;
    }
    if (file.size() == 0) {
      file.println("millis,topic,payload,requested,status,applied,safe_mode");
    }
    file.printf("%lu,%s,%s,%s,%s,%s,%u\n",
                static_cast<unsigned long>(audit.receivedAtMs),
                topic != nullptr ? topic : "",
                payload != nullptr ? payload : "",
                GreenhouseRemote::modeCommandLabel(audit.requested),
                GreenhouseRemote::commandStatusLabel(audit.status),
                GreenhouseRemote::controlModeLabel(audit.applied),
                safeMode_ ? 1U : 0U);
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
    if (!safeMode_ || safeModeReason_ != reason) {
      recordDiagnosticEvent(DiagnosticCode::safeModeEntered, static_cast<int32_t>(reason));
    }
    safeMode_ = true;
    manualOverrideActive_ = false;
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

  void recordDiagnosticEvent(DiagnosticCode code, int32_t detail) {
    if (code == DiagnosticCode::none) {
      return;
    }

    const uint8_t slot = recentFaultCount_ < kRecentFaultCapacity
                             ? static_cast<uint8_t>((recentFaultHead_ + recentFaultCount_) % kRecentFaultCapacity)
                             : recentFaultHead_;
    if (recentFaultCount_ < kRecentFaultCapacity) {
      ++recentFaultCount_;
    } else {
      recentFaultHead_ = static_cast<uint8_t>((recentFaultHead_ + 1U) % kRecentFaultCapacity);
    }

    recentFaults_[slot].code = code;
    recentFaults_[slot].atMs = millis();
    recentFaults_[slot].detail = detail;
    recentFaults_[slot].bootCount = bootCount_;
    persistRecentFaultState(slot);
  }

  RecentFaultEntry latestRecentFault() const {
    if (recentFaultCount_ == 0U) {
      return RecentFaultEntry{};
    }
    const uint8_t latestIndex = static_cast<uint8_t>((recentFaultHead_ + recentFaultCount_ - 1U) % kRecentFaultCapacity);
    return recentFaults_[latestIndex];
  }

  void persistRecentFaultState(uint8_t slot) {
    char key[16];
    preferences_.putUChar("fault_head", recentFaultHead_);
    preferences_.putUChar("fault_cnt", recentFaultCount_);

    snprintf(key, sizeof(key), "fault%u_cd", static_cast<unsigned>(slot));
    preferences_.putUChar(key, static_cast<uint8_t>(recentFaults_[slot].code));
    snprintf(key, sizeof(key), "fault%u_ms", static_cast<unsigned>(slot));
    preferences_.putUInt(key, recentFaults_[slot].atMs);
    snprintf(key, sizeof(key), "fault%u_dt", static_cast<unsigned>(slot));
    preferences_.putInt(key, recentFaults_[slot].detail);
    snprintf(key, sizeof(key), "fault%u_bt", static_cast<unsigned>(slot));
    preferences_.putUInt(key, recentFaults_[slot].bootCount);
  }

  void loadRecentFaultState() {
    recentFaultHead_ = preferences_.getUChar("fault_head", 0U);
    recentFaultCount_ = preferences_.getUChar("fault_cnt", 0U);
    if (recentFaultHead_ >= kRecentFaultCapacity) {
      recentFaultHead_ = 0U;
    }
    if (recentFaultCount_ > kRecentFaultCapacity) {
      recentFaultCount_ = kRecentFaultCapacity;
    }

    char key[16];
    for (uint8_t slot = 0; slot < kRecentFaultCapacity; ++slot) {
      snprintf(key, sizeof(key), "fault%u_cd", static_cast<unsigned>(slot));
      recentFaults_[slot].code = static_cast<DiagnosticCode>(preferences_.getUChar(key, static_cast<uint8_t>(DiagnosticCode::none)));
      snprintf(key, sizeof(key), "fault%u_ms", static_cast<unsigned>(slot));
      recentFaults_[slot].atMs = preferences_.getUInt(key, 0U);
      snprintf(key, sizeof(key), "fault%u_dt", static_cast<unsigned>(slot));
      recentFaults_[slot].detail = preferences_.getInt(key, 0);
      snprintf(key, sizeof(key), "fault%u_bt", static_cast<unsigned>(slot));
      recentFaults_[slot].bootCount = preferences_.getUInt(key, 0U);
    }
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
    return GreenhouseHal::readBatteryState();
  }

  int computeHealthScore() const {
    int score = 100;
    if (!snapshot_.airAvailable) {
      score -= 25;
    }
    if (sensorFaults_.lightReadFailures > 0U) {
      score -= 5;
    }
    if (sensorFaults_.waterReadFailures > 0U) {
      score -= 5;
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
    if (loraLink_.stats().queueWarningActive) {
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
    if (Settings::MQTT.enableInboundModeCommands) {
      char commandTopic[GreenhouseRemote::kTopicSize];
      GreenhouseRemote::buildModeCommandTopic(commandTopic, sizeof(commandTopic), Settings::MQTT.baseTopic);
      mqttClient_.subscribe(commandTopic);
    }
    publishHomeAssistantDiscovery();
    publishModeCommandState();
    publishModeCommandResult();
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
    publishHomeAssistantSensorDiscovery("sensor", "battery_voltage", "Battery Voltage", "{{ value_json.battery.voltage_v }}", "voltage", "measurement", "V");
    publishHomeAssistantSensorDiscovery("sensor", "lora_queue_depth", "LoRa Queue Depth", "{{ value_json.lora.queue_depth }}", "", "measurement", "frames");
    publishHomeAssistantSensorDiscovery("sensor", "lora_acknowledged", "LoRa Acknowledged", "{{ value_json.lora.acknowledged }}", "", "measurement", "frames");
    publishHomeAssistantSensorDiscovery("sensor", "lora_ack_timeouts", "LoRa ACK Timeouts", "{{ value_json.lora.ack_timeouts }}", "", "measurement", "frames");
    publishHomeAssistantSensorDiscovery("binary_sensor", "safe_mode_active", "Safe Mode Active", "{{ value_json.safe_mode.active }}", "problem", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "battery_low", "Battery Low", "{{ value_json.battery.low }}", "battery", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "battery_critical", "Battery Critical", "{{ value_json.battery.critical }}", "battery", "", "");
    publishHomeAssistantSensorDiscovery("binary_sensor", "lora_ready", "LoRa Ready", "{{ value_json.connectivity.lora }}", "connectivity", "", "");
    publishHomeAssistantModeSelectDiscovery();
    discoveryPublished_ = true;
  }

  void publishHomeAssistantModeSelectDiscovery() {
    if (!Settings::MQTT.enableInboundModeCommands) {
      return;
    }

    char topic[192];
    char payload[1024];
    char commandTopic[GreenhouseRemote::kTopicSize];
    char stateTopic[GreenhouseRemote::kTopicSize];
    char availabilityTopic[96];

    GreenhouseRemote::buildModeCommandTopic(commandTopic, sizeof(commandTopic), Settings::MQTT.baseTopic);
    GreenhouseRemote::buildModeStateTopic(stateTopic, sizeof(stateTopic), Settings::MQTT.baseTopic);
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability", Settings::MQTT.baseTopic);
    snprintf(topic,
             sizeof(topic),
             "%s/select/%s_mode_control/config",
             Settings::MQTT.discoveryPrefix,
             Settings::MQTT.clientId);
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Control Mode\",\"uniq_id\":\"%s_mode_control\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"avty_t\":\"%s\",\"options\":[\"AUTO\",\"OPEN\",\"CLOSED\"],\"dev\":{\"ids\":[\"%s\"],\"name\":\"Mini Greenhouse\",\"mdl\":\"ESP32-S3 Greenhouse Controller\",\"mf\":\"Solar LoRa Greenhouse Automation\"}}",
             Settings::MQTT.clientId,
             commandTopic,
             stateTopic,
             availabilityTopic,
             Settings::MQTT.clientId);
    mqttClient_.publish(topic, payload, true);
  }

  void publishModeCommandState() {
    if (!mqttClient_.connected() || !Settings::MQTT.enableInboundModeCommands) {
      return;
    }

    char topic[GreenhouseRemote::kTopicSize];
    GreenhouseRemote::buildModeStateTopic(topic, sizeof(topic), Settings::MQTT.baseTopic);
    mqttClient_.publish(topic, modeLabel(), true);
  }

  void publishModeCommandResult() {
    if (!mqttClient_.connected() || !Settings::MQTT.enableInboundModeCommands || !lastCommandAudit_.available) {
      return;
    }

    char topic[GreenhouseRemote::kTopicSize];
    char payload[256];
    GreenhouseRemote::buildModeResultTopic(topic, sizeof(topic), Settings::MQTT.baseTopic);
    snprintf(payload,
             sizeof(payload),
             "{\"requested\":\"%s\",\"status\":\"%s\",\"applied\":\"%s\",\"received_at_ms\":%lu}",
             GreenhouseRemote::modeCommandLabel(lastCommandAudit_.requested),
             GreenhouseRemote::commandStatusLabel(lastCommandAudit_.status),
             GreenhouseRemote::controlModeLabel(lastCommandAudit_.applied),
             static_cast<unsigned long>(lastCommandAudit_.receivedAtMs));
    mqttClient_.publish(topic, payload, true);
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

  float finiteOrDefault(float value, float fallback = 0.0F) const {
    return isnan(value) || isinf(value) ? fallback : value;
  }

  void buildTelemetryPayload(char *payload, size_t payloadSize) {
    const CropProfile &profile = GreenhouseLogic::cropProfile(Settings::CROP.profile);
    const RecentFaultEntry latestFault = latestRecentFault();
    snprintf(payload,
             payloadSize,
             "{\"mode\":\"%s\",\"controller_state\":\"%s\",\"safe_mode\":{\"active\":%s,\"reason\":\"%s\",\"boot_failures\":%u},\"reset_reason\":\"%s\",\"crop\":{\"profile\":\"%s\",\"status\":\"%s\"},\"air\":{\"available\":%s,\"age_ms\":%lu,\"temp_c\":%.2f,\"humidity_pct\":%.2f},\"water\":{\"available\":%s,\"age_ms\":%lu,\"temp_c\":%.2f},\"light\":{\"available\":%s,\"age_ms\":%lu,\"lux\":%.2f},\"metrics\":{\"vpd_kpa\":%.2f,\"dew_point_c\":%.2f,\"frost_risk\":%s},\"battery\":{\"available\":%s,\"calibrated\":%s,\"sensor\":\"VBAT_Read\",\"adc_ctrl_pin\":%d,\"raw_mv\":%lu,\"voltage_v\":%.2f,\"percent\":%d,\"band\":\"%s\",\"low\":%s,\"critical\":%s},\"health\":{\"score\":%d},\"diagnostics\":{\"recent_fault_count\":%u,\"recent_fault\":{\"code\":\"%s\",\"detail\":%ld,\"at_ms\":%lu,\"boot_count\":%lu},\"sensor_errors\":{\"air\":%lu,\"bme\":%lu,\"dht\":%lu,\"light\":%lu,\"water\":%lu},\"servo\":{\"blocked_commands\":%lu,\"last_move_ms\":%lu,\"longest_move_ms\":%lu}},\"power_budget\":{\"servo_moves\":%s,\"vent_fans\":%s,\"defogger\":%s,\"grow_light\":%s,\"circulation\":%s},\"manual_override\":{\"active\":%s},\"actuators\":{\"top_open\":%s,\"bottom_open\":%s,\"fan_exhaust\":%s,\"fan_intake\":%s,\"defogger\":%s,\"grow_light\":%s,\"circulation\":%s},\"connectivity\":{\"wifi\":%s,\"mqtt\":%s,\"ota\":%s,\"lora\":%s},\"storage\":{\"filesystem_ready\":%s},\"commands\":{\"mode\":{\"enabled\":%s,\"requested\":\"%s\",\"status\":\"%s\",\"applied\":\"%s\",\"received_at_ms\":%lu}},\"lora\":{\"session_id\":%lu,\"queue_depth\":%u,\"queue_warning_active\":%s,\"queue_warnings\":%lu,\"max_queue_depth\":%lu,\"last_queue_warning_at_ms\":%lu,\"sent\":%lu,\"acknowledged\":%lu,\"ack_timeouts\":%lu,\"dropped\":%lu,\"last_dropped_sequence\":%lu,\"last_dropped_at_ms\":%lu,\"duplicate_inbound\":%lu,\"invalid_inbound\":%lu,\"last_rssi_dbm\":%d,\"last_snr_db\":%.2f,\"last_error_code\":%d},\"uptime_ms\":%lu}",
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
             finiteOrDefault(snapshot_.airTempC),
             finiteOrDefault(snapshot_.humidityPct),
             snapshot_.waterAvailable ? "true" : "false",
             static_cast<unsigned long>(snapshot_.waterAgeMs),
             finiteOrDefault(snapshot_.waterTempC),
             snapshot_.lightAvailable ? "true" : "false",
             static_cast<unsigned long>(snapshot_.lightAgeMs),
             finiteOrDefault(snapshot_.lightLux),
             metrics_.vpdAvailable ? finiteOrDefault(metrics_.vaporPressureDeficitKPa) : 0.0F,
             metrics_.dewPointAvailable ? finiteOrDefault(metrics_.dewPointC) : 0.0F,
             metrics_.frostRisk ? "true" : "false",
             battery_.available ? "true" : "false",
             battery_.calibrated ? "true" : "false",
             static_cast<int>(PinMap::BATTERY_ADC_CTRL),
             static_cast<unsigned long>(battery_.rawMilliVolts),
             finiteOrDefault(battery_.voltageV),
             battery_.percent,
             batteryBandLabel(battery_),
             battery_.low ? "true" : "false",
             battery_.critical ? "true" : "false",
             computeHealthScore(),
             static_cast<unsigned>(recentFaultCount_),
             GreenhouseRuntime::diagnosticCodeLabel(latestFault.code),
             static_cast<long>(latestFault.detail),
             static_cast<unsigned long>(latestFault.atMs),
             static_cast<unsigned long>(latestFault.bootCount),
             static_cast<unsigned long>(sensorFaults_.airReadFailures),
             static_cast<unsigned long>(sensorFaults_.bmeReadFailures),
             static_cast<unsigned long>(sensorFaults_.dhtReadFailures),
             static_cast<unsigned long>(sensorFaults_.lightReadFailures),
             static_cast<unsigned long>(sensorFaults_.waterReadFailures),
             static_cast<unsigned long>(servoProtection_.commandBlockedCount),
             static_cast<unsigned long>(servoProtection_.lastMoveDurationMs),
             static_cast<unsigned long>(servoProtection_.longestMoveDurationMs),
             powerBudget_.allowServoMoves ? "true" : "false",
             powerBudget_.allowVentFans ? "true" : "false",
             powerBudget_.allowDefogger ? "true" : "false",
             powerBudget_.allowGrowLight ? "true" : "false",
             powerBudget_.allowCirculation ? "true" : "false",
             manualOverrideActive_ ? "true" : "false",
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
             Settings::MQTT.enableInboundModeCommands ? "true" : "false",
             GreenhouseRemote::modeCommandLabel(lastCommandAudit_.requested),
             GreenhouseRemote::commandStatusLabel(lastCommandAudit_.status),
             GreenhouseRemote::controlModeLabel(lastCommandAudit_.applied),
             static_cast<unsigned long>(lastCommandAudit_.receivedAtMs),
             static_cast<unsigned long>(loraLink_.stats().sessionId),
             static_cast<unsigned>(loraLink_.stats().queueDepth),
             loraLink_.stats().queueWarningActive ? "true" : "false",
             static_cast<unsigned long>(loraLink_.stats().queueWarningCount),
             static_cast<unsigned long>(loraLink_.stats().maxQueueDepthObserved),
             static_cast<unsigned long>(loraLink_.stats().lastQueueWarningAtMs),
             static_cast<unsigned long>(loraLink_.stats().sentFrames),
             static_cast<unsigned long>(loraLink_.stats().acknowledgedFrames),
             static_cast<unsigned long>(loraLink_.stats().ackTimeouts),
             static_cast<unsigned long>(loraLink_.stats().droppedFrames),
             static_cast<unsigned long>(loraLink_.stats().lastDroppedSequence),
             static_cast<unsigned long>(loraLink_.stats().lastDroppedAtMs),
             static_cast<unsigned long>(loraLink_.stats().duplicateInboundFrames),
             static_cast<unsigned long>(loraLink_.stats().invalidInboundFrames),
             loraLink_.stats().lastRssi,
             static_cast<double>(loraLink_.stats().lastSnr),
             static_cast<int>(loraLink_.stats().lastErrorCode),
             static_cast<unsigned long>(millis()));
  }

  void publishTelemetry(uint32_t now) {
    char payload[2200];
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
    if (loraLink_.stats().queueWarningCount > recordedQueueWarningCount_) {
      recordDiagnosticEvent(DiagnosticCode::loraQueueWarning, static_cast<int32_t>(loraLink_.stats().queueDepth));
      recordedQueueWarningCount_ = loraLink_.stats().queueWarningCount;
    }
    if (loraLink_.stats().droppedFrames > recordedDroppedFrames_) {
      recordDiagnosticEvent(DiagnosticCode::loraFrameDropped,
                            static_cast<int32_t>(loraLink_.stats().lastDroppedSequence));
      recordedDroppedFrames_ = loraLink_.stats().droppedFrames;
    }
  }

  Preferences preferences_;
  Adafruit_BME280 bme_;
  DHT dht_{PinMap::TEMP_AIR_DHT, DHT22};
  BH1750 lightSensor_;
  OneWire oneWire_{PinMap::TEMP_WATER};
  DallasTemperature waterSensor_{&oneWire_};
  Adafruit_SSD1306 display_{Settings::DISPLAY_WIDTH, Settings::DISPLAY_HEIGHT, &Wire, -1};
  DNSServer dnsServer_;
  WebServer webServer_{kHttpPort};
  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  GreenhouseLoRa::SX1262Transport loraTransport_{Settings::LORA.frequencyMHz,
                                                 Settings::LORA.bandwidthKHz,
                                                 Settings::LORA.spreadingFactor,
                                                 Settings::LORA.codingRate,
                                                 Settings::LORA.syncWord,
                                                 Settings::LORA.outputPowerDbm,
                                                 Settings::LORA.preambleLength,
                                                 Settings::LORA.ackTimeoutMs};
  GreenhouseLoRa::LinkService loraLink_{loraTransport_};
  LedcServo topServo_{0};
  LedcServo bottomServo_{1};
  Button modeButton_{PinMap::BUTTON_MODE};
  Button forceOpenButton_{PinMap::BUTTON_FORCE_OPEN};
  Button forceCloseButton_{PinMap::BUTTON_FORCE_CLOSE};

  SensorSnapshot snapshot_;
  SensorCacheState sensorCache_{};
  GreenhouseMetrics metrics_;
  BatteryState battery_;
  ActuatorState actuators_;
  ActuatorState effectiveActuators_;
  ActuatorState manualActuators_;
  ClimateConfig runtimeClimate_ = Settings::CLIMATE;
  ControlMode mode_ = ControlMode::automatic;
  ControllerState controllerState_ = ControllerState::automatic;
  PowerBudget powerBudget_{};

  SensorHardwareState sensorHardware_{};
  bool displayReady_ = false;
  bool displaySleeping_ = false;
  bool filesystemReady_ = false;
  bool otaReady_ = false;
  bool firmwareUploadSucceeded_ = false;
  bool mqttEnabled_ = false;
  bool webServerStarted_ = false;
  bool setupApActive_ = false;
  bool wifiConnected_ = false;
  bool mdnsActive_ = false;
  bool discoveryPublished_ = false;
  bool safeMode_ = false;
  bool manualOverrideActive_ = false;
  bool climateOverrideActive_ = false;
  bool bootHealthy_ = false;
  bool rehomeAfterUnsafeReset_ = false;
  CommandAudit lastCommandAudit_{};
  SafeModeReason safeModeReason_ = SafeModeReason::none;
  ConfigFault configFault_ = ConfigFault::none;
  esp_reset_reason_t lastResetReason_ = ESP_RST_UNKNOWN;
  uint32_t bootCount_ = 0;
  uint8_t bootFailures_ = 0;
  SensorFaultState sensorFaults_{};
  ServoProtectionState servoProtection_{};
  RecentFaultEntry recentFaults_[kRecentFaultCapacity]{};
  uint8_t recentFaultHead_ = 0;
  uint8_t recentFaultCount_ = 0;
  uint32_t recordedLightFailures_ = 0;
  uint32_t recordedWaterFailures_ = 0;
  uint32_t recordedQueueWarningCount_ = 0;
  uint32_t recordedDroppedFrames_ = 0;
  uint32_t lastServoBlockedEventAt_ = 0;

  uint32_t lastControlAt_ = 0;
  uint32_t lastDisplayAt_ = 0;
  uint32_t lastDisplayActivityAt_ = 0;
  uint32_t lastScreenSaverRefreshAt_ = 0;
  uint32_t nextScreenSaverModeSwitchAt_ = 0;
  uint32_t lastLogAt_ = 0;
  uint32_t lastOtaAt_ = 0;
  uint32_t lastWifiConnectAttemptAt_ = 0;
  uint32_t lastMqttConnectAttemptAt_ = 0;
  uint32_t lastTelemetryPublishAt_ = 0;
  uint32_t lastProgressAt_ = 0;
  uint32_t lastWifiScanStartedAt_ = 0;
  uint32_t lastWifiScanCompletedAt_ = 0;
  uint8_t wifiReconnectFailures_ = 0;
  bool wifiScanInProgress_ = false;
  bool wifiScanRequested_ = false;
  size_t wifiScanResultCount_ = 0U;
  WifiRuntimeConfig wifiConfig_{};
  WifiScanEntry wifiScanEntries_[kWifiScanResultCapacity]{};
  ScreenSaverBlade screenSaverGrass_[kScreenSaverGrassCount]{};
  ScreenSaverParticle screenSaverParticles_[kScreenSaverParticleCount]{};
  ScreenSaverPlant screenSaverPlant_{};
  ScreenSaverMode screenSaverMode_ = ScreenSaverMode::night;
  uint32_t screenSaverFrame_ = 0;
  int8_t screenSaverDriftX_ = 0;
  int8_t screenSaverDriftY_ = 0;
  String wifiScanResultsJson_ = "[]";
  char setupApSsid_[20]{};
  char setupApPassword_[16]{};
  char wifiScanError_[48]{};
  char firmwareUploadStatus_[96] = "Firmware upload did not complete.";

  static GreenhouseController *activeInstance_;
};

GreenhouseController *GreenhouseController::activeInstance_ = nullptr;

GreenhouseController controller;

}  // namespace

void setup() {
  controller.begin();
}

void loop() {
  controller.update();
}
