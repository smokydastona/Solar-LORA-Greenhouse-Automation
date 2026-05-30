# Operations And Safety

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

### Winter

- Prioritize sealing, thermal mass, and moisture control.
- Use the winter dome to reduce heat loss before adding larger electrical loads.
- Add the solar-heater buffer zone before assuming active electric heat is required everywhere.

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

## Troubleshooting overview

### Vents do not move

- Verify the servo rail has real 5 V under load.
- Confirm common ground between the servo rail and the ESP32.
- Re-check configured servo angles.

### Climate values are wrong

- Reposition the BME280 out of direct sun.
- Verify the DS18B20 pull-up resistor.
- Check I2C wiring for the BME280 and BH1750.

### Fans never turn on in auto mode

- Confirm the MOSFET module is wired to the correct branch polarity.
- Warm the BME280 sensor above the configured thresholds.
- Verify the fan GPIO pins match the real hardware.

### Grow light runs at the wrong times

- Verify Wi-Fi and NTP if using scheduled supplemental lighting.
- Verify the light threshold and local time zone in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h).
