# Remote Telemetry Contract

This document defines the current remote telemetry surface implemented by the greenhouse controller firmware.

It exists so dashboards, Home Assistant, and any future LoRa or cloud bridge can target a stable state model instead of reverse-engineering ad hoc serial output.

## Transport

- Primary transport: MQTT over Wi-Fi
- Secondary transport: raw point-to-point LoRa over the onboard SX1262 using RadioLib
- MQTT client library: PubSubClient
- State publishing: retained JSON payloads
- Availability: retained `online` or `offline` topic
- Home Assistant integration: MQTT discovery payloads published automatically on broker connection when enabled
- LoRa status exposure: session id plus queue depth, queue-warning state, sent, acknowledged, ACK timeout, dropped, duplicate, invalid, RSSI, SNR, and last-error counters are included in the JSON state payload
- Diagnostics exposure: recent persisted fault history, per-sensor read-error counters, and servo timing/block counters are included in the JSON state payload and climate CSV log

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
- `enableInboundModeCommands`

LoRa transport configuration lives beside it in [../include/Settings.h](../include/Settings.h) under `Settings::LORA`.

Diagnostics thresholds live in [../include/Settings.h](../include/Settings.h) under `Settings::DIAGNOSTICS`.

## Topics

Assuming the default `baseTopic` is `greenhouse/mini`:

