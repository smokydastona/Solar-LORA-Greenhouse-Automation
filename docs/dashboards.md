# Dashboard Integrations

This document defines the current dashboard posture for the project and provides starter artifacts for Home Assistant and Grafana.

Current truth:

- Every node can now serve its own local status and Wi-Fi-setup page directly from the ESP32-S3 over HTTP.
- Home Assistant is a realistic first remote dashboard target today because MQTT discovery is already implemented in firmware.
- Grafana is still a starter example only. A real Grafana deployment still needs a time-series storage bridge outside the current firmware repo.

## Local Node Dashboard

The firmware now exposes a built-in node-local dashboard before any central brain exists.

Current behavior:

- If Wi-Fi credentials are already stored, the node serves its dashboard over the joined network at the IP address shown on serial and OLED.
- If no Wi-Fi credentials are available or the configured network cannot be joined, the node starts a setup AP and serves the same page at `http://192.168.4.1/`.
- The page exposes a local status view, a JSON state endpoint at `/api/state`, a nearby-network scan endpoint for SSID selection, Wi-Fi and controller settings forms, clipboard and pasted-text import for copied Wi-Fi share strings or QR payloads, a browser firmware-upload form for `firmware.bin`, and a reset action that clears saved Wi-Fi and reopens setup mode.

Recommended first-node workflow:

1. Flash the firmware.
2. Join the setup SSID if the board is not yet configured.
3. Open `http://192.168.4.1/`.
4. Pick the target SSID from the nearby-network dropdown, then import copied Wi-Fi share text from the commissioning phone or PC or enter only the password manually.
5. Reopen the node dashboard on the address the board reports after restart.

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
- OLED idle mode, sleep state, and configured idle timeouts
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

The starter package now intentionally groups the entities into:

- operator health and power gauges
- control and safety state including the MQTT mode select entity
- climate summary and trend history
- power and LoRa diagnostics
- actuation state glance cards

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
- LoRa queue depth
- LoRa ACK metrics

The included JSON is a stronger starter layout, but it still assumes you have already normalized greenhouse telemetry into Grafana-queryable metric names such as:

- `mini_greenhouse_air_temperature_c`
- `mini_greenhouse_humidity_pct`
- `mini_greenhouse_vpd_kpa`
- `mini_greenhouse_dew_point_c`
- `mini_greenhouse_battery_voltage_v`
- `mini_greenhouse_battery_percentage`
- `mini_greenhouse_lora_queue_depth`
- `mini_greenhouse_lora_acknowledged`
- `mini_greenhouse_lora_ack_timeouts`

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

## Reality check

This repo now includes stronger dashboard artifacts, but it still does not include:

- a broker-to-database bridge
- Grafana provisioning automation
- Home Assistant package auto-install logic
- a central cloud dashboard service

Treat the example files as operator-facing starting points that match the implemented firmware contract, not as a claim that the full remote dashboard stack is shipped in-repo.

## Compatibility Rule

If the MQTT payload or entity names change, update this document and the example files in the same change set.
