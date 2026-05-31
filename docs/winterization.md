# Winterization Checklist

This document is the focused operator checklist for preparing the greenhouse and controller system for cold-weather service.

It complements, not replaces, [OPERATIONS_AND_SAFETY.md](./OPERATIONS_AND_SAFETY.md) and [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md).

## Before Cold Weather Starts

1. Inspect the greenhouse cover for tears, brittle seams, and loose attachment points.
2. Re-tighten anchors, tie-downs, vent hardware, and any external cable routing.
3. Confirm the controller enclosure is out of drip paths and not sitting near pooled water.
4. Replace or dry desiccant packs in the electronics enclosure.
5. Confirm every load branch and service lead is labeled before winter conditions make service harder.

## Controller And Sensor Checks

1. Verify the primary air sensor still reports believable values.
2. Verify the controller can still enter and exit `AUTO`, `OPEN`, and `CLOSED` modes cleanly.
3. Verify safe mode recovery instructions are available on-site for service.
4. Recheck battery reading plausibility if battery telemetry is enabled.
5. Confirm LittleFS logging still works before relying on winter history for diagnosis.

## Mechanical Checks

1. Re-test vent motion with conservative travel before leaving the system unattended.
2. Check for linkage binding that could worsen with frost or ice.
3. Treat SG90 servos as starter hardware, not as guaranteed winter-duty actuators under icing conditions.
4. Remove or secure anything that can flap, snag, or freeze into the vent path.

## Power Checks

1. Verify the 5 V solar plus power-bank path still charges and powers the controller in real daylight.
2. Reduce non-essential loads before extended overcast weather if power margin is weak.
3. Protect lithium storage from temperature conditions outside its safe chemistry range.
4. Do not assume the future 12 V winter backbone exists unless it has actually been built and commissioned.

## Moisture And Freeze Checks

1. Keep enclosure cable entries facing downward or sideways.
2. Remove standing water from trays, shelves, and low enclosure areas.
3. Use frost cloth and thermal-mass strategies before assuming the electrical layer can solve a hard freeze alone.
4. Inspect for condensation after the first cold wet nights, not only after failure.

## If Upgrading To The Future 12 V System

1. Treat [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md) as a separate build scope, not a small add-on to the live 5 V wiring.
2. Fuse the battery immediately at the positive terminal.
3. Keep the 12 V high-current load path physically separate from controller signal wiring.
4. Do not switch 12 V loads directly from ESP32 GPIOs.

## Read Next

- [OPERATIONS_AND_SAFETY.md](./OPERATIONS_AND_SAFETY.md)
- [WIRING_12V_UPGRADE.md](./WIRING_12V_UPGRADE.md)
- [troubleshooting.md](./troubleshooting.md)