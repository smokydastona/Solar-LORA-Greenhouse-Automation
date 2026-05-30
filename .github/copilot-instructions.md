# Copilot Instructions — Solar LoRa Greenhouse Automation

This repository is for a real small-greenhouse automation system centered on an ESP32-S3 controller, a first-generation 5 V solar plus power-bank control layer, and a future 12 V winter backbone.

## Truth-first rules

- Be direct immediately when something cannot be verified, built, or executed.
- Never pretend a past chat, external artifact, or unavailable tool was read if it was not actually available.
- Do not soften technical limits by inventing certainty.
- If a requirement depends on hardware that does not yet exist in the repo, document the boundary clearly and implement the software side completely.

## Project mission

- Control greenhouse climate with real hardware, not mock logic.
- Keep the first-generation 5 V system simple, low-voltage, repairable, and safe for unattended family use.
- Preserve the separation between:
  - direct-solar subsystems that stay intentionally dumb
  - controller-backed subsystems that use buffered power, sensing, and logic
- Document the current system and the future 12 V upgrade as one coherent roadmap.

## Repository layout

- `src/` — greenhouse controller firmware
- `include/` — pin map and configurable thresholds
- `docs/` — master plan, wiring, build guides, materials, safety, and future upgrade docs
- `.github/` — repository automation, issue templates, PR template, and project-specific Copilot instructions

## Non-negotiables

- Build every requested feature completely. No placeholders, TODO stubs, fake integrations, or pretend hardware support.
- Do not silently replace greenhouse-specific decisions with generic IoT patterns.
- Do not reintroduce wax-piston vent automation. It is rejected for this project unless the user explicitly changes direction.
- Do not recommend unsafe mains-voltage hacks inside the greenhouse. Prefer low-voltage DC designs with explicit fusing and enclosure planning.
- Treat the project as safety-sensitive because it may be used unattended by non-technical family members.

## Current design facts

- The greenhouse already has two vents installed.
- At least one direct-solar USB fan already exists and remains independent.
- The controller is an ESP32-S3 design in this repo.
- The 5 V system is the active implementation target.
- The 12 V system is a documented future upgrade, not the current deployed electrical backbone.

## Implementation standards

- Keep firmware changes consistent with the configured pin map and thresholds in `include/`.
- When changing control behavior, update the matching docs in `docs/` in the same change set.
- Prefer explicit thresholds, state machines, and fail-safe defaults over opaque automation.
- Keep manual override paths simple and physically understandable.
- Preserve a clean separation between high-current load wiring and controller signal wiring in both code and docs.

## Workflow after every code or documentation change

1. Run diagnostics for the touched files or the repo.
2. Review the impact radius and update any affected docs immediately.
3. Re-check for errors after edits.
4. Keep the repo shippable: docs match code, workflows match repo contents, and issue templates reflect the real project.
5. Default to committing and pushing after a completed change set unless the user says not to.
6. If CI exists, treat CI as the authoritative final validation and check the resulting run.

## Impact-radius checklist

- Pin map changes:
  - Update `include/PinMap.h`
  - Update wiring docs in `docs/WIRING_5V.md`
  - Update any build or commissioning notes that mention those pins

- Threshold or behavior changes:
  - Update `include/Settings.h`
  - Update `docs/BUILD_GUIDE_5V.md`
  - Update `docs/GREENHOUSE_MASTER_PLAN.md` and `docs/OPERATIONS_AND_SAFETY.md` if behavior changes user-facing expectations

- Sensor changes:
  - Keep I2C and OneWire expectations accurate
  - Update BOM and wiring docs
  - Update display and logging docs if telemetry changes

- Power-architecture changes:
  - Keep 5 V and 12 V documents in sync
  - Recheck safety guidance, fuse tables, and grounding rules
  - Update README summary if the system boundary changes

- `.github` changes:
  - Remove references to non-existent folders, apps, firmware, or release assets
  - Keep issue templates aligned with the current repo structure
  - Keep CI workflows buildable from this repo alone

## Commands that matter in this repo

- Build firmware: `pio run -e greenhouse_controller`
- Upload firmware: `pio run -e greenhouse_controller -t upload`
- Serial monitor: `pio device monitor -b 115200`
- Check git status: `git status`
- View latest CI run: `gh run list --workflow "Build Release Bundle" --limit 1 --json status,conclusion,displayTitle`

## Repo-specific guidance for Copilot

- When the user asks for the full project plan, prefer the docs already in this repo over recreating freeform summaries.
- When the user asks for wiring, make the answer consistent with the real pin map and power architecture here.
- When a request touches both firmware and build/install guidance, update both in the same change set.
- If a requested feature cannot be honestly implemented without missing hardware, implement the code and docs boundary cleanly and say exactly what remains hardware-dependent.

