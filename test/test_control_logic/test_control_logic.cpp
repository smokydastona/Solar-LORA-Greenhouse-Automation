#include <unity.h>

#include "ControlLogic.h"
#include "ControlLogicDefaults.h"
#include "ControllerRuntime.h"
#include "LoRaLink.h"
#include "RemoteControl.h"

namespace {

GreenhouseLogic::ClimateConfig climateConfig() {
  return GreenhouseDefaults::CLIMATE;
}

GreenhouseLogic::SystemConfig systemConfig(bool defogger = true, bool growLight = true, bool circulation = true) {
  return GreenhouseLogic::SystemConfig{defogger, growLight, circulation};
}

GreenhouseLogic::EvaluationContext baseContext() {
  GreenhouseLogic::EvaluationContext ctx{};
  ctx.mode = GreenhouseLogic::ControlMode::automatic;
  ctx.climate = climateConfig();
  ctx.system = systemConfig();
  ctx.daylight = true;
  ctx.hasTime = true;
  ctx.currentMinute = 9U * 60U;
  ctx.snapshot.airAvailable = true;
  ctx.snapshot.airTempC = 30.0F;
  ctx.snapshot.humidityPct = 60.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 15000.0F;
  return ctx;
}

class FakeLoRaTransport : public GreenhouseLoRa::ILoRaTransport {
 public:
  bool beginResult = true;
  bool readyState = true;
  bool sendResult = true;
  uint32_t sendCount = 0;
  uint32_t lastSessionId = 0;
  uint32_t lastSequence = 0;
  uint32_t lastCrc32 = 0;

  bool begin() override {
    return beginResult;
  }

  bool ready() const override {
    return readyState;
  }

  bool send(const GreenhouseLoRa::TelemetryFrame &frame, GreenhouseLoRa::LinkStats &) override {
    ++sendCount;
    lastSessionId = frame.sessionId;
    lastSequence = frame.sequence;
    lastCrc32 = frame.crc32;
    return sendResult;
  }

  void poll(GreenhouseLoRa::LinkStats &stats) override {
    stats.backendConfigured = true;
    stats.backendReady = readyState;
  }
};

void test_mode_cycle_wraps_in_expected_order() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::ControlMode::forceOpen),
                          static_cast<uint8_t>(GreenhouseLogic::nextMode(GreenhouseLogic::ControlMode::automatic)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::ControlMode::forceClosed),
                          static_cast<uint8_t>(GreenhouseLogic::nextMode(GreenhouseLogic::ControlMode::forceOpen)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::ControlMode::automatic),
                          static_cast<uint8_t>(GreenhouseLogic::nextMode(GreenhouseLogic::ControlMode::forceClosed)));
}

void test_daylight_resolver_uses_light_sensor_when_available() {
  auto ctx = baseContext();
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 15000.0F;
  ctx.currentMinute = 23U * 60U;

  TEST_ASSERT_TRUE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, true, ctx.currentMinute));

  ctx.snapshot.lightLux = 1000.0F;

  TEST_ASSERT_FALSE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, true, ctx.currentMinute));
}

void test_daylight_resolver_falls_back_to_schedule_when_light_sensor_is_unavailable() {
  auto ctx = baseContext();
  ctx.snapshot.lightAvailable = false;
  ctx.currentMinute = 9U * 60U;

  TEST_ASSERT_TRUE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, true, ctx.currentMinute));

  ctx.currentMinute = 20U * 60U;

  TEST_ASSERT_FALSE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, true, ctx.currentMinute));
}

void test_daylight_resolver_defaults_to_night_when_no_light_sensor_or_time_is_available() {
  auto ctx = baseContext();
  ctx.snapshot.lightAvailable = false;

  TEST_ASSERT_FALSE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, false, 0U));
}

