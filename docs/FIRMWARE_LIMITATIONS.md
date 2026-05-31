# Firmware Limitations

This document states what the current firmware does, what it does not yet do, and what operating assumptions remain policy rather than fully verified automation behavior.

Use this file to prevent over-trusting the controller during deployment planning.

## Current implemented behavior

- Manual `AUTO`, `OPEN`, and `CLOSED` modes are implemented.
- CSV logging to internal LittleFS is implemented.
- Logged sensor rows now include explicit availability flags for air, water, and light readings.
- OTA is optional and only available when Wi-Fi credentials are configured and OTA is enabled.
- The controller supports the current owned-hardware DHT22 plus SG90 path and the fuller BME280 plus DS18B20 plus BH1750 path documented elsewhere in the repo.

## Not currently implemented as verified firmware features

- Battery-voltage awareness is not implemented.
- Servo-jam detection is not implemented.
- A dedicated watchdog or self-recovery policy is not documented as implemented behavior.
- Hardware-in-the-loop validation is not part of the current repo workflow.
- LittleFS is no longer auto-formatted on mount failure; a storage fault now leaves logging disabled until the filesystem is repaired deliberately.

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
- These tests validate the shared control-decision function, not the full board-level hardware stack.

## Safe operating interpretation

- Treat the current system as safe by simple architecture and conservative operating policy, not by full fault-tolerant automation.
- The independent direct-solar fan remains valuable because it still provides heat-response airflow if the controller layer is down.
- The future 12 V backbone is not currently integrated with the live 5 V controller hardware.

## Intended safe-policy targets

These are design intentions documented in the repo. They should not be treated as implemented unless the firmware and commissioning notes explicitly say so.

- If the main air sensor is unavailable, default to a conservative day-open and night-closed vent policy.
- If the water-temperature probe fails, continue greenhouse operation without thermal-mass logic.
- If battery monitoring is added later, shed non-critical loads before sacrificing the controller.
- If jam detection is added later, verify it with real hardware sensing rather than inferred motion alone.

## Deployment boundary

- USB flashing is the guaranteed update path.
- OTA remains optional convenience, not a required service path.
- Before unattended long-duration deployment, battery state awareness, jam handling, and watchdog behavior should be treated as open engineering work rather than implied features.
