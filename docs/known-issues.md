# Known Issues

This document tracks the important current limitations that an operator, builder, or contributor should know before treating the project as unattended-production hardware.

It is not a duplicate of [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md). This file is the shorter public-facing list of the most important open issues and boundaries.

## Current Known Issues

### Servo jam and stall protection is not implemented

- The firmware can command vent motion, but it does not yet detect a stalled or jammed servo.
- A mechanical bind, ice, or linkage problem can still overstress SG90-class hardware.
- Until this is implemented, keep servo travel conservative and treat bench travel checks as mandatory.

### Battery telemetry is voltage-only today

- The board now reads battery voltage through the onboard battery-read path.
- The value still needs meter verification and calibration on the real board.
- Solar current, charge state, daily energy harvest, and runtime estimation are not yet live firmware features.

### OTA is convenience-only, not a field-proof update path

- OTA exists when Wi-Fi is configured.
- USB flashing remains the guaranteed recovery and service path.
- A rollback-capable OTA strategy is not yet claimed.

### LoRa hardware exists, but greenhouse LoRa transport does not

- The target board includes LoRa hardware.
- The active first-generation implementation still uses local control and optional Wi-Fi MQTT telemetry.
- Do not assume there is already a completed radio telemetry protocol, packet retry policy, or link-quality layer in this repo.

### The 12 V winter architecture is documented, not deployed

- The future 12 V upgrade is intentional and well documented.
- It is not the live electrical backbone for the active 5 V controller build.
- Do not mix 12 V upgrade assumptions into the current 5 V commissioning flow unless you are explicitly building that later system.

### Hardware-in-the-loop validation is still missing

- Host-side logic tests exist.
- Real board, power-loss, and actuator stress validation are still outside the current repo workflow.

## Read Next

- [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md)
- [troubleshooting.md](./troubleshooting.md)
- [power-telemetry-roadmap.md](./power-telemetry-roadmap.md)