void test_force_open_turns_on_venting_and_disables_heat_and_light() {
  auto ctx = baseContext();
  ctx.mode = GreenhouseLogic::ControlMode::forceOpen;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.topVentOpen);
  TEST_ASSERT_TRUE(actual.bottomVentOpen);
  TEST_ASSERT_TRUE(actual.exhaustFanOn);
  TEST_ASSERT_TRUE(actual.intakeFanOn);
  TEST_ASSERT_TRUE(actual.circulationFansOn);
  TEST_ASSERT_FALSE(actual.defoggerOn);
  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_force_closed_turns_everything_off() {
  auto ctx = baseContext();
  ctx.mode = GreenhouseLogic::ControlMode::forceClosed;
  ctx.previousActuators = GreenhouseLogic::ActuatorState{true, true, true, true, true, true, true};

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.topVentOpen);
  TEST_ASSERT_FALSE(actual.bottomVentOpen);
  TEST_ASSERT_FALSE(actual.exhaustFanOn);
  TEST_ASSERT_FALSE(actual.intakeFanOn);
  TEST_ASSERT_FALSE(actual.circulationFansOn);
  TEST_ASSERT_FALSE(actual.defoggerOn);
  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_vents_stay_open_between_open_and_close_thresholds_when_previously_open() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 25.0F;
  ctx.snapshot.humidityPct = 60.0F;
  ctx.previousActuators.topVentOpen = true;
  ctx.previousActuators.bottomVentOpen = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.topVentOpen);
  TEST_ASSERT_TRUE(actual.bottomVentOpen);
}

void test_vents_stay_closed_between_open_and_close_thresholds_when_previously_closed() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 25.0F;
  ctx.snapshot.humidityPct = 60.0F;
  ctx.previousActuators.topVentOpen = false;
  ctx.previousActuators.bottomVentOpen = false;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.topVentOpen);
  TEST_ASSERT_FALSE(actual.bottomVentOpen);
}

void test_fans_stay_on_between_fan_on_and_fan_off_thresholds_when_previously_on() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 26.0F;
  ctx.snapshot.humidityPct = 76.0F;
  ctx.previousActuators.exhaustFanOn = true;
  ctx.previousActuators.intakeFanOn = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.exhaustFanOn);
  TEST_ASSERT_TRUE(actual.intakeFanOn);
}

void test_fans_stay_off_between_fan_on_and_fan_off_thresholds_when_previously_off() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 26.0F;
  ctx.snapshot.humidityPct = 76.0F;
  ctx.previousActuators.exhaustFanOn = false;
  ctx.previousActuators.intakeFanOn = false;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.exhaustFanOn);
  TEST_ASSERT_FALSE(actual.intakeFanOn);
}

void test_vent_fans_turn_off_at_night_even_when_heat_or_humidity_is_high() {
  auto ctx = baseContext();
  ctx.daylight = false;
  ctx.snapshot.airTempC = 31.0F;
  ctx.snapshot.humidityPct = 90.0F;
  ctx.previousActuators.exhaustFanOn = true;
  ctx.previousActuators.intakeFanOn = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.exhaustFanOn);
  TEST_ASSERT_FALSE(actual.intakeFanOn);
}

void test_vent_fans_turn_back_on_in_daylight_when_thresholds_remain_exceeded() {
  auto ctx = baseContext();
  ctx.daylight = true;
  ctx.snapshot.airTempC = 31.0F;
  ctx.snapshot.humidityPct = 90.0F;
  ctx.previousActuators.exhaustFanOn = false;
  ctx.previousActuators.intakeFanOn = false;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.exhaustFanOn);
  TEST_ASSERT_TRUE(actual.intakeFanOn);
}

void test_circulation_fans_turn_on_during_daytime_mixing_conditions() {
  auto ctx = baseContext();
  ctx.daylight = true;
  ctx.snapshot.airTempC = 24.0F;
  ctx.snapshot.humidityPct = 75.0F;
  ctx.previousActuators.circulationFansOn = false;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.circulationFansOn);
}

