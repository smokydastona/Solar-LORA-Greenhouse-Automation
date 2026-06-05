# Firmware Update Procedure

This document defines the supported firmware update paths for the project.

Current truth:

- USB flashing is the guaranteed service path.
- Browser-based Wi-Fi firmware upload is now implemented on the local dashboard using the board's OTA partition table.
- ArduinoOTA remains optional convenience when it is enabled in settings.
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
7. Confirm the boot banner reports the expected firmware version from `include/Version.h`.
8. Verify the OLED initializes and the expected sensors report sane values.
9. If battery telemetry is enabled, verify the reading is still plausible after the update.

## Optional Update Method: Browser Upload Over Wi-Fi

Use the built-in dashboard upload form when:

- the node is already reachable on the joined Wi-Fi network or setup AP
- power is stable
- you have the raw `firmware.bin` from the latest local build or GitHub release bundle

### Browser update steps

1. Put the greenhouse in a safe service state before starting the upload.
2. Open the node dashboard on its `.local` hostname, reported IP address, or `http://192.168.4.1/` if it is still in setup mode.
3. In the `Update Firmware` section, choose the raw `firmware.bin` file.
4. Start the upload and wait for the node to reboot automatically.
5. Reopen the dashboard and confirm the boot banner and page report the new firmware version.

## Optional Update Method: ArduinoOTA

Use ArduinoOTA only when:

- Wi-Fi is stable
- the controller is already healthy
- ArduinoOTA is enabled in [../include/Settings.h](../include/Settings.h)
- physical access still exists as a recovery path

### OTA safety rules

- Do not treat Wi-Fi update paths as the only service path for an unattended greenhouse.
- Do not start any Wi-Fi firmware upload during unstable power conditions or during weather that could require immediate vent control.
- Upload only the raw `firmware.bin`, not the merged flash image.
- If an update fails or the board stops booting correctly, fall back to USB service.

## Post-Update Verification Checklist

After any firmware update, verify:

1. boot completes without entering safe mode unexpectedly
2. the serial boot banner and OLED boot screen both show the intended firmware version
3. the display is readable
4. override buttons still work
5. primary air sensor data is present
6. servo motion remains in the intended safe travel range
7. logs still write if LittleFS is healthy
8. MQTT still publishes if networked telemetry is enabled

## Update Boundary

- No rollback strategy is currently documented as production-ready in this repo, even though OTA partitions exist.
- Until that changes, USB remains the authoritative recovery path.

## Read Next

- [installation.md](./installation.md)
- [troubleshooting.md](./troubleshooting.md)
- [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md)