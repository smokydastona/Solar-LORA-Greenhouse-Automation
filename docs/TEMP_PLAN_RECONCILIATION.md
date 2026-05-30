# Temp Plan Reconciliation

## Purpose

This note captures what from `docs/temp/` is still useful, what has already been absorbed into the active repo docs, and what should not be treated as current project truth.

## Actionable items kept from the temp plans

- Structural weather-hardening remains valid: anchors, tie-downs, spare clips, immediate cover repair, and routine storm inspection.
- Electronics enclosure discipline remains valid: IP-rated box, downward or sideways cable entries, strain relief, and desiccant.
- The low-voltage solar plus power-bank approach remains valid as a first-generation system boundary.
- Irrigation remains a future expansion path, starting simple and only later integrating with sensors and automation.
- Seasonal operating procedures remain valid in principle: spring recommissioning, summer heat checks, fall storm prep, and winter moisture and snow vigilance.
- Winter protection priorities remain valid in principle: sealing, thermal mass, frost cloth, and careful battery handling before adding larger electrical loads.

## Conflicts with the current repo that should not override active docs

### Greenhouse size and geometry

- The temp `.docx` files describe a much larger Miracle-Gro tunnel-style greenhouse.
- The active repo is for the current smaller greenhouse build and its real vent geometry.
- Do not import the temp-document dimensions, bench layouts, or long-tunnel airflow assumptions into active wiring or firmware docs without measurement.

### Airflow topology

- The temp plans assume a four-fan `two high / two low` USB circulation layout as the main greenhouse airflow scheme.
- The active repo keeps one direct-solar fan independent and uses controller-managed venting plus switched branches for the main automated behavior.
- The separate LM2596 four-fan subsystem in the repo is a mixing branch, not the definition of the main greenhouse control loop.

### Controller path

- The temp plans mention Arduino Nano, ESP8266, and generic DHT-style control paths.
- The active implementation target in this repo is the ESP32-S3 firmware under `src/` with the pin map and thresholds defined in `include/`.
- Treat older controller examples in temp docs as concept sketches only.

### Sensor choices

- The temp plans propose DHT22/SHT31 and generic resistive or capacitive moisture probes.
- The active firmware and docs use BME280, BH1750, and DS18B20 as the first implemented sensor set.
- Any new sensor from the temp plans should be considered optional future work, not an active requirement.

### Solar regulator details

- The temp plans describe using an LM2596 as a solar-to-power-bank charging regulator.
- The active repo already treats the LM2596 primarily as part of the separate mini-fan voltage-reduction branch.
- Do not assume every power-bank charging path in the repo should be redrawn around the temp-plan LM2596 charging scheme without hardware validation.

## Working rule

- Use the temp docs for practical field wisdom and upgrade ideas.
- Use the active repo docs and code as the current source of truth.
- If a temp-doc idea changes firmware behavior, wiring, or safety expectations, update the active docs only after it is adapted to the real greenhouse hardware in this repo.