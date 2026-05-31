# Power Telemetry Roadmap

This document defines the realistic path from the current battery-voltage-only implementation to a more serious off-grid power telemetry model.

It is tied to the actual system shapes already documented in this repository:

- current active system: 5 V solar plus power-bank controller build
- future system: 12 V winter backbone with MPPT controller and protected battery storage

## Current Repo Reality

Implemented now:

- onboard battery voltage sensing through the Heltec board battery-read path
- low and critical battery thresholds in firmware
- battery state exposure in the MQTT payload when configured

Not yet implemented:

- solar panel voltage telemetry
- solar current telemetry
- battery current telemetry
- charge-state telemetry beyond inferred battery percentage
- daily harvested energy accounting
- daily consumed energy accounting
- runtime estimation based on measured load

## Why This Needs Staged Hardware

The current 5 V system uses solar panels feeding a USB power-bank layer. That makes power telemetry harder than a simple bare-battery ADC measurement because:

- many consumer power banks hide charging internals
- the controller may not have direct access to panel voltage or current
- runtime estimation is weak if current draw is not measured at the load path

The future 12 V system is better suited to serious telemetry because its architecture is explicit:

- shed PV array
- MPPT controller
- 12 V battery
- fused branches
- 12 V to 5 V controller buck

## Phase 1: Current 5 V System Hardening

### Goal

Make the present 5 V controller build more power-aware without pretending it is already a full solar monitor.

### Existing hardware used

- Heltec-style SX1262 LoRa V3 ESP32-S3 board
- onboard `VBAT_Read` path
- existing 5 V solar panels
- existing USB power bank

### Telemetry available in this phase

- battery voltage
- battery low flag
- battery critical flag
- coarse battery percentage

### Remaining limits in this phase

- no trustworthy solar harvest figure
- no measured load current
- no true charge state from the power bank internals

## Phase 2: Add Explicit 5 V Instrumentation

### Goal

Instrument the 5 V power path that currently sits between the solar source, the power-bank layer, and the controller-backed loads.

### Planned hardware additions

- one panel-voltage divider path sized for the actual panel open-circuit voltage
- one current-sense stage on the controller-backed 5 V load branch
- one current-sense stage on the solar-to-charge path if the chosen wiring path exposes it safely

### Telemetry targets

- solar input voltage
- solar input current
- 5 V load current
- approximate charging vs discharging state
- estimated watt draw of the controller-backed branch

### Design rule

Do not add a telemetry path that compromises the current low-voltage safety posture or creates an unsafe battery or panel measurement hack inside the greenhouse.

## Phase 3: Add Runtime And Daily Energy Estimation

### Goal

Turn raw voltage and current data into operator-useful power information.

### Derived values to add

- estimated runtime remaining under current load
- daily watt-hours harvested
- daily watt-hours consumed
- simple charge-state classification: charging, idle, discharging, low reserve

### Firmware work required

- add a power-sample accumulation window
- reset daily counters on a stable day boundary
- surface derived values over MQTT and logs
- rate-limit flash writes so power telemetry does not consume storage life carelessly

## Phase 4: Future 12 V Winter Telemetry

### Goal

Move serious power telemetry to the future winter architecture where the electrical system is explicit and measurable.

### Planned hardware already documented elsewhere in repo

- 300 W to 400 W PV array on shed roof
- MPPT charge controller
- 12 V LiFePO4 battery with low-temperature cutoff or AGM fallback
- fused distribution block
- 12 V fan, defogger, grow-light, and buck-converter branches

### Telemetry targets for this phase

- array voltage
- array current
- controller charge state
- battery voltage
- battery current
- branch-level current for the 12 V to 5 V controller feed if practical
- daily harvested and consumed energy
- runtime estimate under winter load profile

### Why This Phase Matters

This is the first architecture stage where the repo can realistically claim professional off-grid telemetry instead of a battery-voltage proxy.

## Recommended Implementation Order

1. Keep the current onboard battery telemetry accurate and calibrated.
2. Add safe 5 V current and voltage instrumentation only where the current architecture exposes them cleanly.
3. Add derived runtime and daily energy counters after current measurement exists.
4. Treat the future 12 V system as the real long-term home for serious solar telemetry.

## Companion Docs

- [WIRING_5V.md](./WIRING_5V.md)
- [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md)
- [MATERIALS_LIST.md](./MATERIALS_LIST.md)
- [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md)
