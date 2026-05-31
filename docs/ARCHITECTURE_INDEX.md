# Architecture Index

This file is the diagram and architecture navigation index for the greenhouse project.

Use this document when you need to know which diagrams are current, which documents are authoritative, and which planning artifacts are historical context only.

## Canonical architecture diagrams

These are the only greenhouse architecture diagrams that should be treated as current visual references.

| Diagram | Scope | Status | Primary companion doc |
| --- | --- | --- | --- |
| [greenhouse-5v-summer-architecture.png](./diagrams/greenhouse-5v-summer-architecture.png) | Current first-generation 5 V controller-backed build plus independent direct-solar airflow | Canonical current-state diagram | [WIRING_5V.md](./WIRING_5V.md) |
| [greenhouse-5v-plus-12v-winter-architecture.png](./diagrams/greenhouse-5v-plus-12v-winter-architecture.png) | Future second-generation winter system with 12 V backbone feeding a later controller interface | Canonical future-state diagram | [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md) |

## Reference images that are not architecture diagrams

| Image | Purpose | Status |
| --- | --- | --- |
| [SX1262_Pinout.png](./diagrams/SX1262_Pinout.png) | Board-level reference for the ESP32-S3 SX1262 LoRa V3 hardware | Reference only |

## Historical and non-canonical material

- Anything under `docs/temp/` is planning history or reconciliation material, not a deployment wiring reference.
- The canonical written sources remain [GREENHOUSE_MASTER_PLAN.md](./GREENHOUSE_MASTER_PLAN.md), [WIRING_5V.md](./WIRING_5V.md), [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md), and [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md).
- If older concept sketches or superseded exports are added to the repository later, they should be marked clearly as superseded and should not reuse the canonical filenames above.

## How to use this index

- For the active build, start with [GREENHOUSE_MASTER_PLAN.md](./GREENHOUSE_MASTER_PLAN.md), then [WIRING_5V.md](./WIRING_5V.md), then [BUILD_GUIDE_5V.md](./BUILD_GUIDE_5V.md).
- For the future winter expansion, start with [GREENHOUSE_MASTER_PLAN.md](./GREENHOUSE_MASTER_PLAN.md), then [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md).
- For the fail-safe behavior and subsystem independence view, read [SAFETY_MODEL.md](./SAFETY_MODEL.md).
- For current implementation limits, read [FIRMWARE_LIMITATIONS.md](./FIRMWARE_LIMITATIONS.md) before assuming a safety or recovery feature exists in live firmware.
