# Mini Greenhouse Controller

This repository contains a complete first-generation automation project for a small greenhouse built around an ESP32-S3 controller, a 5 V solar plus power-bank electrical system, dual vent servos, sensor-driven climate control, onboard display, data logging, and an expansion path to a later 12 V winter system.

The implementation is based on the full greenhouse planning conversation captured in the shared spec links. It does not assume an empty hardware concept. It assumes a real greenhouse with two vents already installed, a direct-solar USB fan already working, and a near-term goal of adding a reliable brain, power distribution, vent motion, sensing, and clear documentation.

## What is in this repository

- PlatformIO firmware for the ESP32-S3 controller in [platformio.ini](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/platformio.ini) and [src/main.cpp](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/src/main.cpp)
- Pin mapping and configurable thresholds in [include/PinMap.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/PinMap.h) and [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h)
- The consolidated greenhouse master plan in [docs/GREENHOUSE_MASTER_PLAN.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/GREENHOUSE_MASTER_PLAN.md)
- Step-by-step first-generation build instructions in [docs/BUILD_GUIDE_5V.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/BUILD_GUIDE_5V.md)
- Detailed wiring references in [docs/WIRING_5V.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/WIRING_5V.md) and [docs/WIRING_12V_UPGRADE.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/WIRING_12V_UPGRADE.md)
- A categorized bill of materials in [docs/MATERIALS_LIST.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/MATERIALS_LIST.md)
- Seasonal and safety operating guidance in [docs/OPERATIONS_AND_SAFETY.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/OPERATIONS_AND_SAFETY.md)

## GitHub automation

- CI builds the ESP32-S3 firmware and bundles the docs snapshot through [build-release-bundle.yml](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/.github/workflows/build-release-bundle.yml)
- GitHub Pages publishes a rendered documentation site from the markdown sources through [deploy-docs-site.yml](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/.github/workflows/deploy-docs-site.yml)
- Repo agent behavior is now defined in the greenhouse-specific [copilot-instructions.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/.github/copilot-instructions.md)
## Implemented firmware features

- Dual vent control with two servo outputs
- Temperature and humidity control using a BME280
- Water temperature measurement using a DS18B20
- Light sensing using a BH1750
- MOSFET-ready outputs for intake fan, exhaust fan, defogger, grow light, and circulation fans
- Persistent control mode selection using onboard preferences storage
- Manual override buttons for `AUTO`, `FORCE OPEN`, and `FORCE CLOSED` use cases
- OLED status display for climate values and actuator states
- CSV logging to LittleFS
- Optional Wi-Fi and OTA update support when Wi-Fi credentials are configured

## Quick start

1. Install VS Code with PlatformIO or use an environment that can build PlatformIO projects.
2. Update the Wi-Fi credentials and any threshold values in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h) if you want network time and OTA.
3. Wire the hardware according to [docs/WIRING_5V.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/WIRING_5V.md).
4. Flash the firmware to the ESP32-S3 board.
5. Commission the system using the checklist in [docs/BUILD_GUIDE_5V.md](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/docs/BUILD_GUIDE_5V.md).

## Design boundaries

- The current repository fully implements the 5 V greenhouse controller and documents the future 12 V expansion.
- The direct-solar USB fan already in service remains a separate always-on-in-sun subsystem by design.
- The future 12 V winter system is documented in full, including wiring, sizing, and safety rules, but is intentionally not driven by the current 5 V controller hardware until the separate 12 V backbone is installed.
