# Mini Greenhouse Controller

This repository contains the current first-generation automation firmware and documentation set for a small greenhouse built around an ESP32-S3 controller, a 5 V solar plus power-bank electrical system, dual vent servos, sensor-driven climate control, onboard display, data logging, and an expansion path to a later 12 V winter system.

The implementation is based on the full greenhouse planning conversation captured in the shared spec links. It does not assume an empty hardware concept. It assumes a real greenhouse with two vents already installed, a direct-solar USB fan already working, and a near-term goal of adding a reliable brain, power distribution, vent motion, sensing, and clear documentation.

## What is in this repository

- PlatformIO firmware for the ESP32-S3 controller in [platformio.ini](./platformio.ini) and [src/main.cpp](./src/main.cpp)
- Pin mapping and configurable thresholds in [include/PinMap.h](./include/PinMap.h) and [include/Settings.h](./include/Settings.h)
- The consolidated greenhouse master plan in [docs/GREENHOUSE_MASTER_PLAN.md](./docs/GREENHOUSE_MASTER_PLAN.md)
- Canonical diagram and architecture navigation in [docs/ARCHITECTURE_INDEX.md](./docs/ARCHITECTURE_INDEX.md)
- Step-by-step first-generation build instructions in [docs/BUILD_GUIDE_5V.md](./docs/BUILD_GUIDE_5V.md)
- Detailed wiring references in [docs/WIRING_5V.md](./docs/WIRING_5V.md) and [docs/WIRING_12V_UPGRADE.md](./docs/WIRING_12V_UPGRADE.md)
- A categorized bill of materials in [docs/MATERIALS_LIST.md](./docs/MATERIALS_LIST.md)
- Seasonal and safety operating guidance in [docs/OPERATIONS_AND_SAFETY.md](./docs/OPERATIONS_AND_SAFETY.md)
- Explicit implementation boundaries in [docs/FIRMWARE_LIMITATIONS.md](./docs/FIRMWARE_LIMITATIONS.md)
- A maturity and feature-priority plan in [docs/PROFESSIONALIZATION_ROADMAP.md](./docs/PROFESSIONALIZATION_ROADMAP.md)
- Contributor workflow guidance in [docs/DEVELOPER_GUIDE.md](./docs/DEVELOPER_GUIDE.md)
- Field and bench recovery guidance in [docs/TROUBLESHOOTING.md](./docs/TROUBLESHOOTING.md)
- MQTT, Home Assistant, and dashboard payload contract in [docs/REMOTE_TELEMETRY_CONTRACT.md](./docs/REMOTE_TELEMETRY_CONTRACT.md)

## GitHub automation

- CI builds the ESP32-S3 firmware and bundles the docs snapshot through [build-release-bundle.yml](./.github/workflows/build-release-bundle.yml)
- GitHub Pages publishes a rendered documentation site from the markdown sources through [deploy-docs-site.yml](./.github/workflows/deploy-docs-site.yml)
- Repo agent behavior is now defined in the greenhouse-specific [copilot-instructions.md](./.github/copilot-instructions.md)
## Implemented firmware features

- Dual vent control with two servo outputs
- Temperature and humidity control using either a DHT22 or a BME280
- Optional water temperature measurement using a DS18B20
- Optional light sensing using a BH1750
- MOSFET-ready outputs for intake fan, exhaust fan, defogger, grow light, and circulation fans
- Persistent control mode selection using onboard preferences storage
- Manual override buttons for `AUTO`, `FORCE OPEN`, and `FORCE CLOSED` use cases
- OLED status display for climate values and actuator states
- CSV logging to LittleFS
- Optional Wi-Fi and OTA update support when Wi-Fi credentials are configured
- Preferences-backed boot reason logging and boot-failure tracking
- Safe-mode boot after repeated failed boots or by holding both override buttons during startup
- ESP32 task-watchdog and application-progress watchdog recovery policy
- VPD, dew point, frost risk, crop profile evaluation, and crop-status interpretation
- Optional MQTT publishing with Home Assistant discovery payloads

