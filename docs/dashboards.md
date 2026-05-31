# Dashboard Integrations

This document defines the current dashboard posture for the project and provides starter artifacts for Home Assistant and Grafana.

Current truth:

- Home Assistant is a realistic first remote dashboard target today because MQTT discovery is already implemented in firmware.
- Grafana is only a starter example right now. A real Grafana deployment still needs a time-series storage bridge outside the current firmware repo.

## Current Dashboard Surface

The implemented telemetry contract already exposes the following dashboard-relevant fields over MQTT:

- controller mode and safe-mode state
- health score
- air temperature and humidity
- VPD and dew point
- frost risk
- crop profile and crop status
- battery voltage and percentage
- actuator states
- connectivity and storage readiness

Use [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md) as the payload authority.

## Home Assistant

Recommended current path:

1. Enable MQTT in [../include/Settings.h](../include/Settings.h).
2. Connect the controller to a real broker.
3. Allow MQTT discovery to create the base entities automatically.
4. Layer a dashboard view on top of those discovered entities.

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
- low and critical flags
- future solar telemetry once the roadmap hardware exists

### Actuation

- vent positions
- exhaust fan
- intake fan
- defogger
- grow light
- circulation

## Compatibility Rule

If the MQTT payload or entity names change, update this document and the example files in the same change set.
