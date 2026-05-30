# Materials List

## Already owned or already discussed as on-hand

| Item | Status | Notes |
| --- | --- | --- |
| Small greenhouse | Existing | Base structure |
| 16 steel landscaping ties | Existing | Structural anchoring already done |
| Shelving | Existing | Already secured |
| Two vent openings | Existing | Low rear and high front airflow path |
| At least one direct-solar 5 V fan | Existing | Stays as independent cooling |
| 5 V solar panels | Existing | Used for charging or direct fan duty |
| USB power bank(s) | Existing | Used for 5 V buffered control power |
| Handheld mini fans | Existing | Used for the LM2596 always-on mixing subsystem |
| UV repair tape | Recommended to keep on hand | Immediate cover repair after tears or punctures |

## Purchased electronics currently on hand

| Item | Quantity | Status | Notes |
| --- | --- | --- | --- |
| SX1262 LoRa V3 ESP32 LX7 dual-core 0.96 in blue OLED Type-C Wi-Fi board, 863 MHz to 928 MHz | 1 | Bought | Treat this as the actual on-hand controller board; matches the repo target closely enough for the current ESP32-S3 controller plan |
| SG90 9 g micro servo motors | 2 | Bought | Actual purchased vent actuators; lighter-duty than the MG90S-class metal-gear servos used as the torque target elsewhere in the docs |
| DHT22 / AM2302 temperature and humidity sensor module | 1 | Bought | Actual purchased air temperature and humidity sensor; this differs from the active first-pass firmware sensor set |
| DAOKI 15 A 400 W MOSFET trigger switch drive modules | Unknown exact count | Bought | Purchased switched-load power stage hardware for fan, light, heater, or circulation branches |

## Immediate first-generation controller build

| Item | Quantity | Purpose | Procurement status |
| --- | --- | --- | --- |
| SX1262 LoRa V3 ESP32-S3 board | 1 | Main greenhouse controller with onboard OLED and future LoRa capability | Bought, using the SX1262 LoRa V3 ESP32 LX7 dual-core listing above |
| BME280 sensor | 1 | Air temperature and humidity | Not yet reflected as bought in this repo |
| BH1750 sensor | 1 | Ambient light sensing | Not yet reflected as bought in this repo |
| DS18B20 waterproof probe | 1 | Water temperature sensing | Not yet reflected as bought in this repo |
| OLED display | Not needed if using the target LoRa V3 board | Local status display | Covered by the bought controller board |
| MG90S-class metal-gear micro servos | 2 | Top and bottom vent motion | Current torque target; user currently bought 2 x SG90 instead |
| DAOKI-style 15 A 400 W MOSFET trigger modules | 3 to 5 | Exhaust, intake, grow light, heater, circulation outputs | Bought, exact installed branch count still to be decided |
| Momentary buttons | 3 | AUTO, FORCE OPEN, FORCE CLOSED | Not yet reflected as bought in this repo |
| 4.7 kOhm resistor | 1 | DS18B20 pull-up | Not yet reflected as bought in this repo |
| Terminal block or distribution strip | 1 | 5 V and ground distribution | Not yet reflected as bought in this repo |
| 18 AWG stranded wire | As needed | Main 5 V power and actuator runs | Not yet reflected as bought in this repo |
| 22 AWG to 24 AWG stranded wire | As needed | Sensor, button, and logic wiring | Not yet reflected as bought in this repo |
| Weather-resistant electronics box | 1 | Controller enclosure | Not yet reflected as bought in this repo |
| Cable glands or rubber grommets | As needed | Sealed enclosure cable entry | Not yet reflected as bought in this repo |
| Desiccant packs | Several | Moisture control inside electronics box | Not yet reflected as bought in this repo |
| USB breakout or sacrificial USB cables | As needed | Power-bank input and output taps | Not yet reflected as bought in this repo |
| Zip ties and adhesive mounts | As needed | Cable routing | Not yet reflected as bought in this repo |

## Current design versus purchased hardware note

- The controller board and DAOKI MOSFET modules already align with the active first-generation design.
- The bought DHT22 / AM2302 reflects what is physically on hand, but the current firmware and wiring docs still target BME280 plus BH1750 plus DS18B20 for the first implemented sensor stack.
- The bought SG90 servos reflect what is physically on hand, but the current mechanical torque target in the active plan is still MG90S-class metal-gear servos or equivalent if the real vent linkage proves too heavy for SG90 units.

## Structural and weather-hardening items

| Item | Quantity | Purpose |
| --- | --- | --- |
| Spare anchors or tie-down hardware | As needed | Replacing failed anchoring hardware after storms |
| Ratchet straps or equivalent hold-downs | As needed | Additional frame restraint in wind |
| Bungee cords or paracord | As needed | Supplemental bracing or strain relief |
| Cover clips or snap clamps | As needed | Securing greenhouse skin to frame members |
| Foam weatherstrip or seal tape | As needed | Draft reduction on winter shell or access points |

## LM2596 always-on four-fan subsystem

| Item | Quantity | Purpose |
| --- | --- | --- |
| LM2596 adjustable buck converter | 1 primary, 1 spare recommended | Step 5 V down to about 3.0 V |
| Inline fuse holder | 1 optional | Protect the fan branch |
| 2 A fuse | 1 optional | Branch protection |
| Small waterproof box | 1 | Protect buck converter and splices |

## Irrigation and sensing expansion items

| Item | Quantity | Purpose |
| --- | --- | --- |
| Gravity reservoir or elevated bucket | 1 optional | Simple non-powered irrigation buffer |
| Drip line kit or micro-tubing | As needed | Targeted plant watering |
| Shutoff valve or timer | 1 optional | Manual or scheduled irrigation control |
| Capacitive soil-moisture sensors | As needed | Future irrigation and plant-zone sensing |

## Dome and winter-shell build

| Item | Quantity | Purpose |
| --- | --- | --- |
| Clear dome material or segmented clear panels | To fit | Secondary shell over greenhouse |
| Hinge hardware | To fit | Low-mounted accordion or retractable shell |
| Latches or catches | To fit | Hold shell sealed in winter and parked in summer |
| Weather seal tape | To fit | Improve edge sealing |
| Black absorber panel material | 1 assembly | Solar heater surface inside the dome |
| Bubble wrap or inner clear poly layer | To fit | Low-cost winter insulation strategy |
| Frost cloth | To fit | Plant-level emergency freeze protection |

## Future 12 V upgrade

| Item | Quantity | Purpose |
| --- | --- | --- |
| Solar panels totaling 300 W to 400 W | To fit roof | Winter-capable array |
| MPPT charge controller | 1 | Efficient 12 V charging |
| 12 V LiFePO4 battery with low-temp cutoff | 1 | Main winter energy storage |
| AGM deep-cycle battery | 1 optional alternative | Simpler fallback chemistry |
| Fused distribution block | 1 | Safe branch distribution |
| Defogger | 1 initial, 2 maximum planned | Winter condensation management |
| Supplemental grow light | 1 or more | Winter photoperiod support |
| 12 V to 5 V buck converter | 1 | Feed controller from 12 V backbone |

## Materials deliberately excluded from the active design

| Item | Reason excluded |
| --- | --- |
| Wax-piston vent opener | Rejected as impractical for this greenhouse geometry |
| Tiny solar-garden-light circuits as core fan power | Too weak and too inconsistent for primary airflow duty |
