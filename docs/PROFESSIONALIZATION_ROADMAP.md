# Professionalization Roadmap

This document translates the project's current greenhouse-controller implementation into a realistic path toward a more professional, deployable open-source system.

It is intentionally blunt about what already exists, what is missing, and what would materially improve unattended reliability, field serviceability, and open-source credibility.

## Current maturity snapshot

| Area | Current repo state | Professional target | Gap summary |
| --- | --- | --- | --- |
| Reliability | Conservative control plus watchdogs, safe mode, boot tracking, and local logging | Fault-tolerant unattended recovery | Moderate gap |
| Greenhouse intelligence | Threshold-based climate control plus VPD, dew point, frost risk, and crop profiles | Plant-aware optimization | Moderate gap |
| Dashboard and telemetry | OLED, CSV, MQTT, Home Assistant discovery, and starter dashboard docs | Remote dashboard and history | Moderate gap |
| Automation engine | Fixed logic and manual override modes | Rules, schedules, seasonal logic | Major gap |
| Hardware instrumentation | Air, water, and lux optional paths | Power, irrigation, and crop instrumentation | Moderate to major gap |
| Open-source project quality | Strong build docs, wiring, safety docs, CI, tests | Full operator and developer package | Moderate gap |

## What is already professionally credible

- The repo has a coherent system boundary: the 5 V controller layer is separate from the independent direct-solar fan and the future 12 V winter backbone.
- Wiring, build, materials, and safety guidance already exist and are internally consistent.
- Shared control logic is isolated in [../include/ControlLogic.h](../include/ControlLogic.h) with host-side tests in [../test/test_control_logic/test_control_logic.cpp](../test/test_control_logic/test_control_logic.cpp).
- GitHub Actions already build firmware, bundle docs, and publish the rendered docs site.
- The repo now avoids false claims about sensor availability, log integrity, and Pages deployment status.

## What still makes the project feel early-stage

- Solar current, panel voltage, and charge-state telemetry are still missing.
- No LoRa transport is implemented despite the target board including LoRa hardware.
- No Prometheus-style metrics, REST API, or webhook surface exists.
- Dashboard packaging and screenshots are still early.
- Disease-risk modeling remains incomplete beyond the current greenhouse metrics layer.

## Priority order

The fastest route to a project that feels deployable rather than experimental is:

1. Reliability hardening
2. Greenhouse intelligence and plant-state interpretation
3. Remote telemetry and dashboard quality
4. Automation engine and rule system
5. Sensor and power instrumentation expansion
6. Open-source packaging, integration, and contributor ergonomics

## 1. Reliability hardening

### Must-have field reliability work

| Feature | Why it matters | Repo status | Suggested implementation path |
| --- | --- | --- | --- |
| Hardware watchdog | Recovers from hard firmware stalls | Implemented | Keep validation and documentation aligned with real field behavior |
| Software watchdog | Recovers from control-loop lockups | Implemented | Keep timing budget visible and validate behavior under real load |
| Brownout detection | Prevents undefined behavior during power collapse | Not implemented | Add explicit brownout state handling once broader power telemetry exists |
| Safe-mode boot | Prevents reboot loops after bad config or failing peripherals | Implemented | Keep recovery workflow simple and physically understandable |
| Automatic recovery after power loss | Required for unattended service | Partially present | Persist recovery counters and restore last safe mode, thresholds, and service state |
| Sensor failure detection | Avoids blind control | Partially present | Expand from availability flags to stale-data, out-of-range, and disagreement detection |
| LoRa link-loss recovery | Required once remote nodes exist | Not implemented | Add heartbeat timeout, radio reset, and backoff reconnect policy |
| Automatic reboot scheduling | Useful for long unattended uptime | Not implemented | Add configurable maintenance reboot window with boot reason logging |
| Battery health monitoring | Prevents controller death and bad decisions | Partially implemented | Keep calibration honest and expand beyond voltage-only heuristics over time |
| Solar charge monitoring | Distinguishes energy shortage from logic failure | Not implemented | Measure panel voltage and charge-path current where hardware allows |
| Flash write protection | Preserves storage over long life | Partial | Rate-limit writes, rotate logs, and isolate config from telemetry churn |

### Triple-A reliability features

| Feature | Value |
| --- | --- |
| Dual OTA partitions | Allows safe remote firmware updates |
| Rollback after failed update | Prevents soft-bricking during unattended upgrade |
| Remote diagnostics | Reduces site visits |
| Self-test system | Makes commissioning repeatable |
| Node health score | Gives operators a quick trust indicator |
| Predictive maintenance alerts | Surfaces degrading batteries, sensors, and actuators before failure |

### Recommended implementation phases

1. Add battery-voltage input, reset-reason logging, and watchdog instrumentation.
2. Add safe-mode boot and a boot-failure counter in `Preferences`.
3. Move OTA to a documented dual-partition update strategy.
4. Add a self-test command path that verifies sensors, storage, outputs, and time source.

## 2. Greenhouse intelligence

### Immediate intelligence upgrades

| Capability | Why it matters | Repo status |
| --- | --- | --- |
| VPD calculation | Gives plant-relevant humidity guidance | Implemented |
| Dew point calculation | Helps condensation and disease decisions | Implemented |
| Heat index | Improves hot-weather interpretation | Not implemented |
| Frost warning | Important for shoulder-season risk | Implemented |
| Disease risk detection | Makes humidity data actionable | Not implemented |
| Crop profiles | Allows different targets for tomatoes, lettuce, herbs, etc. | Implemented |
| Growth-stage profiles | Makes thresholds seasonal and biological | Not implemented |

