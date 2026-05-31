# Versioning Policy

This repository now uses an explicit versioning policy for firmware, docs, and telemetry changes.

## Versioning Model

The project follows semantic versioning with a strict pre-1.0 interpretation.

Format:

- `MAJOR.MINOR.PATCH`

Examples:

- `0.1.0`
- `0.2.0`
- `1.0.0`

The firmware version string that ships on the controller is defined in [../include/Version.h](../include/Version.h). Update that header in the same change set as any release-note or release-tag change.

On pushes to `main`, CI now uses that same header as the source of truth for release tags and GitHub releases. If `include/Version.h` says `0.1.1`, the release workflow targets `v0.1.1` and skips creating a duplicate if that tag already exists.

## Pre-1.0 Rule

The project is currently below `1.0.0`.

That means:

- `MINOR` releases may still include breaking changes if the project needs them
- any breaking change must still be called out explicitly in [../CHANGELOG.md](../CHANGELOG.md)
- `PATCH` releases should remain narrow, low-risk fixes or documentation corrections

## What Changes A Version

### Patch release

Use a patch release for:

- documentation fixes that do not change required operator behavior
- bug fixes that do not change the intended public control or telemetry contract substantially
- wiring-note clarifications
- CI and packaging fixes with no intended user-facing feature expansion

### Minor release

Use a minor release for:

- new firmware features such as new sensors, telemetry fields, or reliability behaviors
- new dashboard packages or integration artifacts
- behavior changes that operators need to know about
- topic additions or telemetry payload growth

### Major release

Use a major release for:

- a stabilized `1.0.0` declaration that the project considers its core contracts intentionally supported
- post-1.0 breaking changes to firmware behavior, wiring assumptions, or telemetry contracts

## Contract Discipline

The following surfaces are treated as public contracts and should be updated deliberately:

- wiring and pin assumptions
- operator-facing behavior documented in build and safety guides
- MQTT topic and payload contract
- dashboard example compatibility notes

If any of those change, the same change set should update:

- [../CHANGELOG.md](../CHANGELOG.md)
- [api.md](./api.md) or [REMOTE_TELEMETRY_CONTRACT.md](./REMOTE_TELEMETRY_CONTRACT.md) when telemetry changes
- the matching wiring, installation, or operations docs when hardware assumptions change

## Release Tag Guidance

When release tags are used, prefer tags that match the version exactly:

- `v0.1.0`
- `v0.2.0`

The running firmware should surface the same version on the serial boot banner and OLED boot screen so field verification matches the git tag and changelog entry.

## Reality Rule

Do not cut a new version just because docs or code changed. Cut a version when the repo reaches a meaningful and reviewable state that would make sense to an outside user or operator.
