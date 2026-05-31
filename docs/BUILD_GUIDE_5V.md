# 5 V Build Guide

## Scope

This is the build sequence for the complete first-generation controller system implemented in this repository.

## Hardware paths before you start

### Current firmware target

- The default firmware configuration now supports the currently owned starter hardware: one DHT22 / AM2302 for air temperature and humidity and two SG90 vent servos.
- In that starter path, BH1750 and DS18B20 stay optional and disabled until those sensors are actually added.
- The DHT22 data pin is now locked to [include/PinMap.h](../include/PinMap.h) `TEMP_AIR_DHT` on GPIO 16. On the Heltec-style SX1262 LoRa V3 pinout used by your bought board, GPIO 16 is broken out on Header J3 physical pin 17 and is not shown as one of the board's pre-wired OLED, LoRa, LED, or button signals.
- The greenhouse pin map is now remapped around the board's fixed OLED, LoRa, LED, button, and USB-serial reservations so the firmware pin definitions match this board family instead of the old generic devkit assumptions.

### Fuller upgrade path

- The firmware still supports the fuller BME280 plus BH1750 plus DS18B20 sensor stack.
- On this board family, that upgrade path uses the fixed shared I2C bus on GPIO 17 and GPIO 18, which is already used by the onboard OLED.
- MG90S-class metal-gear servos remain the recommended torque upgrade if the real vent linkage is too heavy for SG90 units.

## Before you wire anything

1. Check the greenhouse structure before installing electronics.
2. Re-drive or tighten existing anchors if the frame has shifted.
3. Re-secure any loose cover clips, zip ties, or vent hardware.
4. Pick enclosure and cable routes that stay out of drip paths, runoff, and standing water.
5. If the four-fan LM2596 branch is being installed at the same time, preset the LM2596 output with a multimeter before connecting the fans.

## Stage 1: bench build the controller

1. Flash the ESP32-S3 board with the firmware in this repository.
2. Power the board from USB only.
3. Verify the onboard OLED display initializes.
4. Verify the serial console starts at 115200 baud.
5. If Wi-Fi is configured, verify the board joins Wi-Fi and advertises OTA.

## Stage 2: wire the sensor bus

### Current owned-hardware starter path

1. Wire the DHT22 data pin to the configured DHT22 GPIO.
2. Prefer wiring DHT22 VCC to the controller 3.3 V rail and GND to the common ground bus so the data line stays in the ESP32-S3 logic range.
3. If your DHT22 module is a bare sensor rather than a breakout board, add the correct pull-up resistor specified for that sensor module.
4. If your exact DHT22 breakout requires 5 V power, do not connect its data line directly to GPIO 16 unless the module is documented as 3.3 V logic-safe or you add a level-shift or 3.3 V pull-up arrangement.
5. Power the controller and verify air temperature and humidity values appear on the display. Unavailable sensors now show `N/A` on the display instead of a fake zero reading.
6. Leave BH1750 and DS18B20 disabled in [include/Settings.h](../include/Settings.h) until they are physically installed.

### Optional reliability and telemetry add-ons

1. If you want battery-voltage monitoring, add a safe resistor-divider path from the monitored DC rail to an ADC-capable ESP32-S3 pin.
2. Do not connect any battery or 5 V rail directly to an ESP32 GPIO.
3. Leave the battery monitor disabled in [../include/Settings.h](../include/Settings.h) until the divider, scaling ratio, and target ADC pin are confirmed on your actual board.
4. If you want MQTT or Home Assistant telemetry, configure the broker settings in [../include/Settings.h](../include/Settings.h) before commissioning.

### Fuller upgrade path

1. Wire I2C SDA and SCL from the ESP32-S3 to the BME280 and BH1750.
2. Wire 5 V and ground to the BME280 and BH1750.
3. Wire the DS18B20 data line to the configured OneWire pin.
4. Add a 4.7 kOhm pull-up between DS18B20 data and 5 V.
5. Enable those sensors in [include/Settings.h](../include/Settings.h) and verify air temperature, humidity, water temperature, and light values appear on the display.

## Stage 3: add the vent servos

1. Mechanically mount the top and bottom vent servos.
2. Power the servos from a dedicated 5 V rail sourced from the power bank, not from the ESP32 USB pin.
3. Connect both servo grounds to the common ground bus.
4. Connect each servo signal wire to its configured GPIO.

### Phase A: bench-test travel only

5. Start with the bought SG90 servos as the current owned-hardware path.
6. Leave the default servo angles in [include/Settings.h](../include/Settings.h) at their reduced-travel SG90 bench-test values for the first power-up.
7. Power the system and verify each servo moves only through a small safe range without binding, slamming, or over-centering the linkage.
8. Stop immediately if either servo chatters, stalls, draws the linkage into a hard stop, or twists the vent plastic.