## Quick start

1. Install VS Code with PlatformIO or use an environment that can build PlatformIO projects.
2. Update the Wi-Fi credentials and any threshold values in [include/Settings.h](./include/Settings.h) if you want network time and OTA.
3. Wire the hardware according to [docs/WIRING_5V.md](./docs/WIRING_5V.md), choosing either the current owned-hardware DHT22 plus SG90 path or the fuller BME280 plus BH1750 plus DS18B20 upgrade path.
4. Flash the firmware to the ESP32-S3 board.
5. Commission the system using the checklist in [docs/BUILD_GUIDE_5V.md](./docs/BUILD_GUIDE_5V.md).

## Logic test harness

- Shared control decisions now live in [include/ControlLogic.h](./include/ControlLogic.h).
- Host-side tests for mode transitions and air-sensor fallback live in [test/control_logic/test_control_logic.cpp](./test/control_logic/test_control_logic.cpp).
- If PlatformIO and a native compiler are available, run `pio test -e control_logic_native`.

## Reality status

| Subsystem | Planned | Implemented in repo | Physically installed |
| --- | --- | --- | --- |
| 5 V ESP32-S3 controller layer | Yes | Yes | Not yet installed in greenhouse |
| Direct-solar USB fan branch | Yes | Documented as independent subsystem | Yes |
| Dual vent actuation | Yes | Yes | Vents installed; controller actuation not yet installed |
| Current DHT22 plus SG90 starter path | Yes | Yes | Hardware path available for buildout |
| BME280 plus BH1750 plus DS18B20 upgrade path | Yes | Yes | Not confirmed installed |
| Four-fan LM2596 mixing subsystem | Yes | Documented | Not confirmed installed |
| 12 V winter backbone | Yes | Documented only | No |
| Defogger branch | Yes | Output support documented in firmware and wiring docs | No |
| Grow-light branch | Yes | Output support documented in firmware and wiring docs | No |

## Canonical references

- Use [docs/ARCHITECTURE_INDEX.md](./docs/ARCHITECTURE_INDEX.md) to determine which diagrams are current and which materials are reference-only.
- Use [docs/SAFETY_MODEL.md](./docs/SAFETY_MODEL.md) for the one-page view of what remains active if the controller layer fails.
- Use [docs/FIRMWARE_LIMITATIONS.md](./docs/FIRMWARE_LIMITATIONS.md) before assuming a safety, recovery, or sensor-fallback behavior is already implemented.
- Use [docs/PROFESSIONALIZATION_ROADMAP.md](./docs/PROFESSIONALIZATION_ROADMAP.md) for the prioritized path from current repo state to a more deployable open-source system.
- Use [docs/DEVELOPER_GUIDE.md](./docs/DEVELOPER_GUIDE.md) before refactoring firmware, adding telemetry, or changing pin and threshold behavior.
- Use [docs/REMOTE_TELEMETRY_CONTRACT.md](./docs/REMOTE_TELEMETRY_CONTRACT.md) before building a dashboard, Home Assistant package, or telemetry bridge.

## Maturity stance

- This repo is already strong in system definition, safety boundaries, and build documentation.
- The biggest remaining professionalization gaps are now power-hardware rollout, richer telemetry consumers, irrigation instrumentation, and broader fault sensing rather than the complete absence of reliability or telemetry primitives.
- The share links reviewed for this topic were mostly unrelated controller-remapper and generic ESP32 inspiration material, so the roadmap in this repo is grounded primarily in the greenhouse code and docs that actually exist here.

## Design boundaries

- The current repository implements the control logic, onboard status display, logging path, and documentation for the 5 V greenhouse controller, and documents the future 12 V expansion.
- The direct-solar USB fan already in service remains a separate always-on-in-sun subsystem by design.
- The future 12 V winter system is documented in full, including wiring, sizing, and safety rules, but is intentionally not driven by the current 5 V controller hardware until the separate 12 V backbone is installed.
