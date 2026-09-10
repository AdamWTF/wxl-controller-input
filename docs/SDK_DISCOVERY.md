# WarcraftXL SDK discovery

## Pin

- Repository: `WarcraftXL/wxl-core`
- Tag: `v1.1.247`
- Commit: `bc2fefd93fb195da05138f548c9c87db98413a49`
- ABI manifest value: `1.1`
- `WXL_API_VERSION`: `1`
- Client build: `12340`
- Calling convention: plain C ABI with explicit `__cdecl`

The pin was inspected directly before implementation. `include/wxl/PluginApi.h` defines
`WXL_PluginInfo`, `WXL_Api`, and the `WXL_Query`/`WXL_Load` exports. The core loads one DLL from
`Extensions/<id>/<id>.dll`. Its build discovers folders under `extensions/`, compiles their `.cpp`
files, defines `WXL_EXTENSION`, and supports strict rejection of direct `offsets/` includes.

## Available public surface

- logging;
- typed event subscribe/emit, including `OnUpdate`, `OnInput`, `OnWorldEnter`, and `OnWorldLeave`;
- named and address hook arbitration;
- extension-to-extension versioned interface publication;
- diagnostic overlay controls;
- specialized asset/render interfaces unrelated to controller gameplay.

The event callbacks execute synchronously on the publishing game thread. `OnUpdate` supplies a
bounded once-per-frame integration point suitable for SDL polling and policy consumption.

## Confirmed gaps

No public semantic interface in this pin provides:

- movement state begin/end (`MOVEFORWARD`, `MOVEBACKWARD`, `STRAFELEFT`, `STRAFERIGHT`);
- action-button/action-slot press and release with current paging semantics;
- named WoW binding execution;
- Escape/game-menu execution;
- RMB-style relative camera input;
- reliable focused edit-box/text-entry state;
- realm and character identity;
- an addon-facing Lua/native registration or call bridge.

Named hooks exist for low-level world/input functions, but using them would still require guessed
signatures or engine details and would not be a semantic SDK contract. The extension therefore does
not attach them and declares no hooks in `wxl.json`. Header-only `game/*` helpers in this core still
embed curated offsets, and none covers the required gameplay-input semantics; they were not used.

## Minimum proposed upstream interface

Publish a C-compatible `wxl.game-input` interface with its own version and `structSize` containing:

```text
SetMovement(direction, down, ownerTag)
SetRelativeMouselook(x, y, active, ownerTag)
SetActionButton(buttonId, down, ownerTag)
ExecuteBinding(command, down, ownerTag)
ExecuteEscape()
GetInputContext(out inWorld, out textEntry, out gameplayWindowFocused)
GetActionButtonState(buttonId, out effectiveSlot/page)
GetCharacterIdentity(realmBuffer, characterBuffer)
RegisterLuaInterface(name, version, functionTable)
```

The core should keep addresses, signatures, action paging, main-thread enforcement, and synthetic
event tagging behind this interface. Movement and press/release calls should be idempotent per
owner and expose a single `CancelOwner(ownerTag)` cleanup operation. The controller extension can
then back its existing narrow sinks without changing policy.

The current functional candidate uses the explicitly permitted compatibility route while this
interface is unavailable: WoW-window keyboard messages for movement/actions and foreground-only
RMB/relative mouse output for camera. This does not change the preferred upstream design and must
pass the full coexistence matrix before release.
