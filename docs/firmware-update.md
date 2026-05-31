# Firmware Update Procedure

This document defines the supported firmware update paths for the project.

Current truth:

- USB flashing is the guaranteed service path.
- OTA is optional convenience only.
- The repo does not yet claim a rollback-safe remote update system.

## Preferred Update Method: USB

Use USB whenever possible for planned upgrades, recovery, or commissioning.

### Update steps

1. Put the greenhouse in a safe service state before changing firmware.
2. Prefer `CLOSED` mode or a physically safe vent state before disconnecting power.
3. Power the controller from a known-good local USB connection.
4. Build with `pio run -e greenhouse_controller`.
5. Flash with `pio run -e greenhouse_controller -t upload`.
6. Open serial monitor at `115200` baud and confirm normal boot.
7. Verify the OLED initializes and the expected sensors report sane values.
8. If battery telemetry is enabled, verify the reading is still plausible after the update.

## Optional Update Method: OTA

Use OTA only when:

- Wi-Fi is stable
- the controller is already healthy
- physical access still exists as a recovery path

### OTA safety rules

- Do not treat OTA as the only update path for an unattended remote greenhouse.
- Do not start OTA during unstable power conditions or during weather that could require immediate vent control.
- If OTA fails or the board stops booting correctly, fall back to USB service.

## Post-Update Verification Checklist

After any firmware update, verify:

1. boot completes without entering safe mode unexpectedly
2. the display is readable
3. override buttons still work
4. primary air sensor data is present
5. servo motion remains in the intended safe travel range
6. logs still write if LittleFS is healthy
7. MQTT still publishes if networked telemetry is enabled

## Update Boundary

- No rollback partition strategy is currently documented as production-ready in this repo.
- Until that changes, USB remains the authoritative recovery path.

## Read Next

- [installation.md](./installation.md)
- [troubleshooting.md](./troubleshooting.md)
- [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md)