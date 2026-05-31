# Architecture Overview

This document is the standard entry point for understanding the system architecture of the Mini Greenhouse Controller.

For the canonical diagrams and greenhouse-specific companion docs, use [ARCHITECTURE_INDEX.md](./ARCHITECTURE_INDEX.md).

## Current System Goal

The active system is a controller-backed 5 V greenhouse automation layer that improves temperature, humidity, airflow, and operator visibility without making the greenhouse depend entirely on a single smart controller.

The design deliberately keeps one airflow path independent:

- direct-solar fan path: intentionally simple and independent
- controller-backed path: buffered power, sensing, logging, actuation, and optional telemetry

## Architecture Layers

### 1. Independent thermal safety layer

- direct-solar USB fan remains outside controller dependency
- if the controller is down, the greenhouse still has some passive heat-response airflow during sun

### 2. Controller and power layer

- ESP32-S3 LoRa board acts as the main control node
- active electrical target is 5 V solar plus power-bank buffering
- future 12 V winter backbone is documented separately and is not claimed as current live infrastructure

### 3. Sensor layer

- starter path: DHT22 plus SG90 vent actuation
- fuller path: BME280, BH1750, and DS18B20
- sensor availability is surfaced explicitly so disconnects do not masquerade as zero readings

### 4. Decision layer

- explicit thresholds and hysteresis
- mode handling: `AUTO`, `OPEN`, `CLOSED`, and safe mode behavior
- greenhouse metrics: VPD, dew point, frost risk, and crop-profile interpretation

### 5. Actuation layer

- dual vent servos
- MOSFET-ready exhaust, intake, defogger, grow-light, and circulation outputs

### 6. Operator and telemetry layer

- onboard OLED status display
- LittleFS CSV logging
- boot-event logging and reset-reason tracking
- optional Wi-Fi MQTT publishing and Home Assistant discovery
- LoRa hardware present on the board, but a completed greenhouse LoRa transport is not yet claimed in this repo

## Reliability Posture

Implemented in firmware today:

- watchdog handling
- safe-mode boot path
- boot-failure tracking
- sensor disconnect visibility
- flash logging with deliberate avoidance of destructive auto-format behavior
- onboard battery voltage sensing through the board battery-read path

Not yet complete to a full off-grid fleet standard:

- solar current and charge telemetry
- charge-state accounting
- offline packet queueing over LoRa
- automatic channel recovery and link-quality tracking over live LoRa transport
- dedicated RTC hardware backing independent of network time

## Canonical Diagrams

- Current-state 5 V architecture: [diagrams/greenhouse-5v-summer-architecture.png](./diagrams/greenhouse-5v-summer-architecture.png)
- Future 5 V plus 12 V architecture: [diagrams/greenhouse-5v-plus-12v-winter-architecture.png](./diagrams/greenhouse-5v-plus-12v-winter-architecture.png)
- Board reference pinout: [diagrams/SX1262_Pinout.png](./diagrams/SX1262_Pinout.png)

## Read Next

- [hardware.md](./hardware.md)
- [installation.md](./installation.md)
- [api.md](./api.md)
- [ARCHITECTURE_INDEX.md](./ARCHITECTURE_INDEX.md)
- [known-issues.md](./known-issues.md)