void test_circulation_fans_turn_off_at_night_even_when_previously_on() {
  auto ctx = baseContext();
  ctx.daylight = false;
  ctx.snapshot.airTempC = 24.0F;
  ctx.snapshot.humidityPct = 75.0F;
  ctx.previousActuators.circulationFansOn = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.circulationFansOn);
}

void test_sensor_failure_daytime_opens_vents_and_turns_off_climate_outputs() {
  auto ctx = baseContext();
  ctx.snapshot.airAvailable = false;
  ctx.daylight = true;
  ctx.system = systemConfig(false, false, true);
  ctx.previousActuators = GreenhouseLogic::ActuatorState{false, false, true, true, true, false, true};

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.topVentOpen);
  TEST_ASSERT_TRUE(actual.bottomVentOpen);
  TEST_ASSERT_FALSE(actual.exhaustFanOn);
  TEST_ASSERT_FALSE(actual.intakeFanOn);
  TEST_ASSERT_FALSE(actual.defoggerOn);
  TEST_ASSERT_FALSE(actual.circulationFansOn);
  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_sensor_failure_night_closes_vents_and_turns_off_climate_outputs() {
  auto ctx = baseContext();
  ctx.snapshot.airAvailable = false;
  ctx.daylight = false;
  ctx.system = systemConfig(false, false, true);
  ctx.previousActuators = GreenhouseLogic::ActuatorState{true, true, true, true, true, false, true};

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.topVentOpen);
  TEST_ASSERT_FALSE(actual.bottomVentOpen);
  TEST_ASSERT_FALSE(actual.exhaustFanOn);
  TEST_ASSERT_FALSE(actual.intakeFanOn);
  TEST_ASSERT_FALSE(actual.defoggerOn);
  TEST_ASSERT_FALSE(actual.circulationFansOn);
  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_grow_light_uses_schedule_and_lux_threshold() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 20.0F;
  ctx.snapshot.humidityPct = 50.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 2000.0F;
  ctx.currentMinute = 9U * 60U;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.growLightOn);
}

void test_defogger_turns_on_only_during_cold_night_conditions() {
  auto ctx = baseContext();
  ctx.daylight = false;
  ctx.snapshot.airTempC = 4.0F;
  ctx.snapshot.humidityPct = 65.0F;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_TRUE(actual.defoggerOn);
}

void test_defogger_stays_off_during_day_even_if_air_is_cold() {
  auto ctx = baseContext();
  ctx.daylight = true;
  ctx.snapshot.airTempC = 2.0F;
  ctx.previousActuators.defoggerOn = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.defoggerOn);
}

void test_defogger_uses_off_threshold_to_drop_out_on_warmer_night() {
  auto ctx = baseContext();
  ctx.daylight = false;
  ctx.snapshot.airTempC = 8.5F;
  ctx.previousActuators.defoggerOn = true;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.defoggerOn);
}

void test_grow_light_stays_off_at_exact_lux_threshold() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 20.0F;
  ctx.snapshot.humidityPct = 50.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = ctx.climate.growLightLuxThreshold;
  ctx.currentMinute = 9U * 60U;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_grow_light_stays_off_outside_lighting_window_even_when_dark() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 20.0F;
  ctx.snapshot.humidityPct = 50.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 1000.0F;
  ctx.currentMinute = ctx.climate.growLightStopMinutes;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_grow_light_stays_off_without_time_even_when_dark() {
  auto ctx = baseContext();
  ctx.snapshot.airTempC = 20.0F;
  ctx.snapshot.humidityPct = 50.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 1000.0F;
  ctx.hasTime = false;
  ctx.currentMinute = 0U;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.growLightOn);
}

