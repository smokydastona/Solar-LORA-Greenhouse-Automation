# API And Telemetry Surface

This document is the standard entry point for machine-consumable integrations.

The current implemented integration surface is MQTT over Wi-Fi with Home Assistant discovery, a node-local HTTP dashboard, and raw point-to-point SX1262 LoRa telemetry. For the full canonical payload contract, use [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md).

## Implemented Today

- retained MQTT controller state topic
- retained MQTT availability topic
- retained MQTT mode-command state topic
- retained MQTT mode-command result topic
- Home Assistant MQTT discovery payloads
- safe inbound MQTT mode control accepting only `AUTO`, `OPEN`, and `CLOSED`
- structured JSON state including climate, battery, health, diagnostics, actuator, and connectivity fields
- local HTTP dashboard and setup page with station-mode live controls
- nearby Wi-Fi scan JSON endpoint at `/api/wifi/scan`
- local control endpoints for live mode changes, manual override, and per-output commands
- local climate-settings endpoints for authenticated threshold editing from the station dashboard
- local firmware upload endpoint at `/update`

## Local HTTP Surface

The node-local HTTP interface currently exposes:

- `GET /`: dashboard, Wi-Fi onboarding form, station-mode live control surface, and firmware upload form
- `GET /api/state`: live JSON state snapshot for dashboards, bridges, and local troubleshooting
- `GET /api/control`: current control mode, manual-override state, and effective actuator state
- `POST /api/control/mode`: update controller mode with `AUTO`, `OPEN`, `CLOSED`, or `MANUAL`
- `POST /api/control/manual`: enable or release manual output override with an `enabled` toggle
- `POST /api/control/actuator`: set an individual output while manual override is active
- `GET /api/settings/climate`: read the active climate threshold profile and override status
- `POST /api/settings/climate`: save an authenticated climate threshold override profile
- `POST /api/settings/climate/reset`: restore firmware-default climate thresholds after authentication
- `GET /api/settings/display`: read the active OLED idle mode and whether it comes from firmware defaults or a dashboard override
- `POST /api/settings/display`: save an authenticated OLED idle-mode override
- `POST /api/settings/display/reset`: restore firmware-default OLED idle behavior after authentication
- `GET /api/wifi/scan`: nearby SSID scan results for the onboarding dropdown
- `POST /wifi`: save Wi-Fi credentials and restart
- `POST /wifi/reset`: clear saved Wi-Fi and restart into setup mode
- `POST /update`: upload a `firmware.bin` over Wi-Fi and reboot into the new image

## Local HTTP Contract Notes

- `/api/state` is the broadest machine-readable snapshot and is the best starting point for local dashboard work.
- `/api/control` is narrower and exists to support the built-in station dashboard with a simpler control-focused payload.
- `/api/settings/climate` and `/api/settings/display` return both the active values and whether they come from firmware defaults or a dashboard override.
- `/api/control/mode`, `/api/control/manual`, and `/api/control/actuator` are intentionally local-LAN control surfaces. They are not designed for direct internet exposure.
- settings writes are the local dashboard operations with an explicit password gate today

The current write-time authentication rule is:

- the station dashboard must present the current station Wi-Fi password
- if the station network is open, the node setup password is also accepted for dashboard settings writes

## Current Topic Model

Default base topic:

- `greenhouse/mini/state`
- `greenhouse/mini/availability`
- `greenhouse/mini/command/mode/set`
- `greenhouse/mini/command/mode/state`
- `greenhouse/mini/command/mode/result`

Home Assistant discovery uses the configured discovery prefix, defaulting to `homeassistant`.

## Current State Model

The published state includes:

- mode and safe-mode state
- resolved controller state
- reset reason
- crop profile and crop status
- air, water, and light sensor availability and readings
- greenhouse metrics such as VPD and dew point
- active climate-threshold profile and whether a dashboard override is active through the climate-settings endpoint
- battery status
- controller health score
- recent persisted diagnostic event, per-sensor error counters, and servo timing diagnostics
- power-budget gates for servo moves, vent fans, defogger, grow light, and circulation
- manual-override active state for the local dashboard
- OLED idle mode, sleep state, and idle timeout windows
- actuator state
- connectivity and storage status
- last remote command status
- LoRa ACK, queue-pressure, drop, and signal diagnostics
- uptime

## Integrations Status

Implemented:

- MQTT
- Home Assistant
- Dashboard starter configs documented in [dashboards.md](./dashboards.md)

Not yet implemented as first-class repo surfaces:

- Grafana ingestion bridge or database pipeline
- Prometheus exporter
- webhooks

Grafana starter artifacts now exist in this repo, but they are still examples only. They depend on an external MQTT-to-time-series bridge that the repo does not currently ship.

The HTTP control surface is intentionally node-local and unauthenticated. It is meant for the greenhouse owner on the same LAN, not for internet exposure.

Climate threshold editing is narrower: the station dashboard requires the current station Wi-Fi password, or the node setup password if the station network is open, before it will save threshold changes.

## VPN Boundary

Once the node has joined the normal Wi-Fi network, it behaves like a standard HTTP device on the LAN. It does not proxy traffic, inject station-mode DNS, or block VPN use on other clients. If a phone or laptop VPN client still cannot reach the dashboard, that limit is on the client VPN policy such as local-LAN blocking, not on the node firmware.

## Compatibility Rule

If the payload shape, topic structure, or local HTTP surface changes, this document and any affected dashboard docs should change in the same commit.

## Read Next

- [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md)
- [development.md](./development.md)
- [dashboards.md](./dashboards.md)