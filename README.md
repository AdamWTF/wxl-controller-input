# WarcraftXL Controller Input

`wxl-controller-input` is a WarcraftXL extension for World of Warcraft 3.3.5a
(build 12340). It is intended to provide native-feeling gamepad movement, camera control,
four LT/RT action layers, hotplugging, persistent profiles, and a versioned addon bridge while
leaving physical keyboard and mouse input untouched.

## Current status: diagnostic foundation 0.1.0

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
- deterministic policy tests and build-only Win32 CI.

The validated global profile is loaded at startup. Per-character records are persisted and tested,
but remain dormant until WarcraftXL can provide a safe realm/character identity.

The native runtime interface exposes capability flags, connection/context state, raw axes and
buttons, logical modifiers/layer, and binding-capture control. It intentionally does not advertise
the game-output capability. Calls are main-thread-only until WarcraftXL defines a broader threading
contract.

Gameplay output is intentionally disabled. The pinned WarcraftXL SDK has no published semantic
operations for movement, action slots, named bindings, text-entry detection, or RMB-style camera
input. See [SDK discovery](docs/SDK_DISCOVERY.md) for the evidence and proposed API. No private
offsets or synthetic mouse fallback have been added.

## Pins

- WarcraftXL: tag `v1.1.247`, commit `bc2fefd93fb195da05138f548c9c87db98413a49`
- SDL: release `3.4.10`, commit `8e37db5e797b6167f3a00d697d816a684bd259c7`
- Client: Win32 build `12340`

SDL is fetched at configure time and linked statically; no `SDL3.dll` is shipped.

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

Do not install this diagnostic DLL expecting it to control a character. It is for controller
enumeration and raw-state hardware validation only.

## License

GPL-3.0-or-later. SDL is available under the zlib license.