void test_optional_outputs_stay_off_when_disabled_in_system_config() {
  auto ctx = baseContext();
  ctx.daylight = false;
  ctx.system = systemConfig(false, false, false);
  ctx.snapshot.airTempC = 4.0F;
  ctx.snapshot.humidityPct = 90.0F;
  ctx.snapshot.lightAvailable = true;
  ctx.snapshot.lightLux = 1000.0F;

  const auto actual = GreenhouseLogic::evaluateActuators(ctx);

  TEST_ASSERT_FALSE(actual.defoggerOn);
  TEST_ASSERT_FALSE(actual.growLightOn);
  TEST_ASSERT_FALSE(actual.circulationFansOn);
}

void test_controller_state_resolves_low_power_when_battery_is_low() {
  const auto state = GreenhouseRuntime::resolveControllerState(false,
                                                               true,
                                                               true,
                                                               false,
                                                               GreenhouseLogic::ControlMode::automatic);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRuntime::ControllerState::lowPower),
                          static_cast<uint8_t>(state));
}

void test_power_budget_sheds_noncritical_loads_at_low_battery() {
  const auto budget = GreenhouseRuntime::evaluatePowerBudget(true, true, false);

  TEST_ASSERT_TRUE(budget.allowServoMoves);
  TEST_ASSERT_TRUE(budget.allowVentFans);
  TEST_ASSERT_FALSE(budget.allowGrowLight);
  TEST_ASSERT_FALSE(budget.allowCirculation);
}

void test_power_budget_sheds_all_controlled_loads_at_critical_battery() {
  const auto budget = GreenhouseRuntime::evaluatePowerBudget(true, true, true);

  TEST_ASSERT_FALSE(budget.allowServoMoves);
  TEST_ASSERT_FALSE(budget.allowVentFans);
  TEST_ASSERT_FALSE(budget.allowDefogger);
  TEST_ASSERT_FALSE(budget.allowGrowLight);
  TEST_ASSERT_FALSE(budget.allowCirculation);
}

void test_sensor_sample_freshness_expires_after_timeout() {
  TEST_ASSERT_TRUE(GreenhouseRuntime::sampleFresh(1500U, 1000U, 600U));
  TEST_ASSERT_FALSE(GreenhouseRuntime::sampleFresh(1701U, 1000U, 600U));
  TEST_ASSERT_EQUAL_UINT32(250U, GreenhouseRuntime::sampleAgeMs(1250U, 1000U));
}

void test_runtime_validation_rejects_bad_threshold_configuration() {
  GreenhouseRuntime::ValidationInput input{};
  input.climate = climateConfig();
  input.freshness = GreenhouseRuntime::SensorFreshnessPolicy{20000U, 900000U, 60000U};
  input.sampleIntervalMs = 300000U;
  input.displayIntervalMs = 2000U;
  input.controlIntervalMs = 5000U;
  input.otaPollIntervalMs = 10000U;
  input.mqttPublishIntervalMs = 30000U;
  input.loraMaxRetries = 3U;
  input.loraRetryBackoffMs = 5000U;
  input.batteryLowVoltage = 3.45F;
  input.batteryCriticalVoltage = 3.30F;
  input.batteryFullVoltage = 4.20F;
  input.batteryEmptyVoltage = 3.20F;
  input.topClosedDegrees = 28;
  input.topOpenDegrees = 52;
  input.bottomClosedDegrees = 30;
  input.bottomOpenDegrees = 54;
  input.climate.ventCloseTempC = input.climate.ventOpenTempC + 1.0F;

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRuntime::ConfigFault::climateThresholds),
                          static_cast<uint8_t>(GreenhouseRuntime::validateConfiguration(input)));
}

void test_diagnostic_code_labels_expose_expected_strings() {
  TEST_ASSERT_EQUAL_STRING("LORA_QUEUE_WARN",
                           GreenhouseRuntime::diagnosticCodeLabel(GreenhouseRuntime::DiagnosticCode::loraQueueWarning));
  TEST_ASSERT_EQUAL_STRING("SAFE_MODE",
                           GreenhouseRuntime::diagnosticCodeLabel(GreenhouseRuntime::DiagnosticCode::safeModeEntered));
}

