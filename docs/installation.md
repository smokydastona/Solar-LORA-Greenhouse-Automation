# Installation Guide

This document is the standard entry point for installing, flashing, wiring, and commissioning the current 5 V greenhouse controller build.

For the detailed staged build sequence, use [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md).

## 1. Prepare The Build

- confirm the target board is the expected SX1262 LoRa V3 ESP32-S3 family
- decide whether you are building the starter path or the fuller sensor path
- inspect the greenhouse structure before adding electronics
- choose dry cable routes and a weather-resistant enclosure location

## 2. Flash The Firmware

- install PlatformIO
- build with `pio run -e greenhouse_controller`
- upload with `pio run -e greenhouse_controller -t upload`
- verify serial output at `115200` baud

## 3. Wire The Starter Path

- wire the DHT22 to the configured GPIO and safe logic rail
- wire the two vent servos to the dedicated servo rail and common ground
- keep board-reserved OLED and LoRa pins untouched
- use the board battery-read path rather than adding a direct battery wire to a random ADC pin

## 4. Expand Sensors If Needed

- add BME280 and BH1750 on the shared I2C bus
- add DS18B20 with the correct pull-up resistor
- enable only the sensors that are actually installed in [../include/Settings.h](../include/Settings.h)

## 5. Commission Safely

- verify the OLED initializes
- verify sensors show real values instead of `N/A`
- bench-test servo travel before full linkage travel
- verify local logging works
- if battery monitoring is enabled, compare the reading with a multimeter before trusting percentage
- if MQTT is enabled, verify state publishing and Home Assistant discovery against the real broker

## Pre-Power Wiring Verification Checklist

Before first power-on after wiring or rewiring:

- confirm servo power is not being drawn from the ESP32 USB pin
- confirm all grounds that must be common are actually common
- confirm no reserved board pins were repurposed accidentally
- confirm DHT22 data is not being over-driven by a 5 V-only breakout without a logic-safe arrangement
- confirm MOSFET modules are wired with the intended branch polarity
- confirm no raw battery or panel voltage is connected directly to an ESP32 GPIO
- confirm fuse placement matches the intended branch layout before energizing higher-current paths

## 6. Field Deployment Rules

- do not power servos from the board USB pin
- keep load wiring and logic wiring physically organized
- keep the enclosure out of standing water and drip paths
- treat safe mode and USB flashing as the primary recovery path if the network is unavailable

## Read Next

- [WIRING_5V.md](./WIRING_5V.md)
- [troubleshooting.md](./troubleshooting.md)
- [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md)
- [firmware-update.md](./firmware-update.md)
- [winterization.md](./winterization.md)