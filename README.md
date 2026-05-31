# Mini Greenhouse Controller

This repository is a real small-greenhouse automation project for an ESP32-S3 controller running from a 5 V solar plus power-bank system, with dual vent actuation, local sensing, onboard display, local logging, optional MQTT telemetry, and a documented path to a later 12 V winter backbone.

It is not a generic smart-garden demo. It is designed around a real greenhouse with two installed vents, at least one already-working direct-solar fan, and a requirement for low-voltage, family-safe unattended operation.

## What Problem This Solves

Small greenhouses routinely overheat, trap condensation, and drift into unhealthy humidity swings long before a human notices. This repo adds a controller-backed layer that can:

- open and close vents based on climate conditions
- drive exhaust, intake, defogging, grow-light, and circulation outputs
- calculate greenhouse-health metrics such as VPD and dew point instead of exposing only raw sensor values
- log data locally for diagnosis and tuning
- expose a stable MQTT and Home Assistant telemetry contract when networked operation is wanted

The design keeps one important boundary intact: direct-solar airflow remains intentionally independent, so the greenhouse still gets some heat-response airflow even if the controller is down.

## Why LoRa Instead Of Wi-Fi

The target board is a Heltec-style ESP32-S3 LoRa board because greenhouse deployments often live at the edge of home Wi-Fi reliability or beyond it entirely. LoRa matters here for range, lower infrastructure dependence, and future multi-node or outbuilding expansion.

Current truth:

- the repo already uses Wi-Fi for optional OTA, time sync, MQTT, and Home Assistant integration
- the board hardware already includes LoRa capability
- this repo does not yet claim a completed greenhouse LoRa transport layer

So the answer is not "LoRa instead of Wi-Fi everywhere." The current stance is "Wi-Fi where it is convenient, LoRa where greenhouse distance, resilience, or future fleet layout makes Wi-Fi the wrong backbone."

## Why Solar

The greenhouse already uses solar-backed airflow, and the controller design keeps that off-grid direction for three reasons:

- it reduces dependency on running mains into a wet greenhouse environment
- it matches a low-voltage, repairable, family-safe deployment model
- it makes power telemetry and load budgeting a first-class engineering concern instead of an afterthought

The current live target is a 5 V solar plus power-bank system. A heavier 12 V winter architecture is documented separately and intentionally not presented as already deployed.

## Hardware Required

Minimum starter build:

- 1 x SX1262 LoRa V3 class ESP32-S3 board with onboard OLED
- 1 x DHT22 / AM2302 air temperature and humidity sensor
- 2 x SG90 vent servos
- 5 V solar source and USB power bank
- common low-voltage wiring, enclosure, and distribution hardware

Recommended fuller build:

- BME280 for primary air sensing
- BH1750 for light sensing
- DS18B20 for water temperature
- MOSFET modules for exhaust, intake, defogger, grow light, and circulation branches
- weather-resistant enclosure, glands, fuse planning, and desiccant

See [docs/hardware.md](./docs/hardware.md) for the standardized hardware overview and [docs/MATERIALS_LIST.md](./docs/MATERIALS_LIST.md) for the current greenhouse-specific BOM.

## Architecture At A Glance

The active architecture is intentionally simple:

1. Direct-solar fan path remains independent and dumb.
2. ESP32-S3 controller runs the buffered 5 V logic layer.
3. Sensors feed climate state into explicit control logic.
4. Servos and MOSFET-switched loads act on that state.
5. OLED, LittleFS logging, and optional MQTT expose status and diagnostics.
6. A future 12 V winter backbone is documented as a later power-system upgrade, not current reality.

Use [docs/architecture.md](./docs/architecture.md) for the standard architecture overview and [docs/ARCHITECTURE_INDEX.md](./docs/ARCHITECTURE_INDEX.md) for the canonical diagram set.

## Cost And Build Difficulty

This repo now documents hardware clearly, but it does not yet maintain a vendor-pinned live price sheet. The practical answer today is a rough build band, not a locked quote:

- starter controller build: low hundreds of USD if you need to buy the controller, servos, sensor, enclosure parts, and power hardware
- fuller monitored build: higher once additional sensors, switched-load hardware, enclosure hardening, and solar power instrumentation are added
- winter-capable 12 V system: materially more expensive because it adds real energy-storage and charging hardware

Difficulty is moderate for someone comfortable with low-voltage wiring, PlatformIO firmware flashing, and bench testing before field deployment. It is not a beginner breadboard exercise, but it is also not an industrial controls project.

## Documentation Map

- [docs/architecture.md](./docs/architecture.md): system shape, boundaries, and diagrams
- [docs/hardware.md](./docs/hardware.md): required hardware, BOM tiers, cost posture, and build difficulty
- [docs/installation.md](./docs/installation.md): install, flash, wire, and commission flow
- [docs/troubleshooting.md](./docs/troubleshooting.md): field and bench diagnosis
- [docs/api.md](./docs/api.md): telemetry contract and integration surface
- [docs/development.md](./docs/development.md): contributor workflow, testing, and CI surface
- [docs/dashboards.md](./docs/dashboards.md): dashboard guidance and starter Home Assistant and Grafana artifacts
- [docs/versioning.md](./docs/versioning.md): release and versioning rules for firmware, docs, and telemetry
- [docs/power-telemetry-roadmap.md](./docs/power-telemetry-roadmap.md): staged plan for serious solar and battery telemetry
- [docs/known-issues.md](./docs/known-issues.md): short public list of the most important current deployment gaps
- [docs/firmware-update.md](./docs/firmware-update.md): supported update and recovery procedure
- [docs/winterization.md](./docs/winterization.md): focused cold-weather preparation checklist

