#include <unity.h>

#include "ControlLogic.h"
#include "ControlLogicDefaults.h"

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

void test_daylight_resolver_defaults_to_day_when_no_light_sensor_or_time_is_available() {
  auto ctx = baseContext();
  ctx.snapshot.lightAvailable = false;

  TEST_ASSERT_TRUE(GreenhouseLogic::resolveDaylight(ctx.snapshot, ctx.climate, false, 0U));
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

}  // namespace

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_mode_cycle_wraps_in_expected_order);
  RUN_TEST(test_daylight_resolver_uses_light_sensor_when_available);
  RUN_TEST(test_daylight_resolver_falls_back_to_schedule_when_light_sensor_is_unavailable);
  RUN_TEST(test_daylight_resolver_defaults_to_day_when_no_light_sensor_or_time_is_available);
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
  RUN_TEST(test_metrics_compute_vpd_and_dew_point_for_valid_air_data);
  RUN_TEST(test_frost_risk_triggers_for_near_freezing_air_conditions);
  RUN_TEST(test_tomato_profile_reports_optimal_conditions);
  RUN_TEST(test_lettuce_profile_reports_stressed_conditions_when_too_hot);
  return UNITY_END();
}