### Phase B: final installed travel

9. After the linkage direction and neutral positions are confirmed safe, expand the open and closed angles gradually in small steps until the real vent geometry is reached.
10. Re-test after each angle change instead of jumping directly to full travel.
11. Only treat the angles as final after the vents reliably hit the intended open and closed positions without stressing the servo or the greenhouse skin.
12. If the SG90 units chatter, stall, or cannot hold the vents in wind even after careful tuning, treat that as a mechanical limit and move to MG90S-class metal-gear servos.

## Stage 4: add switched load outputs

1. Install logic-level MOSFET modules for exhaust fan, intake fan, defogger, grow light, and any controlled circulation fan branch.
2. Use the controller GPIO pins only as logic signals.
3. Feed load power from the 5 V bus through the MOSFET-controlled branch.
4. Verify each output turns on and off by manually forcing conditions in firmware or by heating and cooling the current air sensor in use.

## Stage 5: add the manual override buttons

1. Mount three simple buttons in an easy-to-reach location.
2. Wire one side of each button to ground.
3. Wire the other side of each button to the configured GPIO input.
4. Verify the display changes between `AUTO`, `OPEN`, and `CLOSED` modes.

## Stage 6: build the 5 V solar and power-bank backbone

1. Parallel the 5 V solar panels.
2. Use Schottky diodes if the panels do not already include blocking protection.
3. Feed the combined panel output into the power-bank charge input.
4. Use one power-bank output for the controller board.
5. Use another power-bank output or a breakout branch for the 5 V load rail.
6. Verify the bank both charges in sun and powers the controller at the same time.

### Backbone checks that matter

1. Verify the LM2596 output at the final target voltage before connecting the load side.
2. Add an inline fuse on the fan branch or any custom branch that leaves the enclosure.
3. Keep cable runs short and add strain relief where wires enter any box.
4. Do not assume pass-through charging works on every power bank; test it in daylight before mounting everything permanently.

## Stage 7: install inside the greenhouse

1. Mount the controller board and terminal hardware in a weather-resistant enclosure.
2. Keep the enclosure out of standing water and direct drip paths.
3. Route wiring along frame members and secure it with zip ties.
4. Keep sensor placement honest:
   - Air sensor out of direct sun
   - Light sensor near the roof but not blocked
   - Water sensor submerged in the thermal-mass container
5. Keep the power bank serviceable so it can be replaced easily.
6. Point cable-entry holes downward or sideways, not upward.
7. Add desiccant to the enclosure before final closure.
8. Leave enough slack to remove the power bank and ESP32-S3 for service without cutting wires.

## Stage 8: commission the system

1. Start in `CLOSED` mode and verify both vents shut correctly.
2. Switch to `OPEN` mode and verify both vents open correctly.
3. Return to `AUTO` mode.
4. Warm the air sensor and verify the vents and fan outputs activate.
5. Cool the air sensor and verify the vents close again.
6. Simulate main air-sensor loss and verify the controller falls back conservatively instead of continuing the last blind climate state.
7. Check that a log file appears on LittleFS and includes the `*_available` columns for each optional sensor.
8. Let the system run for at least one day before changing thresholds.
9. If battery monitoring is enabled, verify the reported voltage against a multimeter before trusting the percentage value.
10. If MQTT is enabled, verify that the retained state topic and Home Assistant discovery entities appear as expected.

## Safe-mode and recovery behavior

1. The firmware now records reset reason and tracks repeated failed boots in `Preferences`.
2. Holding both override buttons during boot enters safe mode immediately.
3. Repeated unfinished boots also force safe mode.
4. In safe mode, the controller suppresses climate outputs and keeps the greenhouse in a quiet, inspectable state.
5. Press the mode button while in safe mode to clear the boot-failure counter and reboot after the underlying problem is fixed.

## First-week field checks

1. Recheck every servo horn, linkage screw, and fan mount after the first windy day.
2. Inspect the enclosure interior for condensation after the first cold night and after the first watering session.
3. Verify the active air sensor still reads realistically after a sunny afternoon; move the sensor location or shield if it is heat-soaking.
4. Inspect the vent plastic and cover tie points for rubbing damage caused by repeated servo travel.
5. Confirm the power bank is still gaining charge during sun instead of only discharging all day.

## Tuning order

1. Servo angles
2. Vent temperature thresholds
3. Fan temperature thresholds
4. Humidity thresholds
5. Grow-light thresholds and lighting window
6. Defogger thresholds
