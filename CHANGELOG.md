# Changelog

All notable changes to this project will be documented in this file.

This changelog follows Keep a Changelog style. The versioning rules for this repository are defined in [docs/versioning.md](./docs/versioning.md).

This file starts with the first explicitly documented public baseline for the repository. Earlier work remains visible in git history.

## [Unreleased]

### Added

- A dedicated firmware version header at `include/Version.h`, plus boot-time OLED and serial version reporting for field verification.
- Repository hygiene files for licensing and contribution guidance.
- Runtime HAL modules for battery sensing, display setup, and sensor sampling under `include/runtime/` and `src/runtime/`.
- A new SVG system block diagram for the docs set and GitHub Pages site.
- CI-driven versioned release creation that derives Git tags from `include/Version.h` on pushes to `main`.
- Nearby Wi-Fi scanning in the local setup portal so users can pick the SSID locally and only enter or import the password.
- Browser-based firmware upload from the node dashboard using the ESP32-S3 OTA partition layout.
- Automatic firmware patch versioning from git history during each build and release.

### Planned

- Power telemetry expansion for solar voltage, current, charge state, and runtime estimation.
- Dashboard packaging growth beyond the initial Home Assistant and Grafana starter examples.
- Versioned release notes discipline for future firmware and docs releases.

## [0.1.0] - 2026-05-30

### Added

- First documented public release baseline for the ESP32-S3 greenhouse controller project.
- Standard public documentation entry points for architecture, hardware, installation, troubleshooting, API, and development.
- A Home Assistant and Grafana dashboard guide plus starter example configs.
- A versioning policy for firmware, docs, and telemetry changes.
- A power telemetry roadmap tied to the actual 5 V solar plus power-bank system and the planned 12 V winter upgrade.
- Preferences-backed boot reason logging, boot-failure tracking, safe mode, and watchdog-backed recovery behavior.
- Greenhouse metrics including VPD, dew point, frost risk, and crop profile evaluation.
- Optional MQTT retained telemetry and Home Assistant discovery payloads.
- Heltec onboard battery-voltage sensing using the board battery-read path.

### Changed

- README now answers the public project-entry questions directly instead of requiring readers to infer them from greenhouse-specific docs.
- Troubleshooting documentation is now exposed through the standard lowercase path `docs/troubleshooting.md`.

### Fixed

- Documentation packaging now matches the actual repo structure and current implementation boundary more closely.
