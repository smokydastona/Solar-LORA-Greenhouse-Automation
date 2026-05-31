# Firmware Limitations

This document states what the current firmware does, what it does not yet do, and what operating assumptions remain policy rather than fully verified automation behavior.

Use this file to prevent over-trusting the controller during deployment planning.

## Current implemented behavior

- Manual `AUTO`, `OPEN`, and `CLOSED` modes are implemented.
- CSV logging to internal LittleFS is implemented.
- Separate boot-event logging is implemented in LittleFS.
- Logged sensor rows now include explicit availability flags for air, water, and light readings.
- OTA is optional and only available when Wi-Fi credentials are configured and OTA is enabled.
- Preferences-backed boot reason logging and failed-boot counting are implemented.
- Safe-mode boot is implemented for repeated failed boots and manual two-button entry at startup.
- Brownout-triggered safe mode and unfinished-servo recovery boot handling are implemented, and those inspection boots now hold the servos detached instead of re-driving them immediately.
- An ESP32 task watchdog and an application-progress watchdog are implemented.
- Repeated invalid air-sensor reads can now escalate into safe mode.
- Sensor values now expire by age instead of remaining valid indefinitely after the last successful sample.
- Servo movement is now time-limited and detached after a drive window, with repeated retrigger attempts able to force safe mode.
- VPD, dew point, frost-risk evaluation, and crop-profile interpretation are implemented.
- MQTT publishing and Home Assistant discovery are implemented when configured.
- A compact LoRa telemetry payload, queue, retry policy, session identifier, CRC metadata, and inbound duplicate-rejection primitive are implemented in code, even though the concrete radio backend is still disabled.
- Compile-time configuration is now validated at boot, and an invalid threshold or timing combination forces config-fault safe mode instead of running with a broken policy.
- The controller supports the current owned-hardware DHT22 plus SG90 path and the fuller BME280 plus DS18B20 plus BH1750 path documented elsewhere in the repo.

## Not currently implemented as verified firmware features

- Battery-voltage awareness is enabled for the Heltec onboard `VBAT_Read` path, but the reading should still be checked against a meter, trimmed if needed, and explicitly marked calibrated in settings on the real board.
- Direct current-sensed servo-stall detection is not implemented.
- Remote mode commands are not implemented; the current MQTT and Home Assistant path is read-only state telemetry.
- Hardware-in-the-loop validation is not part of the current repo workflow.
- LittleFS is no longer auto-formatted on mount failure; a storage fault now leaves logging disabled until the filesystem is repaired deliberately.
- The LoRa abstraction does not yet include a live SX1262 transport driver, link-quality metrics, or verified over-the-air commissioning flow.

## Host-side control-logic coverage now present

- Host-side tests cover mode cycling behavior.
- Host-side tests cover the day/night resolver path, including light-sensor precedence, schedule fallback, and no-time fallback.
- Host-side tests cover forced-open and forced-closed output behavior.
- Host-side tests cover vent hysteresis retention between open and close thresholds.
- Host-side tests cover fan hysteresis retention between on and off thresholds.
- Host-side tests cover automatic vent-fan shutdown at night and restart after daylight returns.
- Host-side tests cover circulation-fan daytime mixing behavior and night shutdown.
- Host-side tests cover the conservative main air-sensor fallback behavior.
- Host-side tests cover defogger night gating and heater off-threshold behavior.
- Host-side tests cover grow-light schedule and lux-threshold edge cases.
- Host-side tests cover LoRa queue send, retry, drop, and compact-payload formatting behavior.
- These tests validate the shared control-decision function, not the full board-level hardware stack.

## Safe operating interpretation

- Treat the current system as safe by simple architecture and conservative operating policy, not by full fault-tolerant automation.
- The independent direct-solar fan remains valuable because it still provides heat-response airflow if the controller layer is down.
- The future 12 V backbone is not currently integrated with the live 5 V controller hardware.
- Battery percentage is only meaningful after the onboard Heltec battery-read path is verified against a real meter on the deployed board and the calibration-verified flag is set.
- When neither a light sensor nor valid local time is available, day/night-dependent logic now defaults to night behavior instead of assuming daytime.
- Low battery now sheds grow-light and circulation loads first, and critical battery suppresses all controller-backed switched loads and servo movement to protect the controller path.

## Intended safe-policy targets

These are design intentions documented in the repo. They should not be treated as implemented unless the firmware and commissioning notes explicitly say so.

- If the main air sensor is unavailable, default to a conservative day-open and night-closed vent policy, but only treat daytime vent-open behavior as available when the controller has either a valid light reading or valid local time.
- If the water-temperature probe fails, continue greenhouse operation without thermal-mass logic.
- If battery voltage is low, the implemented low-power policy now sheds non-critical loads before sacrificing the controller.
- If jam detection is added later, verify it with real hardware sensing rather than inferred motion alone.

## Deployment boundary

- USB flashing is the guaranteed update path.
- OTA remains optional convenience, not a required service path.
- Before unattended long-duration deployment, final battery calibration, jam handling, and hardware-in-the-loop validation should still be treated as open engineering work.
