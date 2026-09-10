# Hardware test checklist

This file records human validation only; automated test results do not count as hardware passes.

## Diagnostic build 0.1.0

- [ ] Windows 10: DLL loads, no `SDL3.dll` required
- [ ] Windows 11: DLL loads, no `SDL3.dll` required
- [ ] Xbox/XInput pad detected before client start
- [ ] PlayStation pad detected before client start
- [ ] AYN Thor built-in controller detected through GameNative
- [ ] Hotplug after client start
- [ ] Controller 1 disconnect clears raw/logical state
- [ ] Controller 2 is not promoted
- [ ] Same Controller 1 reconnects
- [ ] Held controls are not replayed after reconnect
- [ ] Overlay displays canonical buttons, axes, triggers, and layer
- [ ] Alt+Tab/focus loss cancels state
- [ ] Loading and zoning cancel state
- [ ] Keyboard input remains normal
- [ ] Mouse input remains normal
- [ ] `wxl-touch-input` coexists independent of load order

## Functional exact-build candidate

- [ ] Cardinal and diagonal movement, including opposite transition ordering
- [ ] Stationary/moving RMB-style camera, pitch, deadzone, sensitivity, invert Y
- [ ] Base, LT, RT, and LT+RT mappings and held-button layer ownership
- [ ] ActionSlot, WoWBinding, KeyBinding, and Unassigned outputs
- [ ] Main-bar paging, stance/form, vehicle and possess correctness
- [ ] Profile persistence and addon capture
- [ ] `WXLControllerInput` contract-v1 smoke commands return the documented values
- [ ] `/reload` recreates the Lua table and observer
- [ ] Chat, macro-name, mail, and other focused edit boxes suppress controller gameplay
- [ ] Ordinary main-bar pages resolve the same slots the stock buttons display
- [ ] Stance/form bonus pages resolve the same slots the stock buttons display
- [ ] Vehicle and possess UI suppress ambiguous logical slots 1-12
- [ ] Failed/invalid mutations leave the previous live mapping and file intact

Record the exact controller model, connection mode, Windows version, WarcraftXL commit, extension
commit, DLL SHA-256, camera path, and observed result beside each completed run.

