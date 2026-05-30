# 5 V Wiring Reference

Reference diagram: [5 V summer architecture](./diagrams/greenhouse-5v-summer-architecture.png)

## Controller pin map

| Function | Pin |
| --- | --- |
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| DS18B20 water probe | GPIO 10 |
| Top vent servo signal | GPIO 3 |
| Bottom vent servo signal | GPIO 4 |
| Exhaust fan MOSFET gate | GPIO 5 |
| Intake fan MOSFET gate | GPIO 6 |
| Defogger MOSFET gate | GPIO 7 |
| Grow-light MOSFET gate | GPIO 11 |
| Circulation fan MOSFET gate | GPIO 12 |
| Mode button | GPIO 13 |
| Force-open button | GPIO 14 |
| Force-close button | GPIO 15 |
| Status LED | GPIO 48 |

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
```

## 5 V branch targets

| Branch | Intended load | Fuse target | Wire target |
| --- | --- | --- | --- |
| Main controller-load feed | Controller rail, sensors, servos, MOSFET supply input | 3 A near source | 18 AWG |
| Sensor and logic branch | BME280, BH1750, DS18B20, buttons, logic rails | 1 A | 22 AWG to 24 AWG |
| Servo feed | Two MG90S-class vent servos on dedicated 5 V rail | 2 A to 3 A | 18 AWG |
| Exhaust branch | One 5 V fan branch or equivalent load | 1 A to 2 A | 18 AWG |
| Intake branch | One 5 V fan branch or equivalent load | 1 A to 2 A | 18 AWG |
| Mini-fan LM2596 subsystem | Separate four-fan mixing branch | 2 A | 18 AWG |

Measure the real final current draw before locking any fuse value into hardware.

## Sensor wiring

### BME280

- VIN -> 5 V
- GND -> GND
- SDA -> GPIO 8
- SCL -> GPIO 9

### BH1750

- VIN -> 5 V
- GND -> GND
- SDA -> GPIO 8
- SCL -> GPIO 9

### DS18B20

- Red -> 5 V
- Black -> GND
- Yellow or white -> GPIO 10
- 4.7 kOhm resistor between 5 V and data line

## Servo wiring

### Top vent servo

- Power red -> 5 V bus
- Ground brown or black -> GND bus
- Signal yellow or orange -> GPIO 3

### Bottom vent servo

- Power red -> 5 V bus
- Ground brown or black -> GND bus
- Signal yellow or orange -> GPIO 4

## Button wiring

Each button uses the ESP32 internal pull-up.

- One terminal -> GND
- Other terminal -> configured GPIO

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
