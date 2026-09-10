# WarcraftXL Controller Input

`wxl-controller-input` is a WarcraftXL extension for World of Warcraft 3.3.5a
(build 12340). It is intended to provide native-feeling gamepad movement, camera control,
four LT/RT action layers, hotplugging, persistent profiles, and a versioned addon bridge while
leaving physical keyboard and mouse input untouched.

## Current status: pre-addon build-12340 hardware candidate 0.1.0

The repository currently implements the safe first milestones:

- SDL3 Gamepad discovery, canonical control translation, Controller 1 selection, disconnect,
  and same-identity reconnect;
- radial stick deadzones, deterministic eight-direction movement policy, trigger hysteresis,
  press-start layer ownership, default mappings, profile resolution, and camera policy;
- versioned JSON global/character binding profiles, reset-to-inherited behavior, strict binding
  validation, and atomic configuration/profile replacement;
- release-on-cancel behavior and neutral-state reconciliation;
- validated configuration defaults and a diagnostic WarcraftXL overlay panel;
- neutral-gated binding capture and a versioned `wxl.controller-input` native runtime interface;
- a contract-v1 `_G.WXLControllerInput` Lua table with live transactional mappings, profiles,
  settings, binding capture, controller state, and presentation metadata;
- DLL-owned edit-box suppression and truthful stock main-bar/stance paging resolution, with safe
  logical-slot suppression in ambiguous vehicle and possess contexts;
- deterministic policy tests and build-only Win32 CI.

The validated global profile is loaded at startup. The addon can supply realm and character through
the Lua contract; without it, the DLL remains independently usable with global/default mappings.

The native runtime interface exposes capability flags, connection/context state, raw axes and
buttons, logical modifiers/layer, binding capture, and game output. Calls are main-thread-only.

The hardware candidate ports the proven native movement and action calls from the earlier
WoW Companion Screen controller. Those build-specific bindings are isolated in one adapter and
validated against the exact supported `Wow.exe` hash. The adapter uses the client's own input
state machine for movement, the native action executor for action slots, synchronous window input
for auxiliary key bindings, and the earlier synchronous RMB/mouse-move camera fallback. It never
uses `SendInput` and requires no WarcraftXL core change or second DLL.

## Pins

- WarcraftXL: tag `v1.1.247`, commit `bc2fefd93fb195da05138f548c9c87db98413a49`
- SDL: release `3.4.10`, commit `8e37db5e797b6167f3a00d697d816a684bd259c7`
- Client: Win32 build `12340`

SDL is fetched at configure time and linked statically; no `SDL3.dll` is shipped. Only its
gamepad/joystick and supporting event functionality is enabled; audio, video, rendering, GPU,
camera, haptic, sensor, power, dialog, and tray subsystems are disabled.

## Build tests

```powershell
cmake -S . -B build -DWXL_CONTROLLER_BUILD_EXTENSION=OFF
cmake --build build --config Release --target wxl-controller-tests
ctest --test-dir build -C Release --output-on-failure
```

To build the extension standalone, use a Win32 generator and set `WXL_CORE_ROOT` to the exact
pinned core checkout. CI also verifies the supported integration path by copying `src/` and
`shared.cmake` into `wxl-core/extensions/wxl-controller-input` and building only that target with
`WXL_STRICT_SDK_BOUNDARY=ON`.

This remains a hardware-test candidate. The Lua smoke suite, paging/text-entry behavior,
camera/touch coexistence, and every cleanup path must pass in the client before release.

The maintained [addon contract](docs/ADDON_CONTRACT.md) freezes the version-1 identifiers and Lua
surface. The [implementation status](docs/IMPLEMENTATION_STATUS.md) records what remains before v1.

## License

GPL-3.0-or-later. SDL is available under the zlib license.
