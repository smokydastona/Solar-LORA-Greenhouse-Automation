# Greenhouse Master Plan

## Purpose

This document is the single source of truth for the greenhouse project. It consolidates the current physical build state, the implemented 5 V automation layer, the always-on fan subsystem, the winter dome and solar-heater concept, and the future 12 V power and heating upgrade.

## Current physical greenhouse state

- Base structure: small Miracle-Gro style greenhouse, approximately 4 ft x 2 ft x 6 ft
- Anchoring: 16 steel U-shaped landscaping ties driven into the ground
- Shelving: secured in place with zip ties
- Vent openings: two vents are installed
- Air path intent: low rear intake plus high front exhaust
- Existing active airflow: at least one 5 V USB fan already direct-connected to a solar panel
- Wax pistons: rejected permanently for this greenhouse because the vent geometry and scale do not make them practical
- Battery storage: available but not yet part of the physical greenhouse as-shipped state
- Control brain: not physically installed yet in the greenhouse, but implemented in this repository

## Final 5 V phase architecture

The first-generation automation system is split into two layers.

### Layer 1: independent direct-solar cooling

- The existing direct-solar 5 V USB fan remains independent.
- It runs when sun is strong enough to create overheating.
- It does not depend on the controller, battery state, or firmware.

### Layer 2: the controller-backed 5 V greenhouse system

- Two or more 5 V solar panels feed a USB power bank.
- The power bank acts as the stored-energy buffer for the control system.
- An ESP32-S3 board is the greenhouse brain.
- Two servos drive the top and bottom vent flaps.
- Environmental sensors feed the controller.
- MOSFET-switched outputs handle fan, heater, and light branches.
- Three manual buttons provide simple, low-stress user control for an elderly family member.

## Firmware control behavior

### Automatic mode

- Open both vents when air temperature rises above the vent-open threshold.
- Close both vents when air temperature falls below the vent-close threshold.
- Run intake and exhaust fans when temperature or humidity crosses the fan thresholds and it is daytime.
- Shut the vent fans down at night.
- Enable the defogger only on cold nights if the feature is installed and wired.
- Enable the grow light only inside the configured lighting window and only when measured light is insufficient.
- Run circulation fans during daytime mixing conditions when configured.

### Force-open mode

- Open both vents immediately.
- Turn on intake and exhaust fans.
- Leave grow light and heater off.

### Force-closed mode

- Close both vents immediately.
- Turn off all climate outputs.
- Keep the greenhouse in a stable, quiet, safe state.

## Sensor set

- BME280 for air temperature and humidity
- DS18B20 for water-temperature measurement in the thermal mass bucket or tote
- BH1750 for ambient light measurement
- Optional future sensors: soil moisture, second air-temperature probe, second water-temperature probe, battery-voltage divider

## Human interface requirements

- One display should show current state with no need for an app
- Controls should be understandable without technical knowledge
- A user should not need to open enclosures or touch live terminals
- The only regular interaction should be simple override buttons or one main power switch

## Four handheld mini fans subsystem

The handheld battery fans are treated as a separate continuous-mixing subsystem, not a brain-controlled subsystem.

- Source power: 5 V solar panel plus power bank
- Conversion: one LM2596 adjustable buck converter set to about 3.0 V
- Load arrangement: all four mini fans wired in parallel to the LM2596 output
- Goal: broad internal air mixing that effectively never stops under normal local weather, given adequate sun and battery recharge

## Greenhouse mounting intent for the four mini fans

- Two high-mounted fans to stir the upper hot and humid layer
- Two low-mounted fans to sweep cool air across the lower plant zone
- Electronics mounted low in a weather-resistant box, with the buck converter elevated and protected from splash and condensation

## Winter dome concept

The winter enclosure is a second shell over the greenhouse, not a replacement skin.

- The dome attaches to the back fence.
- The winter version should cover the greenhouse from top to bottom.
- The hinge point should be low enough that the dome seals over the structure, not merely over the roof.
- The summer version should retract so the greenhouse can breathe freely.

### Preferred retractable structure

- Accordion-style segmented shell attached to the fence
- Wider than the greenhouse on both sides
- Capable of folding back in summer
- Capable of sealing around the greenhouse footprint in winter

## Solar heater concept

The solar heater is part of the dome system, not inside the plant volume.

- It belongs inside the dome and outside the greenhouse skin.
- It mounts on the south-facing dome section.
- It uses the clear dome material as its glazing layer.
- It warms the buffer air layer between the dome and the greenhouse.
- It reduces heat loss from the greenhouse without requiring extra holes in the greenhouse plastic.

## Thermal mass strategy

- Use a dark water bucket or tote as the main thermal battery.
- Place the water mass low in the greenhouse where it receives winter sun when possible.
- Use the water sensor to understand how much heat is being stored.
- In the future 12 V phase, combine thermal mass with defogger and supplemental light rather than trying to brute-force space heating.

## 12 V future upgrade summary

The 12 V expansion is not the first build step. It is the second-generation winter system.

### What it adds

- 300 W to 400 W solar array on the shed roof
- MPPT charge controller
- 12 V 60 Ah to 100 Ah LiFePO4 battery with low-temperature charge protection
- AGM fallback option if simpler cold-weather behavior is preferred
- 12 V fused distribution
- One defogger to start, wiring sized for up to two
- Winter supplementary grow light
- Separate 12 V to 5 V buck supply for the controller if the controller is later migrated onto the 12 V backbone

### What it does not replace

- The physical vent and sensor strategy
- The need for thermal mass
- The value of the winter dome and solar buffer heater

## Safety principles

- No exposed high-current terminals
- Every branch fused near the source
- Shared low-voltage ground arranged intentionally, not through random frame contact
- Battery and power electronics housed in an enclosure
- One obvious master shutoff for non-technical use

## Project execution order

1. Build the first-generation 5 V controller system and verify vent actuation, sensors, display, and logging.
2. Integrate the four-fan LM2596 always-on mixing subsystem.
3. Tune thresholds through real greenhouse operation.
4. Build the retractable dome concept.
5. Add the buffer-zone solar heater to the dome.
6. Only after those pieces are stable, build the 12 V winter backbone.
