# Controller addon contract

Status: **contract version 1**, final and frozen for addon development. The companion addon is a
separate optional project. The DLL owns controller discovery, input processing, active layer, game
execution, paging resolution, validation, and persistence. The addon owns presentation and user
interaction.

## Compatibility rules

- Contract version 1 names and meanings will not be changed.
- New fields, functions, binding types, and capabilities may be added compatibly.
- Existing fields or enum values will not be reordered or reused.
- The addon must test capabilities rather than infer them from a DLL version.
- Unknown fields, enum values, binding types, capabilities, and degraded reasons are ignored safely.
- Mutations return `true` on success or `nil, errorCode` and are validated by the DLL.

## Canonical identifiers

| Layer | Value |
|---|---:|
| `Base` | 0 |
| `LT` | 1 |
| `RT` | 2 |
| `LT+RT` | 3 |

| Button | Value/bit |
|---|---:|
| `FaceSouth` | 0 |
| `FaceEast` | 1 |
| `FaceWest` | 2 |
| `FaceNorth` | 3 |
| `DPadUp` | 4 |
| `DPadRight` | 5 |
| `DPadDown` | 6 |
| `DPadLeft` | 7 |
| `LeftShoulder` | 8 |
| `RightShoulder` | 9 |
| `LeftStick` | 10 |
| `RightStick` | 11 |
| `View` | 12 |
| `Menu` | 13 |

The eight primary buttons may be addressed on all layers. Auxiliary buttons are Base-only. `Menu`
is fixed to Escape and cannot be reassigned. Public calls accept canonical string names or these
stable numeric values; addon code should normally use names.

## Capabilities

Lua exposes named booleans: `diagnostics`, `bindingPersistence`, `bindingCapture`, `gameOutput`,
`characterProfiles`, and `effectiveActionSlots`.

The first five are true when the contract table is operational. `effectiveActionSlots` is dynamic:
it is true only while the DLL has a truthful stock-main-bar resolution for the current UI context.
Read it again after paging, stance/form, vehicle, possess, and world transitions.

The extension-to-extension native ABI advertises only the calls it exposes directly:
`diagnostics`, `bindingCapture`, and `gameOutput`.

## Binding values

```lua
{ type = "ActionSlot", slot = 1 } -- 1..120
{ type = "WoWBinding", command = "TARGETNEARESTENEMY" }
{ type = "KeyBinding", key = "F1", modifiers = { "CTRL" } }
{ type = "Unassigned" }
```

Supported WoW commands are `JUMP`, `MOVEFORWARD`, `MOVEBACKWARD`, `STRAFELEFT`, `STRAFERIGHT`,
`TARGETNEARESTENEMY`, `TARGETPREVIOUSENEMY`, `TARGETNEARESTFRIEND`,
`TARGETPREVIOUSFRIEND`, `TOGGLEAUTORUN`, `TOGGLEGAMEMENU`, and `TOGGLEWORLDMAP`.

Modifiers are `ALT`, `CTRL`, and `SHIFT`, with at most three entries. Keys are `A`-`Z`, `0`-`9`,
`-`, `=`, `BACKSPACE`, `DELETE`, `DOWN`, `END`, `ENTER`, `ESCAPE`, `HOME`, `INSERT`, `LEFT`,
`NUMLOCK`, `PAGEDOWN`, `PAGEUP`, `RIGHT`, `SPACE`, `TAB`, `UP`, and `F1`-`F12`. Binding source is
`builtin`, `global`, or `character`.

## Frozen Lua surface

Feature-detect the global table:

```lua
local api = _G.WXLControllerInput
if not api or api.contractVersion ~= 1 then
    -- Show "controller extension bridge unavailable" and remain usable.
end
```

Version 1 is:

```lua
WXLControllerInput = {
    contractVersion = 1,
    GetCapabilities = function() -> capabilities,
    GetState = function() -> state | nil, errorCode,
    GetOption = function(name) -> value | nil, errorCode,
    GetBinding = function(scope, layer, button) -> binding, source | nil, errorCode,
    SetBinding = function(scope, layer, button, binding) -> true | nil, errorCode,
    ResetBinding = function(scope, layer, button) -> true | nil, errorCode,
    ResetLayer = function(scope, layer) -> true | nil, errorCode,
    ResetProfile = function(scope) -> true | nil, errorCode,
    SetCharacter = function(realm, character) -> true | nil, errorCode,
    ClearCharacter = function() -> true,
    SetEnabled = function(enabled) -> true | nil, errorCode,
    SetOption = function(name, value) -> true | nil, errorCode,
    BeginBindingCapture = function() -> true | nil, errorCode,
    CancelBindingCapture = function() -> true,
}
```

`scope` is `global` or `character`; character calls require `SetCharacter`. Realm and character are
separate non-empty strings, at most 127 UTF-8 bytes each with no control characters. The DLL builds
its collision-free private profile key.

`GetState()` returns:

