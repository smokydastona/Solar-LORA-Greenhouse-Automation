# 5 V Wiring Reference

Reference diagram: [5 V summer architecture](./diagrams/greenhouse-5v-summer-architecture.png)

## Controller pin map

| Function | Pin |
| --- | --- |
| I2C SDA | GPIO 17 |
| I2C SCL | GPIO 18 |
| OLED reset | GPIO 21 |
| OLED Vext power control | GPIO 36 active LOW |
| DHT22 air sensor data | GPIO 16 verified on Header J3 pin 17 |
| DS18B20 water probe | GPIO 15 |
| Top vent servo signal | GPIO 7 |
| Bottom vent servo signal | GPIO 6 |
| Exhaust fan MOSFET gate | GPIO 5 |
| Intake fan MOSFET gate | GPIO 42 |
| Defogger MOSFET gate | GPIO 41 |
| Grow-light MOSFET gate | GPIO 40 |
| Circulation fan MOSFET gate | GPIO 39 |
| Mode button | GPIO 47 |
| Force-open button | GPIO 38 |
| Force-close button | GPIO 33 |
| Status LED | GPIO 35 onboard LED |

## Controller electrical topology

```text
5 V solar panels (parallel)
    -> blocking diode(s) if needed
    -> power bank charge input

power bank output #1
    -> ESP32-S3 USB port

power bank output #2 or breakout branch
    -> 5 V terminal block
        -> sensors V+
        -> servo V+
        -> MOSFET-switched 5 V loads

all grounds
    -> shared ground bus
        -> ESP32 GND
        -> sensor GND
        -> servo GND
        -> MOSFET module GND
        -> load negative return

board-reserved wiring
    -> onboard OLED I2C on GPIO 17 and GPIO 18
    -> OLED reset on GPIO 21
    -> OLED Vext enable on GPIO 36, active LOW
    -> onboard SX1262 LoRa on GPIO 8 to 14
    -> onboard LED on GPIO 35
    -> onboard USER button on GPIO 0
```

## 5 V branch targets

| Branch | Intended load | Fuse target | Wire target |
| --- | --- | --- | --- |
| Main controller-load feed | Controller rail, sensors, servos, MOSFET supply input | 3 A near source | 18 AWG |
| Sensor and logic branch | DHT22 starter path or BME280 plus BH1750 plus DS18B20 upgrade path, buttons, logic rails | 1 A | 22 AWG to 24 AWG |
| Servo feed | Two SG90 vent servos for the current owned-hardware path, with MG90S-class as the torque-upgrade path | 2 A to 3 A | 18 AWG |
| Exhaust branch | One 5 V fan branch or equivalent load | 1 A to 2 A | 18 AWG |
| Intake branch | One 5 V fan branch or equivalent load | 1 A to 2 A | 18 AWG |
| Mini-fan LM2596 subsystem | Separate four-fan mixing branch | 2 A | 18 AWG |

Measure the real final current draw before locking any fuse value into hardware.

## Current firmware target versus currently owned hardware

### Current owned-hardware path

- Air temperature and humidity: DHT22 / AM2302 on the configured digital input.
- Vent actuators: 2 x SG90 servos on the dedicated 5 V servo rail.
- Optional sensors not yet required for this starter path: BH1750 and DS18B20.

### Fuller upgrade path

- Air temperature and humidity: BME280 on I2C.
- Light sensing: BH1750 on I2C.
- Water temperature: DS18B20 on OneWire.
- Vent actuators: MG90S-class metal-gear servos if the vent linkage needs more torque.

## Sensor wiring

### DHT22 / AM2302

- VCC -> 3.3 V preferred on the ESP32-S3 controller path
- GND -> GND
- DATA -> GPIO 16
- If using a bare sensor instead of a module, add the pull-up resistor required by that part and pull the data line up to 3.3 V
- If your breakout board requires 5 V power, do not direct-connect its data line to GPIO 16 unless the breakout is explicitly documented as 3.3 V logic-safe or you add level shifting
- GPIO 16 is verified as a broken-out header pin on this Heltec-style SX1262 LoRa V3 family and is not listed in the board manual's onboard OLED, LoRa, LED, or user-button wiring table

### Board-specific warning

- This exact board family reserves several pins for onboard LoRa and OLED wiring.
- GPIO 16 is safe to lock for the DHT22 path based on the published pinout image and wiring table.
- The greenhouse pin map in this repo is now remapped around those onboard reservations for this board family.

### BME280

- VIN -> 5 V
- GND -> GND
- SDA -> GPIO 17 shared board I2C bus
- SCL -> GPIO 18 shared board I2C bus

### BH1750

- VIN -> 5 V
- GND -> GND
- SDA -> GPIO 17 shared board I2C bus
- SCL -> GPIO 18 shared board I2C bus

### DS18B20

- Red -> 5 V
- Black -> GND
- Yellow or white -> GPIO 15
- 4.7 kOhm resistor between 5 V and data line

## Servo wiring

Current owned-hardware path: use the bought SG90 units first.

Upgrade path: move to MG90S-class metal-gear servos if the vent linkage proves too stiff, too windy, or too abusive for SG90 torque.

### Top vent servo

- Power red -> 5 V bus
- Ground brown or black -> GND bus
- Signal yellow or orange -> GPIO 7

### Bottom vent servo

- Power red -> 5 V bus
- Ground brown or black -> GND bus
- Signal yellow or orange -> GPIO 6

## Button wiring

Each button uses the ESP32 internal pull-up.

- One terminal -> GND
- Other terminal -> configured GPIO

Current board-specific button map:

- `AUTO` -> GPIO 47
- `FORCE OPEN` -> GPIO 38
- `FORCE CLOSED` -> GPIO 33

## MOSFET branch wiring

For each controlled 5 V branch:

- Branch +5 V -> load positive
- Load negative -> MOSFET drain or switched negative terminal
- MOSFET source -> ground bus
- MOSFET gate or IN -> configured GPIO
- MOSFET module ground -> ground bus
- Use one DAOKI-style 15 A 400 W MOSFET trigger module per independently switched branch.

## Four-fan LM2596 subsystem

The four handheld mixing fans are deliberately separate from the brain.

```text
5 V 10 W solar panel
    -> power bank input

power bank 5 V output
    -> optional 2 A inline fuse
    -> LM2596 input

LM2596 output adjusted to about 3.0 V
    -> fan 1 positive
    -> fan 2 positive
    -> fan 3 positive
    -> fan 4 positive

LM2596 ground output
    -> fan 1 negative
    -> fan 2 negative
    -> fan 3 negative
    -> fan 4 negative
```

## Wiring rules that matter

- Do not power servos from the ESP32 USB pin.
- Do not leave any load branch without a common ground reference back to the controller.
- Keep the controller box dry and the power bank replaceable.
- Label the load branches physically once wired.
- Use 18 AWG for any branch that actually carries actuator or fan current.
- Treat 22 AWG to 24 AWG as signal and sensor wire only.
- Treat GPIO 16 as the locked DHT22 data pin on this board family.
- Keep GPIO 8 to 14, GPIO 17 to 18, GPIO 21, GPIO 35, GPIO 36, GPIO 43, and GPIO 44 reserved for the board's onboard functions unless you intentionally redesign around them.
