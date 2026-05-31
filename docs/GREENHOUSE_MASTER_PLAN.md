# Greenhouse Master Plan

## Purpose

This document is the single source of truth for the greenhouse project. It consolidates the current physical build state, the implemented 5 V automation layer, the always-on fan subsystem, the winter dome and solar-heater concept, and the future 12 V power and heating upgrade.

Architecture and diagram status is indexed in [ARCHITECTURE_INDEX.md](./ARCHITECTURE_INDEX.md).

Historical planning material from `docs/temp/` is reconciled in [temp/TEMP_PLAN_RECONCILIATION.md](./temp/TEMP_PLAN_RECONCILIATION.md). Use that note for context, but use this file and the active wiring/build docs as the current source of truth.

Current reference diagrams:

- [5 V summer architecture](./diagrams/greenhouse-5v-summer-architecture.png)
- [Future 5 V plus 12 V winter architecture](./diagrams/greenhouse-5v-plus-12v-winter-architecture.png)

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

- Two 5 V 10 W panels in parallel are the current execution target for the controller-backed layer.
- The power bank acts as the stored-energy buffer for the control system.
- The physical target board is the SX1262 LoRa V3 ESP32-S3 board with onboard 0.96 in OLED, USB-C, and battery pads.
- The current firmware build uses the `esp32-s3-devkitc-1` compatibility profile in PlatformIO until a board-specific environment is needed.
- Two servos drive the top and bottom vent flaps.
- Environmental sensors feed the controller.
- MOSFET-switched outputs handle fan, heater, and light branches.
- Three manual buttons provide simple, low-stress user control for an elderly family member.

### Locked-in 5 V execution details

| Item | Current target |
| --- | --- |
| Controller board | SX1262 LoRa V3 ESP32-S3, onboard OLED, USB-C |
| Current owned-hardware air sensor path | DHT22 / AM2302 supported directly in firmware as the starter air-temperature and humidity sensor |
| Current owned-hardware servo path | 2 x SG90 micro servos supported for initial vent testing, with MG90S-class upgrade reserved if torque proves insufficient |
| Solar input | 2 x 5 V 10 W panels in parallel |
| Energy buffer | USB power bank sized to run the controller layer through cloud cover and short evening gaps |
| Daily 5 V energy target | About 20 Wh to 35 Wh for the brain, sensors, vent servos, and switched 5 V loads before any future winter expansion |
| Power wire target | 18 AWG for main 5 V runs and branch feeds |
| Signal wire target | 22 AWG to 24 AWG for sensors, buttons, and control lines |
| 5 V branch protection target | 3 A main controller-load feed, 2 A mini-fan branch, 1 A logic and sensor branch, individual branch fuse sized to the actual load |

### Actuator and switched-load layer

| Function | Hardware target | Notes |
| --- | --- | --- |
| Top vent actuator | SG90 currently supported; MG90S-class metal-gear servo remains the torque-upgrade target | Dedicated 5 V servo rail, not ESP32 USB power |
| Bottom vent actuator | SG90 currently supported; MG90S-class metal-gear servo remains the torque-upgrade target | Match travel to the real vent linkage |
| Controller-switched fans | 5 V USB fan loads, roughly 0.15 A to 0.25 A each | Exact current must be measured on the final fan model |
| Switched power stage | DAOKI-style 15 A 400 W MOSFET trigger modules, one per branch | Use one module per independently controlled branch |

| Output name | Load target | Switching hardware | Fuse target |
| --- | --- | --- | --- |
| Exhaust branch | 5 V exhaust fan load | 1 MOSFET module | 1 A to 2 A |
| Intake branch | 5 V intake fan load | 1 MOSFET module | 1 A to 2 A |
| Defogger branch | Future low-voltage defogger | 1 MOSFET module | Size to final defogger current |
| Grow-light branch | Supplemental 5 V light only if used in this phase | 1 MOSFET module | Size to final light current |
| Circulation branch | Any controller-switched circulation fan branch | 1 MOSFET module | 1 A to 2 A |

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

## Fault handling and reliability stance

### Current firmware truths

- Manual `AUTO`, `OPEN`, and `CLOSED` modes are implemented.
- CSV logging to internal LittleFS is implemented.
- OTA is optional and only available when Wi-Fi credentials are configured and OTA is enabled.
- The firmware does not currently have battery-voltage awareness, servo-jam detection, or a dedicated watchdog policy.

See [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md) for the explicit boundary between implemented behavior and documented design targets.

### Deployment-safe behavior target

- If the main air sensor is unavailable, the safe policy should be vent-open in daytime and vent-closed at night rather than continuing blind climate control.
- If the water-temperature probe fails, thermal-mass logic should be ignored while the rest of the greenhouse continues operating.
- If battery voltage monitoring is added later, non-critical loads should shed first: grow light, circulation, then fan branches before the controller itself is sacrificed.
- Servo-jam handling requires either current sensing, positional feedback, or another verified detection method; until then, jam detection is not a claimed live feature.

## Sensor set

- DHT22 / AM2302 is now supported as the current owned-hardware air temperature and humidity path
- BME280 remains supported as an air-sensor upgrade path when the user wants the I2C-based sensor stack
- DS18B20 remains the optional water-temperature measurement path for the thermal mass bucket or tote
- BH1750 remains the optional ambient light measurement path
- Optional future sensors: soil moisture, second air-temperature probe, second water-temperature probe, battery-voltage divider

## Human interface requirements

- One display should show current state with no need for an app
- Controls should be understandable without technical knowledge
- A user should not need to open enclosures or touch live terminals
- The only regular interaction should be simple override buttons or one main power switch

## Logging, telemetry, and update stance

- Local logging in v1 is internal flash via LittleFS CSV files.
- No SD card is part of the v1 design.
- No remote telemetry is part of the required v1 greenhouse deployment.
- LoRa capability exists on the target board but is not part of the active first-generation control requirement.
- USB flashing remains the guaranteed firmware update path.
- OTA is optional over Wi-Fi when credentials are configured and the OTA flag is enabled.
- A watchdog-based self-recovery policy is desirable before fully unattended long-term deployment, but it is not yet documented as implemented firmware behavior.

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

## Enclosure and environmental hardening

- The controller and low-voltage switching hardware belong in an IP65-class plastic enclosure.
- Use cable glands where possible; sealed grommets are the fallback, not the first choice.
- Put desiccant packs inside the enclosure and check them during seasonal maintenance.
- Cable entry should face downward or sideways, never upward.
- Mount the enclosure above the coldest splash and drip zone, but still low enough to keep wiring short and serviceable.
- Keep the 5 V brain enclosure physically separate from any future 12 V backbone hardware so the current low-voltage system stays understandable and safe.

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

## Future brain-backup concept

- A small 18650-based 5 V UPS or equivalent backup module is a future reliability upgrade for the controller only.
- Its job would be to keep the ESP32-S3, sensors, and logging alive through short 5 V dips.
- It is not intended to run servos, fans, heaters, or lighting.
- It is not part of the active v1 build and should be treated as deferred until the main controller layer is physically installed and characterized.

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

See [SAFETY_MODEL.md](./SAFETY_MODEL.md) for the one-page subsystem-independence and controller-failure view.

## Project execution order

1. Build the first-generation 5 V controller system and verify vent actuation, sensors, display, and logging.
2. Integrate the four-fan LM2596 always-on mixing subsystem.
3. Tune thresholds through real greenhouse operation.
4. Build the retractable dome concept.
5. Add the buffer-zone solar heater to the dome.
6. Only after those pieces are stable, build the 12 V winter backbone.
