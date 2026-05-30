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

## Immediate first-generation controller build

| Item | Quantity | Purpose |
| --- | --- | --- |
| SX1262 LoRa V3 ESP32-S3 board | 1 | Main greenhouse controller with onboard OLED and future LoRa capability |
| BME280 sensor | 1 | Air temperature and humidity |
| BH1750 sensor | 1 | Ambient light sensing |
| DS18B20 waterproof probe | 1 | Water temperature sensing |
| OLED display | Not needed if using the target LoRa V3 board | Local status display |
| MG90S-class metal-gear micro servos | 2 | Top and bottom vent motion |
| DAOKI-style 15 A 400 W MOSFET trigger modules | 3 to 5 | Exhaust, intake, grow light, heater, circulation outputs |
| Momentary buttons | 3 | AUTO, FORCE OPEN, FORCE CLOSED |
| 4.7 kOhm resistor | 1 | DS18B20 pull-up |
| Terminal block or distribution strip | 1 | 5 V and ground distribution |
| 18 AWG stranded wire | As needed | Main 5 V power and actuator runs |
| 22 AWG to 24 AWG stranded wire | As needed | Sensor, button, and logic wiring |
| Weather-resistant electronics box | 1 | Controller enclosure |
| Cable glands or rubber grommets | As needed | Sealed enclosure cable entry |
| Desiccant packs | Several | Moisture control inside electronics box |
| USB breakout or sacrificial USB cables | As needed | Power-bank input and output taps |
| Zip ties and adhesive mounts | As needed | Cable routing |

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
