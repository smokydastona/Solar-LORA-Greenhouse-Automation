# Operations And Safety

See [SAFETY_MODEL.md](./SAFETY_MODEL.md) for the quick reference showing which subsystems remain independent if the controller-backed layer fails.

## Day-to-day use

- Leave the controller in `AUTO` mode for normal operation.
- Use `OPEN` mode only when you intentionally want emergency venting or hands-on access.
- Use `CLOSED` mode only for servicing, storm prep, or a specific cold-night hold.

## Safe-use rules for non-technical users

- Do not open the controller box during normal use.
- Do not unplug random USB leads; only use the labeled master power path.
- If the greenhouse must be shut down, use one clearly labeled main disconnect or unplug the power-bank output in the designated service location.
- Keep children and pets away from loose wires, vent linkages, and any heater hardware.

## Environmental placement rules

- Keep the controller box above standing water.
- Keep high-current splices out of direct sun and away from drip lines.
- Keep the power bank serviceable but sheltered.

## Seasonal operating guidance

### Summer

- Prioritize vent travel and airflow.
- Verify both vent fans shut down at night.
- Watch for plant stress from direct fan blast and adjust fan angles rather than increasing raw speed.
- Check bank state after stretches of overcast weather and reduce nonessential loads before the controller browns out.
- Inspect cover seams, vent flaps, and cable ties regularly because summer UV is hard on plastics.

### Winter

- Prioritize sealing, thermal mass, and moisture control.
- Use the winter dome to reduce heat loss before adding larger electrical loads.
- Add the solar-heater buffer zone before assuming active electric heat is required everywhere.
- Keep condensation off electronics with desiccant, enclosure checks, and careful cable-entry orientation.
- Treat wet snow and winter wind as structure hazards first and plant hazards second.

## Seasonal checklist additions

### Spring recommissioning

1. Inspect the cover for pinholes, tears, or brittle spots before powering the automation layer.
2. Recheck anchor tension, vent hardware, and fan mounts after winter weather.
3. Verify the LM2596 and power-bank path with a meter before reconnecting fans or controllers.
4. Flush any irrigation lines before using them near electronics or plant benches.

### Summer watch items

1. Run a daily quick check during heat events: vents moving, fans spinning, no direct-solar branch failure.
2. Patch new cover damage immediately instead of waiting for it to spread.
3. Watch for overheating above the normal operating band and add shading before increasing electrical complexity.

### Fall storm prep

1. Re-tighten anchors, tie-downs, and any frame bracing before the wet windy season.
2. Drain or simplify irrigation before frost risk and long wet spells.
3. Inspect all zipper paths, seals, and vent closures before switching into colder-weather operation.

### Winter survival checks

1. Clear pooled rain or snow load before it distorts the frame.
2. Keep the north side insulated if the winter shell or bubble-wrap layer is installed.
3. Remove standing water from trays and saucers before freeze nights.
4. Bring lithium power storage indoors if temperatures become unsafe for the battery chemistry in use.

## 12 V safety rules for the future phase

- Fuse the battery immediately at the positive terminal.
- Do not operate unfused branch wiring in a shed or greenhouse.
- Use a proper battery with built-in protection.
- Treat defoggers as significant current loads, not casual accessories.

## Maintenance checklist

1. Inspect all vent-linkage screws and servo horns monthly.
2. Check that no wire insulation is rubbing through on the frame.
3. Confirm the sensor values still look believable.
4. Verify the log file is still being written.
5. Re-test manual override buttons after any wiring change.
6. Check the enclosure for condensation, corrosion, and saturated desiccant.
7. Recheck structural anchors and cover clips after major wind events.
8. Inspect any drip or watering hardware so leaks do not migrate into the electronics zone.

## Emergency response guide

### Heat emergency

- Force vents open.
- Confirm intake and exhaust airflow.
- Add temporary shade before adding new powered hardware.

### Wind storm warning

- Put the greenhouse in the safest stable mode for the weather.
- Check anchors, vent latches, and any removable accessories.
- Remove or secure anything that can flap, snag, or turn into a sail.

### Hard-freeze warning

- Prioritize sealing, frost cloth, and thermal mass first.
- Move genuinely frost-tender plants indoors instead of assuming the electrical layer will save them.
- Protect the battery and controller from condensation and low-temperature abuse.

## Troubleshooting overview

### Vents do not move

- Verify the servo rail has real 5 V under load.
- Confirm common ground between the servo rail and the ESP32.
- Re-check configured servo angles.

### Climate values are wrong

- Reposition the active air sensor out of direct sun.
- If using the current owned-hardware path, re-check the DHT22 wiring and verify the chosen GPIO is actually free on the exact board.
- If using the fuller sensor stack, verify the DS18B20 pull-up resistor.
- If using the fuller sensor stack, check I2C wiring for the BME280 and BH1750.

### Fans never turn on in auto mode

- Confirm the MOSFET module is wired to the correct branch polarity.
- Warm the active air sensor above the configured thresholds.
- Verify the fan GPIO pins match the real hardware.

### Grow light runs at the wrong times

- Verify Wi-Fi and NTP if using scheduled supplemental lighting.
- Verify the light threshold and local time zone in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h).
