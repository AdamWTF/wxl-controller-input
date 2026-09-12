# Implementation status

## Implemented

- Fixed WoWpadX modifier-style-1 ConsolePort keyboard namespace.
- SDL3 positional buttons, triggers, sticks, Guide, Misc1, and paddle controls.
- Stateful transitions, independent Shift/Ctrl, W/A/S/D diagonals, H/V helpers, and stick swap.
- WoWpadX-style radial pointer deadzone and response curve with fractional accumulation.
- Held L3/R3 mouse buttons and physical keyboard/mouse coexistence ownership.
- Sticky Controller 1 discovery, hotplug, disconnect cleanup, focus/world/overlay gates, and neutral
  restart protection.
- Focused WoW-window sink without global injection, client offsets, or core edits; camera-only cursor
  movement is saved and restored.
- Minimal technical configuration, startup/hotplug logging, diagnostic overlay, and policy tests.

## Intentionally retired

- Direct action-slot and named WoW action execution.
- Remapping profiles, binding persistence/capture, addon editor Lua API, and published controller API.
- Build-specific native addresses and the Lua validation hook.

## Release blockers

- Win32 DLL and policy tests must pass in CI.
- ConsolePortLK with the Numpad 4 `CP_R_UP` calibration must pass the complete mapping and four-layer
  test with WoWpadX absent.
- Disconnect, reconnect, focus, physical input coexistence, and all target controller/platform tests
  in `HARDWARE_TEST_CHECKLIST.md` must pass without stuck state or leakage.