void test_metrics_compute_vpd_and_dew_point_for_valid_air_data() {
  GreenhouseLogic::SensorSnapshot snapshot{};
  snapshot.airAvailable = true;
  snapshot.airTempC = 24.0F;
  snapshot.humidityPct = 70.0F;

  const auto metrics = GreenhouseLogic::evaluateMetrics(snapshot, GreenhouseLogic::CropProfileId::tomato);

  TEST_ASSERT_TRUE(metrics.vpdAvailable);
  TEST_ASSERT_TRUE(metrics.dewPointAvailable);
  TEST_ASSERT_FLOAT_WITHIN(0.05F, 0.90F, metrics.vaporPressureDeficitKPa);
  TEST_ASSERT_FLOAT_WITHIN(0.2F, 18.2F, metrics.dewPointC);
}

void test_frost_risk_triggers_for_near_freezing_air_conditions() {
  GreenhouseLogic::SensorSnapshot snapshot{};
  snapshot.airAvailable = true;
  snapshot.airTempC = 1.0F;
  snapshot.humidityPct = 95.0F;

  const auto metrics = GreenhouseLogic::evaluateMetrics(snapshot, GreenhouseLogic::CropProfileId::lettuce);

  TEST_ASSERT_TRUE(metrics.frostRisk);
}

void test_tomato_profile_reports_optimal_conditions() {
  GreenhouseLogic::SensorSnapshot snapshot{};
  snapshot.airAvailable = true;
  snapshot.airTempC = 24.0F;
  snapshot.humidityPct = 70.0F;

  const auto metrics = GreenhouseLogic::evaluateMetrics(snapshot, GreenhouseLogic::CropProfileId::tomato);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::CropStatus::optimal),
                          static_cast<uint8_t>(metrics.cropStatus));
}

void test_lettuce_profile_reports_stressed_conditions_when_too_hot() {
  GreenhouseLogic::SensorSnapshot snapshot{};
  snapshot.airAvailable = true;
  snapshot.airTempC = 30.0F;
  snapshot.humidityPct = 40.0F;

  const auto metrics = GreenhouseLogic::evaluateMetrics(snapshot, GreenhouseLogic::CropProfileId::lettuce);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::CropStatus::stressed),
                          static_cast<uint8_t>(metrics.cropStatus));
}

void test_lora_link_service_sends_queued_frame_when_transport_is_ready() {
  FakeLoRaTransport transport;
  GreenhouseLoRa::LinkService link(transport);
  link.begin(true, "node-a", 2, 1000U, 42U);

  TEST_ASSERT_TRUE(link.queueTelemetry("temp=24.1", 10U));

  link.poll(10U);

  TEST_ASSERT_EQUAL_UINT32(1U, transport.sendCount);
  TEST_ASSERT_EQUAL_UINT32(GreenhouseLoRa::deriveSessionId("node-a", 42U), transport.lastSessionId);
  TEST_ASSERT_EQUAL_UINT32(1U, transport.lastSequence);
  TEST_ASSERT_EQUAL_UINT32(GreenhouseLoRa::crc32("temp=24.1"), transport.lastCrc32);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().sentFrames);
  TEST_ASSERT_EQUAL_UINT8(0U, link.stats().queueDepth);
}

void test_lora_link_service_retries_and_drops_after_max_retries() {
  FakeLoRaTransport transport;
  transport.sendResult = false;
  GreenhouseLoRa::LinkService link(transport);
  link.begin(true, "node-b", 1, 100U);

  TEST_ASSERT_TRUE(link.queueTelemetry("temp=22.0", 0U));

  link.poll(0U);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().failedSendAttempts);
  TEST_ASSERT_EQUAL_UINT8(1U, link.stats().queueDepth);

  link.poll(150U);
  TEST_ASSERT_EQUAL_UINT32(2U, link.stats().failedSendAttempts);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().droppedFrames);
  TEST_ASSERT_EQUAL_UINT8(0U, link.stats().queueDepth);
}

