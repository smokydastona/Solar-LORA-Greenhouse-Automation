# Dashboard Integrations

This document defines the current dashboard posture for the project and provides starter artifacts for Home Assistant and Grafana.

Current truth:

- Home Assistant is a realistic first remote dashboard target today because MQTT discovery is already implemented in firmware.
- Grafana is only a starter example right now. A real Grafana deployment still needs a time-series storage bridge outside the current firmware repo.

## Current Dashboard Surface

The implemented telemetry contract already exposes the following dashboard-relevant fields over MQTT:

- controller mode and safe-mode state
- last remote mode-command result
- health score
- air temperature and humidity
- VPD and dew point
- frost risk
- crop profile and crop status
- battery raw millivolts, voltage, percentage, and band
- actuator states
- connectivity, LoRa ACK health, and storage readiness

Use [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md) as the payload authority.

## Home Assistant

Recommended current path:

1. Enable MQTT in [../include/Settings.h](../include/Settings.h).
2. Connect the controller to a real broker.
3. Allow MQTT discovery to create the base entities automatically.
4. Layer a dashboard view on top of those discovered entities.

The firmware now also publishes an MQTT/Home Assistant select entity for control mode. It accepts only `AUTO`, `OPEN`, and `CLOSED` and mirrors the current accepted mode state.

Starter artifacts:

- Lovelace dashboard YAML: [examples/home-assistant/mini-greenhouse-dashboard.yaml](./examples/home-assistant/mini-greenhouse-dashboard.yaml)

That example assumes the firmware is publishing under the default base topic and Home Assistant discovery is enabled.

## Grafana

Current truth:

- the repo does not yet ship an ingestion bridge from MQTT to InfluxDB, Prometheus, Loki, or another Grafana-friendly backend
- the provided Grafana file is therefore a starter dashboard layout, not a verified end-to-end deployment package

Starter artifact:

- Grafana dashboard JSON: [examples/grafana/mini-greenhouse-overview.json](./examples/grafana/mini-greenhouse-overview.json)

Use it only after you have a real telemetry pipeline that stores:

- air temperature
- humidity
- VPD
- dew point
- battery voltage
- battery percentage
- health score

## Recommended Dashboard Layout

### Overview

- health score
- mode
- safe mode
- reset reason
- battery state

### Environment

- air temperature
- humidity
- VPD
- dew point
- frost risk
- crop status

### Power

- battery voltage
- battery percentage
- battery band
- low and critical flags
- future solar telemetry once the roadmap hardware exists

### Actuation

- vent positions
- exhaust fan
- intake fan
- defogger
- grow light
- circulation

### Diagnostics

- last remote command status
- LoRa queue depth
- LoRa acknowledged count
- LoRa ACK timeout count
- LoRa ready state
- filesystem readiness

## Compatibility Rule

If the MQTT payload or entity names change, update this document and the example files in the same change set.