## What Is In This Repository

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
- Field and bench recovery guidance in [docs/troubleshooting.md](./docs/troubleshooting.md)
- MQTT, Home Assistant, and dashboard payload contract in [docs/REMOTE_TELEMETRY_CONTRACT.md](./docs/REMOTE_TELEMETRY_CONTRACT.md)
- Dashboard integration guidance and starter configs in [docs/dashboards.md](./docs/dashboards.md)
- Versioning and release-note policy in [docs/versioning.md](./docs/versioning.md)
- Versioned release history in [CHANGELOG.md](./CHANGELOG.md)
- Public known deployment issues in [docs/known-issues.md](./docs/known-issues.md)
- Firmware update procedure in [docs/firmware-update.md](./docs/firmware-update.md)
- Winter preparation checklist in [docs/winterization.md](./docs/winterization.md)

## GitHub Automation

- CI builds the ESP32-S3 firmware and bundles the docs snapshot through [build-release-bundle.yml](./.github/workflows/build-release-bundle.yml)
- GitHub Pages publishes a rendered documentation site from the markdown sources through [deploy-docs-site.yml](./.github/workflows/deploy-docs-site.yml)
- Repo agent behavior is now defined in the greenhouse-specific [copilot-instructions.md](./.github/copilot-instructions.md)

## Implemented Firmware Features

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
- Heltec onboard battery-voltage sensing on the board's `VBAT_Read` path
- Optional MQTT publishing with Home Assistant discovery payloads

## Quick Start

1. Install VS Code with PlatformIO or use an environment that can build PlatformIO projects.
2. Update the Wi-Fi credentials and any threshold values in [include/Settings.h](./include/Settings.h) if you want network time and OTA.
3. Wire the hardware according to [docs/WIRING_5V.md](./docs/WIRING_5V.md), choosing either the current owned-hardware DHT22 plus SG90 path or the fuller BME280 plus BH1750 plus DS18B20 upgrade path.
4. Flash the firmware to the ESP32-S3 board.
5. Commission the system using the checklist in [docs/BUILD_GUIDE_5V.md](./docs/BUILD_GUIDE_5V.md).

## Logic Test Harness

- Shared control decisions now live in [include/ControlLogic.h](./include/ControlLogic.h).
- Host-side tests for mode transitions and air-sensor fallback live in [test/control_logic/test_control_logic.cpp](./test/control_logic/test_control_logic.cpp).
- If PlatformIO and a native compiler are available, run `pio test -e control_logic_native`.

## Reality Status

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

## Canonical References

- Use [docs/ARCHITECTURE_INDEX.md](./docs/ARCHITECTURE_INDEX.md) to determine which diagrams are current and which materials are reference-only.
- Use [docs/SAFETY_MODEL.md](./docs/SAFETY_MODEL.md) for the one-page view of what remains active if the controller layer fails.
- Use [docs/FIRMWARE_LIMITATIONS.md](./docs/FIRMWARE_LIMITATIONS.md) before assuming a safety, recovery, or sensor-fallback behavior is already implemented.
- Use [docs/PROFESSIONALIZATION_ROADMAP.md](./docs/PROFESSIONALIZATION_ROADMAP.md) for the prioritized path from current repo state to a more deployable open-source system.
- Use [docs/DEVELOPER_GUIDE.md](./docs/DEVELOPER_GUIDE.md) before refactoring firmware, adding telemetry, or changing pin and threshold behavior.
- Use [docs/REMOTE_TELEMETRY_CONTRACT.md](./docs/REMOTE_TELEMETRY_CONTRACT.md) before building a dashboard, Home Assistant package, or telemetry bridge.
- Use [docs/dashboards.md](./docs/dashboards.md) before wiring Home Assistant or Grafana around the current telemetry contract.
- Use [CHANGELOG.md](./CHANGELOG.md) and [docs/versioning.md](./docs/versioning.md) when cutting or documenting future releases.

## Maturity Stance

- Reliability primitives such as boot tracking, safe mode, watchdog handling, flash-write discipline, and sensor-availability reporting now exist in code.
- Greenhouse intelligence now goes beyond raw temperature and humidity and includes VPD, dew point, frost risk, and crop profiles.
- Optional MQTT and Home Assistant discovery are implemented, but richer integrations such as Grafana, Prometheus, REST, webhooks, and finished LoRa transport remain future work.
- Power telemetry is still earlier than the rest of the system: onboard battery sensing is wired in firmware, but solar current, charge-state telemetry, and full energy accounting are not yet complete.

- This repo is already strong in system definition, safety boundaries, and build documentation.
- The biggest remaining professionalization gaps are now power-system telemetry, richer telemetry consumers, irrigation instrumentation, LoRa transport completion, and broader fault sensing rather than the complete absence of reliability or telemetry primitives.
- The roadmap in this repo is grounded in the greenhouse code and docs that actually exist here, not in aspirational claims about hardware or integrations that have not yet been validated.

## Design Boundaries

- The current repository implements the control logic, onboard status display, logging path, and documentation for the 5 V greenhouse controller, and documents the future 12 V expansion.
- The direct-solar USB fan already in service remains a separate always-on-in-sun subsystem by design.
- The future 12 V winter system is documented in full, including wiring, sizing, and safety rules, but is intentionally not driven by the current 5 V controller hardware until the separate 12 V backbone is installed.