```lua
{
    extensionVersion = "0.1.0",
    enabled = true, runtimeReady = true, connected = true,
    inWorld = true, focused = true, overlayOpen = false,
    textEntryKnown = true, textEntryActive = false,
    captureActive = false, capturedButton = nil,
    device = {
        controllerIndex = 1, name = "...", family = "playstation", stableId = "...",
        labels = { FaceSouth = "Cross", LeftShoulder = "L1", Menu = "Options" },
    },
    axes = {
        raw = { leftX = 0, leftY = 0, rightX = 0, rightY = 0 },
        processed = { leftX = 0, leftY = 0, rightX = 0, rightY = 0 },
        leftTrigger = 0, rightTrigger = 0,
    },
    logicalLT = false, logicalRT = false, activeLayer = "Base",
    pressed = { FaceSouth = false }, cancellationReason = "startup",
    profile = { realm = "Realm", character = "Character", active = "character" },
    actionPage = 1,
    effectiveActionSlots = { [1] = 1, [2] = 2, [12] = 12 },
    degraded = false, degradedReason = nil,
    settings = {
        MovementDeadzone = 0.18, CameraDeadzone = 0.15,
        CameraHorizontalSensitivity = 1.0, CameraVerticalSensitivity = 1.0,
        InvertCameraY = false,
        TriggerActivateThreshold = 0.50, TriggerReleaseThreshold = 0.40,
    },
}
```

The addon may poll at up to 20 Hz. `effectiveActionSlots` is `nil` unless its capability is true.
An empty realm/character means no character profile is active. `degradedReason` is diagnostic and
may gain values; it is not a mutation error code.

Binding capture is neutral-gated. Begin, poll until `capturedButton` is non-nil, then either call
`SetBinding` for that same button or cancel. A successful matching `SetBinding` consumes capture.
Other mutations during capture return `CAPTURE_ACTIVE`. Gameplay is suppressed throughout capture.

Supported options are `MovementDeadzone` and `CameraDeadzone` (0..0.95),
`CameraHorizontalSensitivity` and `CameraVerticalSensitivity` (0.05..10), `InvertCameraY`
(boolean), `TriggerActivateThreshold` (0.01..1), and `TriggerReleaseThreshold` (0..activate).

Mutations use copy-on-write persistence. The candidate is validated and saved atomically before it
becomes active. A `SAVE_FAILED` leaves both the previous runtime state and previous file unchanged.
Successful changes cancel owned input, rebuild policy, and wait for neutral.

Stable errors are `UNAVAILABLE`, `INVALID_ARGUMENT`, `UNSUPPORTED`, `NO_CHARACTER`,
`CAPTURE_ACTIVE`, `NOT_IN_WORLD`, `NOT_FOCUSED`, and `SAVE_FAILED`.

Call `SetCharacter(GetRealmName(), UnitName("player"))` after player login and `ClearCharacter()`
when leaving that identity. Without the addon, global and built-in mappings continue to work.

## Native ABI

Native extensions obtain `wxl.controller-input`, API version 1. The authoritative header is
`src/bridge/ControllerInputApi.h`; implemented calls are `GetState`, `BeginBindingCapture`, and
`CancelBindingCapture`. Consumers initialize `state.structSize`. This interface is not callable by
a WoW addon and is intentionally narrower than the Lua contract.

## Action-bar contract

The addon rearranges Blizzard action buttons rather than creating a shadow spell database. Defaults
are Base 1-8, LT 49-56, RT 61-68, and LT+RT 9-12 plus 57-60. Slots 25-48 remain the vertical bars;
69-72 remain a mouse-only auxiliary strip.

Configured slots 1-12 are logical stock-main-bar positions. When `effectiveActionSlots` is true, the
DLL executes `state.effectiveActionSlots[configuredSlot]`, and the addon presents that same effective
slot. Slots 13-120 are literal and do not use translation.

The DLL resolves ordinary pages and stance/form bonus pages through stock FrameXML. Vehicle and
possess bars are ambiguous in contract v1: `effectiveActionSlots` becomes false and bindings to
logical slots 1-12 are suppressed. Other literal slots, movement, camera, and auxiliary bindings
continue. The addon must show those main controller cells as temporarily unavailable, not guess.

## Lua smoke test

Without the addon, run these from temporary test macros or `/run`:

```lua
/run local a=WXLControllerInput; print(a and a.contractVersion, a and a.GetState().runtimeReady)
/run local a=WXLControllerInput; local b,s=a.GetBinding("global","Base","FaceSouth"); print(b and b.type,s)
/run local a=WXLControllerInput; print(a.GetOption("MovementDeadzone"))
```

Test mutations on a disposable button and immediately restore it:

```lua
/run local a=WXLControllerInput; print(a.SetBinding("global","Base","FaceNorth",{type="Unassigned"}))
/run local a=WXLControllerInput; print(a.ResetBinding("global","Base","FaceNorth"))
```
