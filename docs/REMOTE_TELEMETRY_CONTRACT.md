# Remote Telemetry Contract

This document defines the current remote telemetry surface implemented by the greenhouse controller firmware.

It exists so dashboards, Home Assistant, and any future LoRa or cloud bridge can target a stable state model instead of reverse-engineering ad hoc serial output.

## Transport

- Primary transport: MQTT over Wi-Fi
- Secondary firmware contract: compact LoRa payload queued through the internal radio abstraction
- MQTT client library: PubSubClient
- State publishing: retained JSON payloads
- Availability: retained `online` or `offline` topic
- Home Assistant integration: MQTT discovery payloads published automatically on broker connection when enabled
- LoRa status exposure: session id plus queue depth, sent, dropped, duplicate, and invalid counters are included in the JSON state payload

## Configuration surface

MQTT is disabled by default.

Enable and configure it in [../include/Settings.h](../include/Settings.h) under `Settings::MQTT`.

Relevant fields:

- `host`
- `port`
- `username`
- `password`
- `clientId`
- `baseTopic`
- `discoveryPrefix`
- `enableHomeAssistantDiscovery`
- `publishIntervalMs`

LoRa queue configuration lives beside it in [../include/Settings.h](../include/Settings.h) under `Settings::LORA`.

## Topics

Assuming the default `baseTopic` is `greenhouse/mini`:

| Topic | Purpose |
| --- | --- |
| `greenhouse/mini/state` | retained full-controller JSON state |
| `greenhouse/mini/availability` | retained availability state: `online` or `offline` |

If Home Assistant discovery is enabled, the firmware also publishes retained config payloads under:

- `homeassistant/sensor/.../config`
- `homeassistant/binary_sensor/.../config`

## State payload

The current state topic publishes a retained JSON document shaped like this:

```json
{
  "mode": "AUTO",
  "safe_mode": {
    "active": false,
    "reason": "NONE",
    "boot_failures": 0
  },
  "reset_reason": "POWERON",
  "crop": {
    "profile": "Tomatoes",
    "status": "OPTIMAL"
  },
  "air": {
    "available": true,
    "age_ms": 0,
    "temp_c": 24.1,
    "humidity_pct": 70.0
  },
  "water": {
    "available": false,
    "age_ms": 4294967295,
    "temp_c": 0.0
  },
  "light": {
    "available": true,
    "age_ms": 0,
    "lux": 14820.0
  },
  "metrics": {
    "vpd_kpa": 0.89,
    "dew_point_c": 18.2,
    "frost_risk": false
  },
  "battery": {
    "available": false,
    "calibrated": false,
    "voltage_v": 0.0,
    "percent": 0,
    "low": false,
    "critical": false
  },
  "health": {
    "score": 100
  },
  "actuators": {
    "top_open": false,
    "bottom_open": false,
    "fan_exhaust": false,
    "fan_intake": false,
    "defogger": false,
    "grow_light": false,
    "circulation": false
  },
  "connectivity": {
    "wifi": true,
    "mqtt": true,
    "ota": false,
    "lora": false
  },
  "storage": {
    "filesystem_ready": true
  },
  "lora": {
    "session_id": 123456789,
    "queue_depth": 0,
    "sent": 0,
    "dropped": 0,
    "duplicate_inbound": 0,
    "invalid_inbound": 0
  },
  "uptime_ms": 123456
}
```

## Field semantics

### `mode`

- `AUTO`
- `OPEN`
- `CLOSED`
- `SAFE`

### `safe_mode`

- `active`: whether the controller is suppressing all outputs and holding a conservative state
- `reason`: `NONE`, `MANUAL`, `BOOT`, `BROWNOUT`, `RECOVERY`, `SENSOR`, or `SERVO`
- `boot_failures`: consecutive pending-boot count observed by the preferences-backed boot policy

### `reset_reason`

Maps directly to ESP32 reset categories such as:

- `POWERON`
- `SOFTWARE`
- `PANIC`
- `TASK_WDT`
- `BROWNOUT`

### `crop`

- `profile`: selected crop profile label
- `status`: `OPTIMAL`, `ACCEPTABLE`, `STRESSED`, or `UNAVAILABLE`

### `metrics`

- `vpd_kpa`: vapor pressure deficit in kilopascals
- `dew_point_c`: dew point in Celsius
- `frost_risk`: conservative boolean frost-warning signal

### `battery`

Battery values come from the board's onboard `VBAT_Read` path when voltage monitoring is enabled in settings.

Interpret them like this:

- `available` remains `false`
- `calibrated` remains `false` until the reading has been checked against a real meter and explicitly marked verified in settings
- voltage and percent should not be treated as trusted operational battery truth until both `available` and `calibrated` are `true`

### Sensor freshness

- `air.age_ms`, `water.age_ms`, and `light.age_ms` report how long it has been since the last valid reading for each sensor surface
- once a sensor exceeds its configured freshness window, `available` flips to `false` and the controller stops treating the cached value as live input

### `health`

`score` is a simple controller-health heuristic based on sensor availability, filesystem readiness, safe mode, battery state, and expected connectivity.

It is intended as an operator summary, not a formal reliability metric.

### `power_budget`

- `servo_moves`: whether the controller is currently allowed to issue new vent servo moves
- `vent_fans`: whether controller-backed intake and exhaust fans are allowed to run
- `defogger`, `grow_light`, `circulation`: whether those branches are currently allowed to energize under the active battery policy

## Home Assistant discovery entities

When discovery is enabled, the firmware currently publishes discovery for:

- air temperature
- humidity
- VPD
- dew point
- battery percentage
- health score
- crop status
- frost risk
- exhaust fan state
- intake fan state
- defogger state
- grow-light state
- circulation state

The current integration is read-only. It does not yet accept mode changes or remote actuator commands.

## LoRa compact payload contract

The firmware also builds a compact ASCII payload for the internal LoRa queue. It is designed for low-bandwidth telemetry and currently includes:

- node id
- mode
- safe-mode flag
- health score
- air temperature and humidity when available
- battery voltage and percentage when available

Each queued frame also carries a boot-scoped session id, a sequence number, and a CRC32 computed over the payload. The link service can reject duplicate or invalid inbound frames through its built-in validation window.

The queue and retry policy are implemented in firmware, but the concrete SX1262 radio transport is still intentionally disabled until the on-air backend is validated.

## Remote dashboard minimum viable layout

Any dashboard consuming this contract should, at minimum, expose:

### Overview

- mode
- safe mode state and reason
- health score
- reset reason

### Environment

- air temperature
- humidity
- VPD
- dew point
- frost risk
- crop profile and crop status

### Power

- battery percent
- battery voltage
- low or critical battery flags

### Actuation

- vent positions
- fan states
- defogger state
- grow light state
- circulation state

### Diagnostics

- Wi-Fi state
- MQTT state
- OTA enabled state
- LoRa readiness plus queue and drop counters
- filesystem readiness
- uptime

## Compatibility rule

If this payload shape changes in a future commit, the corresponding change set should update this document in the same commit.