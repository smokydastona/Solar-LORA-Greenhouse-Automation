# 12 V Upgrade Wiring Plan

## Scope

This document defines the future second-generation winter power system. It is intentionally separate from the current 5 V build.

## Target architecture

```text
shed roof PV array (300 W to 400 W)
    -> MPPT charge controller
    -> 12 V battery with protection
    -> fused distribution block
        -> defogger branch 1
        -> defogger branch 2 optional
        -> grow-light branch
        -> 12 V fan branch
        -> 12 V to 5 V buck for controller and sensors
```

## Recommended power hardware

- Solar: 300 W to 400 W mounted on the 10 x 10 shed roof
- Controller: MPPT charge controller sized for the array current
- Battery: 12 V 60 Ah to 100 Ah LiFePO4 with low-temperature charge cutoff
- Fallback battery option: 12 V 100 Ah AGM if the simpler cold-weather behavior is more important than cycle life

## Distribution branches

| Branch | Typical load | Fuse target | Wire target |
| --- | --- | --- | --- |
| Main feed | Full system | 40 A | 12 AWG |
| Defogger 1 | 50 W to 100 W | 15 A | 14 AWG |
| Defogger 2 optional | 50 W to 100 W | 15 A | 14 AWG |
| Grow lights | 20 W to 60 W | 10 A | 16 AWG |
| 12 V fans | 1 A to 3 A | 5 A | 18 AWG |
| 12 V to 5 V buck | controller and sensors | 3 A | 18 AWG |

## Defogger design rules

- Start with one defogger only.
- Size wiring and fuse panel for a second unit later.
- Use the defogger for targeted dehumidification and frost control, not as the primary heat source.

## Grow-light design rules

- Treat winter grow lights as supplemental only.
- Put the grow-light branch on its own fuse and switching element.
- Run lights only during the programmed photoperiod and only when winter daylight is insufficient.

## Shed power-box concept

- Mount the power box low on the fence-side shed wall, not directly under a heat peak.
- Use an insulated, vented box if using LiFePO4 in winter.
- Keep high-current wiring short inside the shed.
- Run a single fused 12 V feed from the shed power box to the greenhouse distribution point.

## Grounding approach

- Use one intentional negative return bus.
- Do not rely on the shed or greenhouse frame as a conductor.
- Bring the controller buck-converter ground back to the same negative bus as the heavy loads.

## Relationship to the current firmware

The controller firmware in this repository already exposes outputs for a defogger and a grow light. Once the 12 V backbone exists, those outputs can drive appropriate relays or MOSFET stages through a 12 V to 5 V interface layer.
