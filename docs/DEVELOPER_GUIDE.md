# Developer Guide

This guide is for contributors working on the firmware, docs, and automation flows in this repository.

It is not a generic ESP32 template. It reflects the current greenhouse controller architecture and the safety-sensitive constraints of this project.

## Project shape

- [../src/main.cpp](../src/main.cpp) contains the board-level runtime: initialization, sensor reads, mode changes, display updates, logging, and actuator output writes.
- [../include/ControlLogic.h](../include/ControlLogic.h) contains the shared greenhouse decision logic that can be tested on the host.
- [../include/Settings.h](../include/Settings.h) contains user-tunable thresholds, feature toggles, and derived control-system configuration.
- [../include/PinMap.h](../include/PinMap.h) is the hardware contract for the current ESP32-S3 target board.
- [../test/control_logic/test_control_logic.cpp](../test/control_logic/test_control_logic.cpp) validates the pure decision layer.

## Design rules that matter

- Preserve the separation between independent direct-solar cooling and controller-backed subsystems.
- Prefer explicit thresholds and fail-safe states over opaque automation.
- Do not claim a recovery, telemetry, or safety feature unless it is actually implemented and documented.
- When behavior changes, update the matching docs in the same change set.

## Local workflow

### Recommended commands

- Build firmware: `pio run -e greenhouse_controller`
- Run host-side control tests: `pio test -e control_logic_native`
- Upload firmware: `pio run -e greenhouse_controller -t upload`
- Serial monitor: `pio device monitor -b 115200`

### Current environment caveat

This workspace has previously been edited in environments where PlatformIO was not installed locally. If `pio` is unavailable, use editor diagnostics for quick checks but treat that as weaker validation than a real build or test run.

## Where to put new logic

### Good candidates for `ControlLogic.h`

- deterministic climate decisions
- calculations such as VPD, dew point, or risk indices
- mode transition logic
- policy evaluation that does not require hardware access

### Good candidates for `main.cpp`

- sensor driver integration
- display formatting
- storage and logging behavior
- Wi-Fi, OTA, LoRa, and time synchronization
- GPIO output writes and boot-time initialization

## Change impact checklist

### If you change pins

- update [../include/PinMap.h](../include/PinMap.h)
- update [WIRING_5V.md](./WIRING_5V.md)
- update any build or commissioning text that mentions those pins

### If you change thresholds or operating behavior

- update [../include/Settings.h](../include/Settings.h)
- update [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md)
- update [GREENHOUSE_MASTER_PLAN.md](./GREENHOUSE_MASTER_PLAN.md)
- update [OPERATIONS_AND_SAFETY.md](./OPERATIONS_AND_SAFETY.md) if the user-facing behavior changes

### If you add or change sensors

- keep I2C and OneWire expectations accurate in docs
- update [MATERIALS_LIST.md](./MATERIALS_LIST.md) if hardware requirements changed
- update display or logging docs if telemetry changes

## Testing expectations

### Minimum bar

- touched files should be diagnostic-clean
- pure logic changes should have or update host-side tests
- doc changes should preserve truthful claims and working links

### Preferred bar

- build the firmware locally or in CI
- run the control logic tests locally or in CI
- if behavior changes affect commissioning, update the operator docs before finishing the change

## CI surfaces

- [../.github/workflows/build-release-bundle.yml](../.github/workflows/build-release-bundle.yml) builds firmware, bundles docs, and can publish a release bundle.
- [../.github/workflows/deploy-docs-site.yml](../.github/workflows/deploy-docs-site.yml) renders the markdown docs site for GitHub Pages.

## Good next engineering targets

- watchdog and safe-mode boot policy
- battery and solar telemetry
- greenhouse metrics such as VPD and dew point
- MQTT or Home Assistant integration
- clearer separation between telemetry generation and transport layers

## How to contribute without breaking trust

- keep claims narrow and verifiable
- avoid placeholder logic and fake hardware support
- do not silently replace greenhouse-specific decisions with generic smart-home patterns
- preserve the current low-voltage safety stance unless the repo explicitly grows into a different electrical architecture