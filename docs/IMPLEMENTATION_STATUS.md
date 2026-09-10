# Implementation status against the original specification

Automated completion does not substitute for the required real-hardware matrix.

## Implemented in the DLL

- Safe Win32 WarcraftXL extension loading and statically linked SDL 3.4.10 controller support.
- Canonical controls; one active Controller 1; deterministic selection, disconnect, and reconnect.
- Bounded main-thread polling with world-render, focus, overlay, capture, and neutral-state gates.
- Radial deadzones; cardinal/diagonal movement; opposing-state ordering; direction hysteresis.
- Native build-12340 movement/action execution isolated in the extension.
- Synchronous RMB camera fallback with lifecycle cleanup and no `SendInput`.
- Four immutable LT/RT layers, trigger hysteresis, and press-start binding ownership.
- Specified action layout; slots 25-48 and 69-72 remain outside controller ownership.
- `ActionSlot`, supported `WoWBinding`, `KeyBinding`, and `Unassigned` models.
- JSON global/character model, validation, inheritance, atomic writes, and default fallback.
- Live copy-on-write binding/profile/config mutations with rollback on failed persistence.
- Contract-v1 Lua table, character identity, effective binding sources, options, and capture submit.
- DLL-owned text-entry observer and stock FrameXML main-bar/stance paging resolver.
- Safe vehicle/possess degradation: ambiguous logical main-bar actions are suppressed.
- Ref-counted controller key ownership that does not release physically-held keys.
- Diagnostics, native state/capture ABI, policy tests, strict Win32 build, and reproducible pins.

## Implemented but awaiting hardware acceptance

- Corrected four cardinal and four diagonal directions, deadzone, and rapid reversals.
- Camera pitch/turn, sensitivity, invert Y, repeated ownership, and cursor restoration.
- Physical mouse/RMB/keyboard and `wxl-touch-input` coexistence during camera use.
- Every control in Base, LT, RT, and LT+RT, including held-button layer changes.
- Targeting, View/Map, Menu/Escape, arbitrary key chords, and named bindings.
- Connect-before-start, hotplug, disconnect, reconnect, focus loss, zoning, death, and logout.
- Xbox/XInput, PlayStation, and AYN Thor/GameNative across the required Windows versions.
- Lua contract smoke suite, `/reload` recreation, text-entry suppression, ordinary paging and
  stance/form resolution, plus vehicle/possess suppression.

Observed so far: DualSense discovery and native gameplay dispatch pass; touch coexistence passes
without `SendInput`. The latest cardinal-direction fix awaits user retest.

## Remaining DLL work

- Correct any behavior exposed by the pending in-client Lua, paging, text-entry, and hardware tests.
- Add support for additional client builds only after WarcraftXL offers a generic game-input API;
  exact-build bindings remain deliberately isolated for this client.

## Remaining addon implementation

- Capability/version negotiation and a graceful bridge-unavailable state.
- Glyph-family, connection, active-layer, and controller-state presentation.
- Binding capture/editor, profiles, deadzones, sensitivity, invert Y, and enable controls.
- Blizzard action-button presentation for four layers and the 69-72 auxiliary mouse strip.
- Global/character inheritance and reset UX.
- No controller cursor, gestures, radial menu, or replacement action database.

## Release blockers

- Lua bridge, mutation/persistence, `/reload`, and profile behavior pass in-client smoke tests.
- Text-entry suppression is proven in the real client.
- Ordinary paging and stance/form resolution, plus vehicle/possess suppression, are proven.
- The full device, lifecycle, keyboard/mouse, and touch coexistence matrix passes.
- No stuck movement, key, camera, or RMB on any cancellation path.
- CI, documentation, packaging, and manifest describe the functional artifact accurately.

v1 is not complete until the required real-hardware validation matrix passes.
