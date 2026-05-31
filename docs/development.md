# Development Guide

This document is the standard entry point for contributors and maintainers.

For the greenhouse-specific engineering rules, use [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md).

## Project Layout

- `src/main.cpp`: board runtime, integration, logging, display, connectivity, and GPIO output writes
- `include/ControlLogic.h`: pure greenhouse logic and greenhouse metrics
- `include/Settings.h`: thresholds, feature toggles, and configuration
- `include/PinMap.h`: hardware pin contract
- `test/test_control_logic/test_control_logic.cpp`: host-side tests for the pure logic layer

## Local Commands

- build firmware: `pio run -e greenhouse_controller`
- run host-side tests: `pio test -e control_logic_native`
- upload firmware: `pio run -e greenhouse_controller -t upload`
- serial monitor: `pio device monitor -b 115200`

## Quality Surface

Current repo expectations:

- touched files stay diagnostic-clean
- logic changes should extend or preserve host-side tests where possible
- docs must stay truthful and match the current repo boundary
- CI should remain buildable from this repo alone

## CI And Release Surface

- firmware and docs bundle workflow: [../.github/workflows/build-release-bundle.yml](../.github/workflows/build-release-bundle.yml)
- docs publishing workflow: [../.github/workflows/deploy-docs-site.yml](../.github/workflows/deploy-docs-site.yml)

## Engineering Status Against A Higher-End Standard

Already present:

- unit-style host tests for core logic
- CI workflows
- release-bundle workflow
- OTA support when Wi-Fi is configured
- watchdog and safe-mode behavior in firmware

Still missing or incomplete:

- linting and static-analysis surface clearly documented as a maintained workflow
- integration tests beyond the pure logic layer
- rollback firmware path
- Docker development environment
- richer release workflow and release automation beyond the new changelog discipline
- hardware revision notes and explicit board-revision tracking

## Versioning And Release Notes

- Release notes now live in [../CHANGELOG.md](../CHANGELOG.md).
- Versioning rules now live in [versioning.md](./versioning.md).
- Any public telemetry, wiring, or operator-behavior change should update the changelog in the same change set.

## Read Next

- [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md)
- [api.md](./api.md)
- [architecture.md](./architecture.md)
- [versioning.md](./versioning.md)