| Topic | Purpose |
| --- | --- |
| `greenhouse/mini/state` | retained full-controller JSON state |
| `greenhouse/mini/availability` | retained availability state: `online` or `offline` |
| `greenhouse/mini/command/mode/set` | inbound mode command topic accepting only `AUTO`, `OPEN`, or `CLOSED` |
| `greenhouse/mini/command/mode/state` | retained current mode state for dashboards and Home Assistant select entities |
| `greenhouse/mini/command/mode/result` | retained result of the last inbound mode command |

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
    "sensor": "VBAT_Read",
    "adc_ctrl_pin": 37,
    "raw_mv": 0,
    "voltage_v": 0.0,
    "percent": 0,
    "band": "UNAVAILABLE",
    "low": false,
    "critical": false
  },
  "health": {
    "score": 100
  },
  "diagnostics": {
    "recent_fault_count": 1,
    "recent_fault": {
      "code": "LORA_QUEUE_WARN",
      "detail": 6,
      "at_ms": 93021,
      "boot_count": 14
    },
    "sensor_errors": {
      "air": 0,
      "bme": 0,
      "dht": 0,
      "light": 2,
      "water": 0
    },
    "servo": {
      "blocked_commands": 0,
      "last_move_ms": 1200,
      "longest_move_ms": 1200
    }
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
  "commands": {
    "mode": {
      "enabled": true,
      "requested": "AUTO",
      "status": "NONE",
      "applied": "AUTO",
      "received_at_ms": 0
    }
  },
  "lora": {
    "session_id": 123456789,
    "queue_depth": 0,
    "queue_warning_active": false,
    "queue_warnings": 0,
    "max_queue_depth": 1,
    "last_queue_warning_at_ms": 0,
    "sent": 0,
    "acknowledged": 0,
    "ack_timeouts": 0,
    "dropped": 0,
    "last_dropped_sequence": 0,
    "last_dropped_at_ms": 0,
    "duplicate_inbound": 0,
    "invalid_inbound": 0,
    "last_rssi_dbm": -77,
    "last_snr_db": 7.5,
    "last_error_code": 0
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
- `sensor` and `adc_ctrl_pin` identify the exact existing onboard measurement path in use today
- `raw_mv` is the averaged ADC-side reading before divider scaling and calibration offset are applied
- voltage and percent should not be treated as trusted operational battery truth until both `available` and `calibrated` are `true`
- `band` is an operator-facing summary with values such as `UNAVAILABLE`, `UNCALIBRATED`, `NORMAL`, `LOW`, and `CRITICAL`

### Sensor freshness

- `air.age_ms`, `water.age_ms`, and `light.age_ms` report how long it has been since the last valid reading for each sensor surface
- once a sensor exceeds its configured freshness window, `available` flips to `false` and the controller stops treating the cached value as live input

### `health`

`score` is a simple controller-health heuristic based on sensor availability, filesystem readiness, safe mode, battery state, expected connectivity, and whether the LoRa queue is under pressure.

It is intended as an operator summary, not a formal reliability metric.

### `diagnostics`

- `recent_fault_count`: number of persisted recent diagnostic events currently retained in the controller ringbuffer
- `recent_fault.code`: one of `AIR_UNAVAILABLE`, `AIR_RECOVERED`, `LIGHT_READ_FAIL`, `WATER_READ_FAIL`, `LORA_QUEUE_WARN`, `LORA_DROP`, `SERVO_BLOCKED`, `SERVO_SLOW`, or `SAFE_MODE`
- `recent_fault.detail`: integer detail field associated with the most recent event, such as queue depth, fault count, safe-mode reason enum, or servo move duration in milliseconds
- `recent_fault.at_ms`: controller `millis()` when that event was recorded
- `recent_fault.boot_count`: boot count associated with the recorded event
- `sensor_errors`: cumulative read-failure counters for the active sensor paths in the current boot session
- `servo.blocked_commands`: count of move requests rejected because the servos were already moving or still cooling down
- `servo.last_move_ms` and `servo.longest_move_ms`: servo drive timing diagnostics captured from the firmware-side protection window

These diagnostics are intended for field troubleshooting and commissioning. They are not a substitute for physical inspection when a vent or sensor branch is behaving unsafely.

### `lora`

- `queue_warning_active`: `true` when the queued frame count is at or above the configured warning threshold
- `queue_warnings`: number of times the queue crossed into warning state during the current boot session
- `max_queue_depth`: deepest observed queue depth during the current boot session
- `last_queue_warning_at_ms`: `millis()` timestamp of the most recent warning-state transition
- `last_dropped_sequence`: most recent sequence number dropped because the queue was full or retries were exhausted
- `last_dropped_at_ms`: `millis()` timestamp of that most recent dropped frame

### `power_budget`

- `servo_moves`: whether the controller is currently allowed to issue new vent servo moves
- `vent_fans`: whether controller-backed intake and exhaust fans are allowed to run
- `defogger`, `grow_light`, `circulation`: whether those branches are currently allowed to energize under the active battery policy

### `commands.mode`

- `enabled`: whether inbound MQTT mode control is enabled in settings
- `requested`: the last received command payload after parsing
- `status`: `NONE`, `ACCEPTED`, `REJECTED_INVALID_PAYLOAD`, `REJECTED_TOPIC`, or `REJECTED_SAFE_MODE`
- `applied`: the accepted control mode associated with that command path
- `received_at_ms`: millis timestamp of the last processed inbound mode command

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

- battery voltage
- LoRa queue depth
- LoRa acknowledged count
- LoRa ACK timeout count
- safe-mode active flag
- battery low and battery critical flags
- LoRa ready flag
- an MQTT/Home Assistant select entity for remote control mode selection

The current integration accepts only remote mode changes. It does not accept direct actuator commands.

The new diagnostics fields are currently carried only in the retained JSON state payload and the local CSV logs. They are not yet published as dedicated Home Assistant discovery entities.

## LoRa on-air contract

The firmware builds a compact ASCII telemetry payload for the LoRa link service and wraps it in a raw on-air frame with explicit ACK behavior.

The compact telemetry payload still includes:

- node id
- mode
- safe-mode flag
- health score
- air temperature and humidity when available
- battery voltage and percentage when available

Telemetry frames are sent over the air in this shape:

- `T|session_id|sequence|payload_crc32|require_ack|node_id|payload`

The receiver is expected to ACK a valid frame with:

- `A|session_id|sequence|payload_crc32`

Each queued frame carries a boot-scoped session id, a sequence number, and a CRC32 computed over the compact payload. The link service treats a frame as delivered only when the matching ACK is received within the configured timeout and can reject duplicate or invalid inbound frames through its validation window.

The firmware now uses the onboard SX1262 through RadioLib. You must still configure a legal frequency and validate the peer receiver path for your region before relying on the radio link in unattended deployment.

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
- LoRa readiness plus queue, acknowledged, ACK timeout, RSSI, SNR, and error counters
- filesystem readiness
- last remote command result
- uptime

## Compatibility rule

If this payload shape changes in a future commit, the corresponding change set should update this document in the same commit.