### Product-level target

Instead of showing only raw sensor values, the system should be able to say:

- crop state: optimal, acceptable, or stressed
- climate interpretation: VPD poor, good, or ideal
- risk state: frost risk, condensation risk, disease risk
- control forecast: irrigation or ventilation likely needed soon

### Recommended implementation phases

1. Expand the current greenhouse-metrics layer alongside [../include/ControlLogic.h](../include/ControlLogic.h).
2. Keep VPD, dew point, and frost-risk test coverage current as thresholds evolve.
3. Grow crop profile tables and operator documentation together.
4. Add disease-risk heuristics only after the base sensor set is trustworthy.

## 3. Dashboard and telemetry quality

### Current state

- OLED provides local status.
- CSV logs provide local evidence.
- MQTT and Home Assistant discovery now exist.
- Starter dashboard documentation now exists, but richer history and visualization tooling are still incomplete.

### Professional target

| Dashboard page | Needed content |
| --- | --- |
| Overview | greenhouse health score, current alerts, battery state, solar state, controller mode |
| Environment | temperature, humidity, VPD, dew point, light, water temperature, optional CO2 |
| Irrigation | soil zones, valve states, water usage, tank level |
| Power | panel voltage, battery voltage, charging state, runtime estimate |
| Analytics | 24 hour, 7 day, 30 day, and seasonal trend views |

### Recommended implementation phases

1. Add a stable telemetry schema first.
2. Publish telemetry through MQTT or HTTP JSON before building a custom dashboard.
3. Expand the current Home Assistant discovery and dashboard starter path as the first remote UX target.
4. Only build a custom web dashboard after the telemetry model stops changing rapidly.

## 4. Automation engine

### Current state

- The controller uses explicit threshold logic and manual overrides.
- That is correct for v1 safety, but it is not yet an automation engine.

### Professional target

| Capability | Repo status |
| --- | --- |
| Rules engine | Not implemented |
| Conditions and predicates | Not implemented |
| Schedules | Partially present for grow light only |
| Seasonal adjustments | Not implemented |
| Crop-specific logic | Not implemented |
| Emergency overrides | Manual local overrides implemented; remote and policy overrides absent |

### Recommended implementation phases

1. Preserve the current threshold logic as the safe fallback path.
2. Add a declarative rule evaluation layer on top of it.
3. Allow policies such as battery-healthy, water-available, daylight-present, and maintenance-lockout.
4. Keep every advanced automation path reversible back to the current simple control mode.

## 5. Hardware instrumentation expansion

### Current minimum covered in repo

- Air temperature and humidity
- Optional water temperature
- Optional light level

### Triple-A instrumentation target

| Sensor or input | Value |
| --- | --- |
| Multiple soil probes | Better irrigation decisions across zones |
| Soil temperature | Root-zone awareness |
| CO2 | Better enclosed-space interpretation |
| PAR sensor | Crop-relevant light metric |
| Lux sensor | Already partially covered through BH1750 path |
| Wind sensor | Better vent safety decisions |
| Rain sensor | Better vent and watering logic |
| Water flow sensor | Detects irrigation success and leaks |
| Water tank level sensor | Required before automated irrigation |
| Battery and panel sensing | Required before serious autonomy |

### Recommended hardware expansion order

1. Battery voltage
2. Water tank level
3. Soil moisture zones
4. Water flow confirmation
5. PAR or improved light sensing
6. CO2 and weather-exposed sensors only after enclosure and condensation control are mature

## 6. Open-source project quality

### Already present

- Architecture diagrams
- Wiring guides
- Materials list
- Build guide
- Safety model
- CI workflows
- Host-side logic tests

### Missing or incomplete

| Artifact | Status |
| --- | --- |
| Troubleshooting guide | Added now, but field content still needs growth |
| Developer guide | Added now, but should expand with architecture and release workflow over time |
| API or telemetry contract | Present, but should evolve carefully with release discipline |
| Screenshots of real hardware and dashboard | Missing |
| CAD and PCB files | Missing |
| Home Assistant integration docs | Present as a starter layer, but still light |
| MQTT schema docs | Present |
| Hardware-in-the-loop validation plan | Missing |

## Recommended 90-day roadmap

### Phase 1: make unattended operation safer

- Add watchdog and restart-reason logging.
- Add battery-voltage sensing and low-power load shedding policy.
- Add safe-mode boot and a self-test screen.
- Expand `FIRMWARE_LIMITATIONS.md` only after those behaviors are verified.

### Phase 2: make the greenhouse smarter

- Add VPD, dew point, heat index, and frost warning calculations.
- Add crop profiles and growth-stage targets.
- Update OLED and logs to show interpreted state, not just raw values.

### Phase 3: make the project inspectable remotely

- Expand MQTT telemetry and keep the contract stable.
- Grow Home Assistant integration and documented dashboards.
- Add remote diagnostics and node health summary.

### Phase 4: make the repo feel production-grade

- Add screenshots, stronger release-note discipline, and commissioning evidence.
- Add a hardware-in-the-loop validation plan.
- Add contribution and issue-triage norms if outside contributors begin arriving.

## Decision rules for future work

- Reliability work outranks feature breadth.
- Any remote-control feature must fail safe when Wi-Fi or LoRa is absent.
- Any irrigation feature should wait until water-level and flow-confirmation telemetry exist.
- Any plant-intelligence feature should state clearly whether it is heuristic guidance or closed-loop automation.
- Documentation should never advertise a feature before firmware behavior and commissioning notes both confirm it.