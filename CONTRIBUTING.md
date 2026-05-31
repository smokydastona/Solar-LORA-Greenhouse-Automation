# Contributing

This repository controls a real greenhouse and should be treated as safety-sensitive low-voltage automation, not as a generic demo project.

## Ground rules

- Keep the 5 V active implementation boundary truthful.
- Do not claim hardware support that is not wired, bench-tested, and documented in this repo.
- Update matching docs in the same change set when behavior, thresholds, wiring assumptions, or safety posture changes.
- Preserve the separation between the controller-backed subsystem and the independent direct-solar airflow path.

## Development workflow

1. Start from the current docs and configured pin map before changing code.
2. Make focused changes that match the real greenhouse architecture.
3. Run the native control-logic tests with `pio test -e control_logic_native` when control behavior changes.
4. Run diagnostics for touched files or the repo before opening a pull request.
5. If firmware behavior changes user expectations, update the relevant docs under `docs/` immediately.

## Pull requests

- Use the repo PR template.
- State any hardware assumptions explicitly.
- Call out any behavior that has not been physically validated in the greenhouse.
- Avoid mixing unrelated firmware, wiring, and docs changes into one PR.

## Safety expectations

- Prefer fail-safe defaults and explicit state transitions.
- Do not introduce unsafe mains-voltage guidance for greenhouse deployment.
- Keep high-current wiring guidance and controller signal guidance clearly separated in both code and docs.