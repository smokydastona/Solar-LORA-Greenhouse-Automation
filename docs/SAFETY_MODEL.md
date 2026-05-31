# Safety Model

This document shows how the greenhouse is intended to fail and which subsystems remain independent when the controller-backed layer is unavailable.

Use it alongside [GREENHOUSE_MASTER_PLAN.md](./GREENHOUSE_MASTER_PLAN.md), [WIRING_5V.md](./WIRING_5V.md), and [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md).

## Safety model figure

```text
                               SAFETY MODEL

 sun
  |
  +--> direct-solar panel --> direct-solar USB fan ------------------------+
  |                                                                        |
  |                                                                        v
  |                                                              basic heat-response airflow
  |
  +--> 5 V controller solar panels --> power bank --> ESP32-S3 controller --> sensors
                                              |                 |              |
                                              |                 |              +--> telemetry, display, logs
                                              |                 |
                                              |                 +--> vent servos
                                              |                 +--> 5 V MOSFET-switched branches
                                              |                        |- intake fan
                                              |                        |- exhaust fan
                                              |                        |- circulation fan
                                              |                        |- defogger output
                                              |                        '- grow-light output
                                              |
                                              +--> if controller layer fails, these controlled branches stop

 future winter path:

 shed roof PV --> MPPT --> 12 V battery --> fused 12 V distribution --> 12 V loads
                                                   |
                                                   +--> 12 V to 5 V buck --> controller layer

 if the controller dies:
 - direct-solar USB fan can still run in sun
 - passive greenhouse structure still exists
 - future 12 V backbone can still remain electrically separate until intentionally integrated
```

## Failure-containment intent

| Failure case | What still works | What stops or degrades | Intended outcome |
| --- | --- | --- | --- |
| ESP32 crash or lockup | Direct-solar fan, passive vent geometry, physical greenhouse shell | Controller-driven vents, switched branches, display updates, logs | Greenhouse fails simpler, not into uncontrolled high-current behavior |
| Wi-Fi or OTA unavailable | Local control logic, display, buttons, direct-solar fan | OTA updates, network time sync | Local operation continues without cloud dependency |
| Main air sensor unavailable | Manual modes, direct-solar fan, other physically independent subsystems | Automatic climate decisions become untrustworthy | Operator should fall back to conservative manual or documented safe policy |
| Power-bank depletion | Direct-solar fan in sun, passive structure | Controller layer and controller-switched loads | Heat-response airflow still has one intentionally dumb path |
| Future 12 V branch fault | 5 V controller layer can remain separate if wired as documented | Faulted 12 V branch only | High-current faults stay branch-local when fused correctly |

## Design rules this figure reinforces

- Keep the direct-solar fan electrically independent from the controller-backed system.
- Keep controller GPIOs isolated from real load current through MOSFET, relay, or interface stages.
- Keep the future 12 V backbone physically and electrically understandable as a separate power layer.
- Do not assume that documented safe-policy targets are already live firmware features; verify them against [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md).

## Operator interpretation

- If the smart layer is down, do not assume the greenhouse is fully protected; assume only the independent and passive protections remain.
- Use this file as the quick answer to "what still works if the brain dies?"
