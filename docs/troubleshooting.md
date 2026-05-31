# Troubleshooting Guide

This document is the standard troubleshooting entry point for the project.

This guide covers the most likely field and bench issues for the current 5 V greenhouse controller build.

It assumes the current repo boundary: ESP32-S3 controller, local display, local logging, dual vent servos, and optional Wi-Fi OTA.

## First question: what still works?

Before changing wiring or reflashing firmware, answer these quickly:

1. Does the direct-solar fan still run in sun?
2. Does the controller board power on?
3. Does the OLED initialize?
4. Do the override buttons still change modes?
5. Are sensor values present or showing `N/A`?
6. Is logging still working or did LittleFS mount fail?

These answers separate a controller problem from a greenhouse-wide power problem.

## Controller does not boot

### Likely causes

- bad USB cable or weak 5 V rail
- power-bank pass-through behavior is unstable
- wiring short on the 5 V load branch
- board damaged by moisture or condensation

### Checks

1. Disconnect servos and switched loads.
2. Power the ESP32-S3 from known-good USB only.
3. Confirm the status LED and OLED behavior.
4. Inspect the enclosure for moisture, corrosion, or conductive debris.

## OLED stays blank

### Likely causes

- board power issue
- incorrect target board assumptions
- damaged OLED or board-level rail issue

### Checks

1. Confirm the board is the expected SX1262 LoRa V3 ESP32-S3 family.
2. Verify no external wiring is shorting reserved board pins.
3. Check serial output at 115200 baud.

## Sensors show `N/A`

### Air sensor missing

Likely causes:

- DHT22 wiring error
- DHT22 powered incorrectly for the actual module type
- BME280 not present while enabled, or vice versa

Checks:

1. Confirm which air sensor path is enabled in [../include/Settings.h](../include/Settings.h).
2. For DHT22, prefer 3.3 V supply unless the specific breakout requires otherwise.
3. If using a 5 V-only DHT22 breakout, do not drive GPIO 16 directly without a logic-safe arrangement.

### Water temperature missing

Likely causes:

- DS18B20 not connected
- missing pull-up resistor
- probe damage or wrong pin

Checks:

1. Confirm DS18B20 data is on the configured OneWire pin.
2. Confirm the pull-up is present and correct.
3. Check for damaged cable or water ingress at the probe head.

### Light value missing

Likely causes:

- BH1750 not enabled or not wired
- I2C bus wiring issue

Checks:

1. Confirm SDA and SCL match the board-reserved I2C pins.
2. Confirm the BH1750 address matches the settings.

## Servos chatter, stall, or move the wrong way

### Likely causes

- servo powered from the controller rail instead of a dedicated 5 V servo rail
- travel angles too aggressive
- linkage binding or vent geometry mismatch
- SG90 torque limit reached

### Checks

1. Power servos from a dedicated 5 V rail, not the board USB pin.
2. Reduce open and closed angles back to conservative bench values.
3. Verify linkage direction and neutral position before increasing travel.
4. Upgrade to MG90S-class servos if the vents are too heavy for SG90 units.

## Vents or fans behave unexpectedly in `AUTO`

### Likely causes

- thresholds do not match actual greenhouse conditions
- air sensor is unavailable, causing conservative fallback behavior
- time or light conditions changed the day/night resolver outcome

### Checks

1. Confirm whether the display shows valid air values or `N/A`.
2. If air data is unavailable, remember the fallback is day-open and night-closed only when the controller can still resolve day or night from either the light sensor or valid local time.
3. If BH1750 is disabled and Wi-Fi time is unavailable, day/night-dependent behavior now fails closed to night logic.
4. Review threshold values in [../include/Settings.h](../include/Settings.h).

## OTA does not work

### Likely causes

- Wi-Fi credentials missing or wrong
- OTA disabled in settings
- controller never joined Wi-Fi

### Checks

1. Confirm `ssid`, `password`, and `enableOta` in [../include/Settings.h](../include/Settings.h).
2. Confirm the greenhouse has a stable Wi-Fi signal where the board is mounted.
3. Treat USB flashing as the guaranteed update path if Wi-Fi is unreliable.

## Logging stops working

### Symptom

- no new CSV rows appear
- serial output reports LittleFS mount failure

### Meaning

The firmware no longer auto-formats LittleFS on mount failure. That is intentional to avoid silently destroying evidence or data.

### Checks

1. Inspect serial output for the mount-failure message.
2. Repair or deliberately reinitialize storage during service.
3. Do not assume missing logs mean the greenhouse was idle.

## Power bank drains too fast

### Likely causes

- pass-through charging is weak or unstable
- fan and servo load is higher than expected
- solar input is undersized for the local weather window

### Checks

1. Test the power bank in daylight with the controller and servos disconnected.
2. Add one load branch at a time and observe runtime.
3. Measure fan current instead of relying on product listings.

## Condensation inside the enclosure

### Likely causes

- enclosure mounted in a drip path
- poor gland sealing
- no desiccant or saturated desiccant
- enclosure placed too close to cold wet surfaces

### Checks

1. Reorient cable entries downward or sideways.
2. Replace or dry desiccant packs.
3. Move the enclosure out of the splash zone.

## When to stop and repair before redeploying

Do not continue unattended operation if any of these are true:

- controller power is unstable
- servos bind or stall repeatedly
- enclosure shows active condensation
- sensor readings are missing for the primary air sensor
- battery or solar behavior is unknown after a power-related failure

## Useful reference docs

- [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md)
- [WIRING_5V.md](./WIRING_5V.md)
- [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md)
- [SAFETY_MODEL.md](./SAFETY_MODEL.md)
- [installation.md](./installation.md)