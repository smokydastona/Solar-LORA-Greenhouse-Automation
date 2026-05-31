#include <unity.h>

#include "ControlLogic.h"

namespace {

GreenhouseLogic::ClimateConfig climateConfig() {
  return GreenhouseLogic::ClimateConfig{
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

}  // namespace

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_mode_cycle_wraps_in_expected_order);
  RUN_TEST(test_force_open_turns_on_venting_and_disables_heat_and_light);
  RUN_TEST(test_force_closed_turns_everything_off);
  RUN_TEST(test_sensor_failure_daytime_opens_vents_and_turns_off_climate_outputs);
  RUN_TEST(test_sensor_failure_night_closes_vents_and_turns_off_climate_outputs);
  RUN_TEST(test_grow_light_uses_schedule_and_lux_threshold);
  RUN_TEST(test_defogger_turns_on_only_during_cold_night_conditions);
  RUN_TEST(test_defogger_stays_off_during_day_even_if_air_is_cold);
  RUN_TEST(test_defogger_uses_off_threshold_to_drop_out_on_warmer_night);
  RUN_TEST(test_grow_light_stays_off_at_exact_lux_threshold);
  RUN_TEST(test_grow_light_stays_off_outside_lighting_window_even_when_dark);
  RUN_TEST(test_grow_light_stays_off_without_time_even_when_dark);
  return UNITY_END();
}