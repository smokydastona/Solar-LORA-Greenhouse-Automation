# API And Telemetry Surface

This document is the standard entry point for machine-consumable integrations.

The current implemented integration surface is MQTT over Wi-Fi with Home Assistant discovery. For the full canonical payload contract, use [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md).

## Implemented Today

- retained MQTT controller state topic
- retained MQTT availability topic
- Home Assistant MQTT discovery payloads
- structured JSON state including climate, battery, health, actuator, and connectivity fields

## Current Topic Model

Default base topic:

- `greenhouse/mini/state`
- `greenhouse/mini/availability`

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
- uptime

## Integrations Status

Implemented:

- MQTT
- Home Assistant

Not yet implemented as first-class repo surfaces:

- Grafana package
- Prometheus exporter
- REST API
- webhooks
- finished LoRa telemetry transport

## Compatibility Rule

If the payload shape or topic structure changes, [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md) should change in the same commit.

## Read Next

- [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md)
- [development.md](./development.md)