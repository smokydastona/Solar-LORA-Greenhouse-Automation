# Known Issues

This document tracks the important current limitations that an operator, builder, or contributor should know before treating the project as unattended-production hardware.

It is not a duplicate of [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md). This file is the shorter public-facing list of the most important open issues and boundaries.

## Current Known Issues

### Servo protection is conservative, not fully instrumented

- The firmware now uses time-limited servo drive windows, cooldown lockout, and safe-mode escalation on repeated retrigger attempts.
- It still does not measure servo current or true motion feedback, so a hard mechanical bind can still go undiagnosed electrically.
- Keep servo travel conservative and treat bench travel checks as mandatory until a hardware-backed jam signal exists.

### Battery telemetry is voltage-only today

- The board now reads battery voltage through the onboard battery-read path.
- The value still needs meter verification and the firmware now exposes whether that calibration has been explicitly verified, along with raw millivolts and an operator-facing battery band.
- Solar current, charge state, daily energy harvest, and runtime estimation are not yet live firmware features.

### OTA is convenience-only, not a field-proof update path

- OTA exists when Wi-Fi is configured.
- USB flashing remains the guaranteed recovery and service path.
- A rollback-capable OTA strategy is not yet claimed.

### LoRa backend exists, but field commissioning is still unfinished

- The target board includes LoRa hardware.
- The firmware now includes a live RadioLib-backed SX1262 transport with queue retries and matching ACKs for raw LoRa telemetry.
- The active first-generation implementation still uses local control and optional Wi-Fi MQTT telemetry for the real deployed path.
- Do not assume LoRa is field-ready until you validate legal regional frequency choice, receiver interoperability, and real greenhouse link quality on your actual node pair.

### Remote MQTT mode control is intentionally narrow

- The firmware now accepts only `AUTO`, `OPEN`, and `CLOSED` over the dedicated MQTT mode-command topic.
- Safe mode still requires physical recovery. Remote mode commands are rejected while the controller is in safe mode.
- Direct remote actuator commands are intentionally not part of the current safety posture.

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