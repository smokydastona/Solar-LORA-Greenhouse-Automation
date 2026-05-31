# 5 V Build Guide

## Scope

This is the build sequence for the complete first-generation controller system implemented in this repository.

## Hardware paths before you start

### Current firmware target

- The default firmware configuration now supports the currently owned starter hardware: one DHT22 / AM2302 for air temperature and humidity and two SG90 vent servos.
- In that starter path, BH1750 and DS18B20 stay optional and disabled until those sensors are actually added.
- The configured DHT22 pin is currently [include/PinMap.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/PinMap.h) `TEMP_AIR_DHT` on GPIO 16. That pin choice must still be checked against the exact SX1262 LoRa V3 board pinout before permanent wiring because the board-specific free-pin map has not been physically verified in this repo.

### Fuller upgrade path

- The firmware still supports the fuller BME280 plus BH1750 plus DS18B20 sensor stack.
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
3. Verify the OLED display initializes.
4. Verify the serial console starts at 115200 baud.
5. If Wi-Fi is configured, verify the board joins Wi-Fi and advertises OTA.

## Stage 2: wire the sensor bus

### Current owned-hardware starter path

1. Wire the DHT22 data pin to the configured DHT22 GPIO.
2. Wire DHT22 VCC and GND to the controller 5 V and ground rails.
3. If your DHT22 module is a bare sensor rather than a breakout board, add the correct pull-up resistor specified for that sensor module.
4. Power the controller and verify air temperature and humidity values appear on the display.
5. Leave BH1750 and DS18B20 disabled in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h) until they are physically installed.

### Fuller upgrade path

1. Wire I2C SDA and SCL from the ESP32-S3 to the BME280 and BH1750.
2. Wire 5 V and ground to the BME280 and BH1750.
3. Wire the DS18B20 data line to the configured OneWire pin.
4. Add a 4.7 kOhm pull-up between DS18B20 data and 5 V.
5. Enable those sensors in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h) and verify air temperature, humidity, water temperature, and light values appear on the display.

## Stage 3: add the vent servos

1. Mechanically mount the top and bottom vent servos.
2. Power the servos from a dedicated 5 V rail sourced from the power bank, not from the ESP32 USB pin.
3. Connect both servo grounds to the common ground bus.
4. Connect each servo signal wire to its configured GPIO.
5. Verify both vents move between fully closed and fully open positions.
6. Start with the bought SG90 servos as the current owned-hardware path.
7. Adjust the servo angles in [include/Settings.h](c:/Users/smoky/OneDrive/Desktop/Homemade%20Mods/Mini%20Greenhouse/include/Settings.h) until travel matches the real vent geometry.
8. If the SG90 units chatter, stall, or cannot hold the vents in wind, treat that as a mechanical limit and move to MG90S-class metal-gear servos.

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
6. Check that a log file appears on LittleFS.
7. Let the system run for at least one day before changing thresholds.

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