void test_lora_link_service_tracks_queue_warning_and_max_depth() {
  FakeLoRaTransport transport;
  transport.readyState = false;
  GreenhouseLoRa::LinkService link(transport);
  link.begin(true, "node-q", 2, 100U, 19U, 2U);

  TEST_ASSERT_TRUE(link.queueTelemetry("msg=1", 1U));
  TEST_ASSERT_TRUE(link.queueTelemetry("msg=2", 2U));
  TEST_ASSERT_TRUE(link.queueTelemetry("msg=3", 3U));

  link.poll(4U);

  TEST_ASSERT_TRUE(link.stats().queueWarningActive);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().queueWarningCount);
  TEST_ASSERT_EQUAL_UINT32(3U, link.stats().maxQueueDepthObserved);
  TEST_ASSERT_EQUAL_UINT32(2U, link.stats().lastQueueWarningAtMs);
}

void test_compact_lora_telemetry_contains_core_fields() {
  char payload[GreenhouseLoRa::kPayloadSize];
  GreenhouseLoRa::buildCompactTelemetry(payload,
                                        sizeof(payload),
                                        "node-c",
                                        "AUTO",
                                        false,
                                        87,
                                        true,
                                        24.5F,
                                        68.0F,
                                        true,
                                        3.91F,
                                        76);

  TEST_ASSERT_NOT_EQUAL(nullptr, strstr(payload, "n=node-c"));
  TEST_ASSERT_NOT_EQUAL(nullptr, strstr(payload, "m=AUTO"));
  TEST_ASSERT_NOT_EQUAL(nullptr, strstr(payload, "h=87"));
  TEST_ASSERT_NOT_EQUAL(nullptr, strstr(payload, "bp=76"));
}

void test_lora_wire_packet_round_trips_through_parser() {
  GreenhouseLoRa::TelemetryFrame frame{};
  strncpy(frame.nodeId, "node-d", sizeof(frame.nodeId) - 1U);
  strncpy(frame.payload, "temp=25.2", sizeof(frame.payload) - 1U);
  frame.sessionId = 99U;
  frame.sequence = 7U;
  frame.crc32 = GreenhouseLoRa::crc32(frame.payload);
  frame.requireAck = true;

  char packet[GreenhouseLoRa::kWirePacketSize];
  TEST_ASSERT_GREATER_THAN(0, GreenhouseLoRa::buildWireTelemetryPacket(packet, sizeof(packet), frame));

  uint32_t sessionId = 0;
  uint32_t sequence = 0;
  uint32_t payloadCrc32 = 0;
  bool requireAck = false;
  char nodeId[GreenhouseLoRa::kNodeIdSize]{};
  char payload[GreenhouseLoRa::kPayloadSize]{};
  TEST_ASSERT_TRUE(GreenhouseLoRa::parseWireTelemetryPacket(packet,
                                                            sessionId,
                                                            sequence,
                                                            payloadCrc32,
                                                            requireAck,
                                                            nodeId,
                                                            sizeof(nodeId),
                                                            payload,
                                                            sizeof(payload)));
  TEST_ASSERT_EQUAL_UINT32(99U, sessionId);
  TEST_ASSERT_EQUAL_UINT32(7U, sequence);
  TEST_ASSERT_EQUAL_UINT32(frame.crc32, payloadCrc32);
  TEST_ASSERT_TRUE(requireAck);
  TEST_ASSERT_EQUAL_STRING("node-d", nodeId);
  TEST_ASSERT_EQUAL_STRING("temp=25.2", payload);
}

