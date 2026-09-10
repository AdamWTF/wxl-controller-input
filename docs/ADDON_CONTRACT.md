# Companion addon contract

The addon is a separate future project. It presents configuration and controller-aware UI; the DLL
remains authoritative for input state, active controller, current layer, persisted mappings, and
execution. The addon must continue to be optional.

The intended interface is `wxl.controller-input`, API version 1, with an independent capability
bitset. All calls are bounded, validate enum/string/buffer arguments, return explicit status codes,
and never throw across the C ABI. It will be registered through a generic WarcraftXL Lua/native
bridge once such a bridge exists.

The DLL already publishes the C-compatible version 1 interface through WarcraftXL's extension
interface registry. Its current capability flags cover diagnostics, binding persistence, binding
capture, and game output. `GetState`, `BeginBindingCapture`, and
`CancelBindingCapture` are implemented. Consumers must invoke them on the game/main thread. The
header is `src/bridge/ControllerInputApi.h`.

Read operations expose extension/runtime versions and readiness; capability/degraded flags;
Controller 1 identity, name, family and presentation labels; raw and processed axes; button and
trigger state; authoritative LT/RT state and active layer; effective configuration; effective
bindings plus built-in/global/character source; realm/character/profile identity; and binding
capture status.

Write operations are versioned equivalents of:

```text
SetBinding / ClearBinding / ResetBinding / ResetLayer
ResetCharacterProfile / ResetGlobalBindings
SetMovementDeadzone / SetCameraDeadzone
SetCameraHorizontalSensitivity / SetCameraVerticalSensitivity / SetInvertCameraY
SetEnabled
BeginBindingCapture / CancelBindingCapture
```

Configuration changes first cancel affected owned state, apply atomically, and wait for a fresh
physical transition. During binding capture, gameplay mappings, movement and camera are suppressed
while canonical button transitions remain observable.

## Action bars

The addon rearranges existing Blizzard action buttons; it does not create a shadow spell database.
Controller groups are Base (1-8), LT (49-56), RT (61-68), and LT+RT (9-12 plus 57-60). Slots 25-48
remain the two normal vertical bars. Slots 69-72 form a mouse-only auxiliary strip.

Main-bar paging is a release blocker. The bridge must expose effective action-button semantics so
the ability displayed for a controller button is exactly the one invoked in every stance, form,
vehicle, possess, and page state. A permanently fixed numerical slot is not sufficient for the main
bar. Until WarcraftXL supplies this semantic contract and it is hardware-tested, the addon must not
claim action-bar correctness.
