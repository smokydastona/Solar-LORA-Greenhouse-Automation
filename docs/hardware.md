# Hardware Overview

This document is the standard entry point for hardware requirements, build tiers, rough cost posture, and overall build difficulty.

For the greenhouse-specific BOM, use [MATERIALS_LIST.md](./MATERIALS_LIST.md). For exact wiring, use [WIRING_5V.md](./WIRING_5V.md).

## Minimum Supported Hardware

Starter build:

- 1 x Heltec-style SX1262 LoRa V3 ESP32-S3 board with onboard OLED
- 1 x DHT22 / AM2302 sensor
- 2 x SG90 vent servos
- 5 V solar source and USB power bank
- enclosure, common-ground wiring, and basic distribution hardware

## Recommended Expanded Hardware

- BME280 for primary air sensing
- BH1750 for light sensing
- DS18B20 waterproof probe for water temperature
- MOSFET trigger modules for switched load branches
- buttons for `AUTO`, `FORCE OPEN`, and `FORCE CLOSED`
- cable glands, weather-resistant enclosure, desiccant, and fuse planning

## Power-System Posture

Current active power target:

- 5 V solar plus power-bank controller system
- independent direct-solar fan remains separate by design

Future documented upgrade:

- 12 V winter backbone with more serious energy storage and charging hardware

## Reliability-Relevant Hardware Notes

Already supported or documented in firmware and docs:

- onboard battery voltage sensing through the board `VBAT_Read` path
- boot and watchdog recovery behavior
- sensor disconnect visibility
- onboard OLED for local state inspection

Still open engineering work for a stronger off-grid deployment:

- solar panel voltage telemetry
- solar current telemetry
- battery current telemetry
- charge-state estimation based on more than battery voltage alone
- battery-temperature-aware protection
- enclosure aging and condensation monitoring

## Rough Cost Posture

This repository does not yet keep a versioned vendor price table, so it should not claim a precise build cost.

The practical budget bands are:

- starter build: low hundreds of USD for controller, sensors, servos, enclosure parts, and 5 V power hardware
- fuller monitored build: higher once more sensors, switched loads, and weather-hardening parts are included
- winter-capable 12 V system: materially higher because it adds charge control and real energy storage hardware

If this project grows into broader public adoption, a versioned BOM with price snapshots should be added instead of leaving cost as an estimate band.

## Build Difficulty

Expected difficulty: moderate.

You should be comfortable with:

- low-voltage wiring and common-ground discipline
- flashing firmware with PlatformIO
- bench testing sensors and servos before greenhouse deployment
- reading logs and verifying battery behavior with a multimeter

This is not a one-evening beginner kit, but it is still intentionally within hobbyist and maker reach.

## Read Next

- [installation.md](./installation.md)
- [WIRING_5V.md](./WIRING_5V.md)
- [MATERIALS_LIST.md](./MATERIALS_LIST.md)