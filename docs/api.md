# API And Telemetry Surface

This document is the standard entry point for machine-consumable integrations.

The current implemented integration surface is MQTT over Wi-Fi with Home Assistant discovery plus raw point-to-point SX1262 LoRa telemetry. For the full canonical payload contract, use [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md).

## Implemented Today

- retained MQTT controller state topic
- retained MQTT availability topic
- retained MQTT mode-command state topic
- retained MQTT mode-command result topic
- Home Assistant MQTT discovery payloads
- safe inbound MQTT mode control accepting only `AUTO`, `OPEN`, and `CLOSED`
- structured JSON state including climate, battery, health, actuator, and connectivity fields

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
- reset reason
- crop profile and crop status
- air, water, and light sensor availability and readings
- greenhouse metrics such as VPD and dew point
- battery status
- controller health score
- actuator state
- connectivity and storage status
- last remote command status
- LoRa ACK and signal diagnostics
- uptime

## Integrations Status

Implemented:

- MQTT
- Home Assistant
- Dashboard starter configs documented in [dashboards.md](./dashboards.md)

Not yet implemented as first-class repo surfaces:

- Grafana package
- Prometheus exporter
- REST API
- webhooks
- direct remote actuator commands

## Compatibility Rule

If the payload shape or topic structure changes, [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md) should change in the same commit.

## Read Next

- [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md)
- [development.md](./development.md)
- [dashboards.md](./dashboards.md)