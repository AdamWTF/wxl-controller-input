# Controller addon contract

Status: **contract version 1**, frozen for addon development. The companion addon is a separate
project and remains optional. The DLL owns controller discovery, input processing, active layer,
game execution, validation, and persistence. The addon owns presentation and user interaction.

This document distinguishes the native ABI already implemented from the Lua bridge still required.
An addon cannot call WarcraftXL's native extension registry directly.

## Compatibility rules

- Contract version 1 names and meanings will not be changed.
- New fields, functions, binding types, and capability bits may be added compatibly.
- Existing fields or enum values will not be reordered or reused.
- The addon must test capabilities rather than infer them from a DLL version.
- Unknown fields, enum values, binding types, and capability bits must be ignored safely.
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
is fixed to Escape and cannot be reassigned.

## Capabilities

Lua exposes capabilities by name: `diagnostics`, `bindingPersistence`, `bindingCapture`,
`gameOutput`, `characterProfiles`, and `effectiveActionSlots`.

The current native ABI advertises `diagnostics`, `bindingCapture`, and `gameOutput`. Persistence code
exists internally but is not advertised until mutation, saving, and live rebinding are connected.
The Lua bridge and its last three capabilities are not implemented yet.

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

The addon feature-detects the global table:

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
    GetState = function() -> state,
    GetBinding = function(scope, layer, button) -> binding, source,
    SetBinding = function(scope, layer, button, binding) -> true | nil, errorCode,
    ResetBinding = function(scope, layer, button) -> true | nil, errorCode,
    ResetLayer = function(scope, layer) -> true | nil, errorCode,
    ResetProfile = function(scope) -> true | nil, errorCode,
    SetCharacter = function(realm, character) -> true | nil, errorCode,
    SetEnabled = function(enabled) -> true | nil, errorCode,
    SetOption = function(name, value) -> true | nil, errorCode,
    BeginBindingCapture = function() -> true | nil, errorCode,
    CancelBindingCapture = function() -> true,
}
```

`scope` is `global` or `character`; character calls require `SetCharacter`. Realm and character are
separate strings and the DLL constructs its private profile key.

`GetState()` returns:

```lua
{
    enabled = true, runtimeReady = true, connected = true,
    inWorld = true, focused = true, overlayOpen = false,
    captureActive = false, capturedButton = nil,
    device = { name = "...", family = "playstation", stableId = "..." },
    axes = { leftX = 0, leftY = 0, rightX = 0, rightY = 0,
             leftTrigger = 0, rightTrigger = 0 },
    logicalLT = false, logicalRT = false, activeLayer = "Base",
    pressed = { FaceSouth = false },
    cancellationReason = "startup",
}
```

The addon may poll at up to 20 Hz. Binding capture is neutral-gated: begin, poll until
`capturedButton` is non-nil, then submit or cancel. Gameplay is suppressed during capture.

Supported options are `MovementDeadzone` and `CameraDeadzone` (0..0.95),
`CameraHorizontalSensitivity` and `CameraVerticalSensitivity` (0.05..10), `InvertCameraY`
(boolean), `TriggerActivateThreshold` (0.01..1), and `TriggerReleaseThreshold` (0..activate).

Mutations cancel affected input, save atomically, rebuild the effective map, and wait for neutral.
Stable errors are `UNAVAILABLE`, `INVALID_ARGUMENT`, `UNSUPPORTED`, `NO_CHARACTER`,
`CAPTURE_ACTIVE`, `NOT_IN_WORLD`, `NOT_FOCUSED`, and `SAVE_FAILED`.

## Native ABI already available

Native extensions obtain `wxl.controller-input`, API version 1. The authoritative header is
`src/bridge/ControllerInputApi.h`; implemented calls are `GetState`, `BeginBindingCapture`, and
`CancelBindingCapture`. Consumers initialize `state.structSize`. This native ABI is not callable by
a WoW Lua addon; implementing the Lua table above remains a DLL milestone.

## Action-bar contract

The addon rearranges Blizzard action buttons rather than creating a shadow spell database. Defaults
are Base 1-8, LT 49-56, RT 61-68, and LT+RT 9-12 plus 57-60. Slots 25-48 remain the vertical bars;
69-72 remain a mouse-only auxiliary strip.

The addon must not claim `effectiveActionSlots` until the DLL/core reports the effective action
behind paged main-bar buttons. Stance, form, vehicle, possess, and paging correctness is a release
blocker, not an addon-side guess.