void test_lora_ack_packet_round_trips_through_parser() {
  char packet[GreenhouseLoRa::kAckPacketSize];
  TEST_ASSERT_GREATER_THAN(0, GreenhouseLoRa::buildAckPacket(packet, sizeof(packet), 33U, 4U, 12345U));

  uint32_t sessionId = 0;
  uint32_t sequence = 0;
  uint32_t payloadCrc32 = 0;
  TEST_ASSERT_TRUE(GreenhouseLoRa::parseAckPacket(packet, sessionId, sequence, payloadCrc32));
  TEST_ASSERT_EQUAL_UINT32(33U, sessionId);
  TEST_ASSERT_EQUAL_UINT32(4U, sequence);
  TEST_ASSERT_EQUAL_UINT32(12345U, payloadCrc32);
}

void test_lora_link_service_rejects_invalid_or_duplicate_inbound_frames() {
  FakeLoRaTransport transport;
  GreenhouseLoRa::LinkService link(transport);
  link.begin(true, "node-z", 2, 1000U, 77U);

  const uint32_t sessionId = link.stats().sessionId;
  const char *payload = "cmd=open";
  const uint32_t checksum = GreenhouseLoRa::crc32(payload);

  TEST_ASSERT_TRUE(link.validateInboundFrame(sessionId, 1U, checksum, payload));
  TEST_ASSERT_FALSE(link.validateInboundFrame(sessionId, 1U, checksum, payload));
  TEST_ASSERT_FALSE(link.validateInboundFrame(sessionId, 2U, checksum + 1U, payload));
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().duplicateInboundFrames);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().invalidInboundFrames);
  TEST_ASSERT_EQUAL_UINT32(1U, link.stats().lastAcceptedSequence);
}

void test_remote_mode_command_parser_accepts_expected_values() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::ModeCommand::automatic),
                          static_cast<uint8_t>(GreenhouseRemote::parseModeCommand("AUTO")));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::ModeCommand::open),
                          static_cast<uint8_t>(GreenhouseRemote::parseModeCommand(" open ")));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::ModeCommand::closed),
                          static_cast<uint8_t>(GreenhouseRemote::parseModeCommand("CLOSED")));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::ModeCommand::invalid),
                          static_cast<uint8_t>(GreenhouseRemote::parseModeCommand("SAFE")));
}

void test_remote_mode_command_rejects_safe_mode_and_bad_topic() {
  char expectedTopic[GreenhouseRemote::kTopicSize];
  GreenhouseRemote::buildModeCommandTopic(expectedTopic, sizeof(expectedTopic), "greenhouse/mini");

  const auto safeAudit = GreenhouseRemote::evaluateModeCommand(true,
                                                               expectedTopic,
                                                               expectedTopic,
                                                               "OPEN",
                                                               111U);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::CommandStatus::rejectedSafeMode),
                          static_cast<uint8_t>(safeAudit.status));

  const auto wrongTopicAudit = GreenhouseRemote::evaluateModeCommand(false,
                                                                     "greenhouse/mini/other",
                                                                     expectedTopic,
                                                                     "OPEN",
                                                                     112U);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::CommandStatus::rejectedTopic),
                          static_cast<uint8_t>(wrongTopicAudit.status));
}

void test_remote_mode_command_accepts_valid_topic_and_payload() {
  char expectedTopic[GreenhouseRemote::kTopicSize];
  GreenhouseRemote::buildModeCommandTopic(expectedTopic, sizeof(expectedTopic), "greenhouse/mini");

  const auto audit = GreenhouseRemote::evaluateModeCommand(false,
                                                           expectedTopic,
                                                           expectedTopic,
                                                           "CLOSED",
                                                           200U);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseRemote::CommandStatus::accepted),
                          static_cast<uint8_t>(audit.status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GreenhouseLogic::ControlMode::forceClosed),
                          static_cast<uint8_t>(audit.applied));
  TEST_ASSERT_EQUAL_UINT32(200U, audit.receivedAtMs);
}

}  // namespace

extern "C" void setUp(void) {}

extern "C" void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_mode_cycle_wraps_in_expected_order);
  RUN_TEST(test_daylight_resolver_uses_light_sensor_when_available);
  RUN_TEST(test_daylight_resolver_falls_back_to_schedule_when_light_sensor_is_unavailable);
  RUN_TEST(test_daylight_resolver_defaults_to_night_when_no_light_sensor_or_time_is_available);
  RUN_TEST(test_force_open_turns_on_venting_and_disables_heat_and_light);
  RUN_TEST(test_force_closed_turns_everything_off);
  RUN_TEST(test_vents_stay_open_between_open_and_close_thresholds_when_previously_open);
  RUN_TEST(test_vents_stay_closed_between_open_and_close_thresholds_when_previously_closed);
  RUN_TEST(test_fans_stay_on_between_fan_on_and_fan_off_thresholds_when_previously_on);
  RUN_TEST(test_fans_stay_off_between_fan_on_and_fan_off_thresholds_when_previously_off);
  RUN_TEST(test_vent_fans_turn_off_at_night_even_when_heat_or_humidity_is_high);
  RUN_TEST(test_vent_fans_turn_back_on_in_daylight_when_thresholds_remain_exceeded);
  RUN_TEST(test_circulation_fans_turn_on_during_daytime_mixing_conditions);
  RUN_TEST(test_circulation_fans_turn_off_at_night_even_when_previously_on);
  RUN_TEST(test_sensor_failure_daytime_opens_vents_and_turns_off_climate_outputs);
  RUN_TEST(test_sensor_failure_night_closes_vents_and_turns_off_climate_outputs);
  RUN_TEST(test_grow_light_uses_schedule_and_lux_threshold);
  RUN_TEST(test_defogger_turns_on_only_during_cold_night_conditions);
  RUN_TEST(test_defogger_stays_off_during_day_even_if_air_is_cold);
  RUN_TEST(test_defogger_uses_off_threshold_to_drop_out_on_warmer_night);
  RUN_TEST(test_grow_light_stays_off_at_exact_lux_threshold);
  RUN_TEST(test_grow_light_stays_off_outside_lighting_window_even_when_dark);
  RUN_TEST(test_grow_light_stays_off_without_time_even_when_dark);
  RUN_TEST(test_optional_outputs_stay_off_when_disabled_in_system_config);
  RUN_TEST(test_controller_state_resolves_low_power_when_battery_is_low);
  RUN_TEST(test_power_budget_sheds_noncritical_loads_at_low_battery);
  RUN_TEST(test_power_budget_sheds_all_controlled_loads_at_critical_battery);
  RUN_TEST(test_sensor_sample_freshness_expires_after_timeout);
  RUN_TEST(test_runtime_validation_rejects_bad_threshold_configuration);
  RUN_TEST(test_diagnostic_code_labels_expose_expected_strings);
  RUN_TEST(test_metrics_compute_vpd_and_dew_point_for_valid_air_data);
  RUN_TEST(test_frost_risk_triggers_for_near_freezing_air_conditions);
  RUN_TEST(test_tomato_profile_reports_optimal_conditions);
  RUN_TEST(test_lettuce_profile_reports_stressed_conditions_when_too_hot);
  RUN_TEST(test_lora_link_service_sends_queued_frame_when_transport_is_ready);
  RUN_TEST(test_lora_link_service_retries_and_drops_after_max_retries);
  RUN_TEST(test_lora_link_service_tracks_queue_warning_and_max_depth);
  RUN_TEST(test_compact_lora_telemetry_contains_core_fields);
  RUN_TEST(test_lora_wire_packet_round_trips_through_parser);
  RUN_TEST(test_lora_ack_packet_round_trips_through_parser);
  RUN_TEST(test_lora_link_service_rejects_invalid_or_duplicate_inbound_frames);
  RUN_TEST(test_remote_mode_command_parser_accepts_expected_values);
  RUN_TEST(test_remote_mode_command_rejects_safe_mode_and_bad_topic);
  RUN_TEST(test_remote_mode_command_accepts_valid_topic_and_payload);
  return UNITY_END();
}
