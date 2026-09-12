# WarcraftXL Controller Input

`wxl-controller-input` is a WarcraftXL extension for World of Warcraft 3.3.5a build 12340. It
replaces WoWpadX's controller-input role for an unmodified ConsolePortLK installation.

```text
SDL3 gamepad -> fixed ConsolePort key/mouse mapping -> WoW window -> ConsolePortLK
```

The extension uses SDL3's normalized Gamepad API, supports one sticky Controller 1 with hotplug,
and emits stateful WoW-window keyboard and mouse input. It does not use `SendInput`, contain client
addresses, publish a Lua editor API, or require Pixel Bridge,
Steam Input, an external mapper, or a companion application.

## Fixed mapping

- D-pad: F1-F4; Back/Start: F5/F6; LB/RB: F7/F8.
- North/East/South/West: Numpad 4/F10/F11/F12. Numpad 4 avoids WarcraftXL's F9 overlay toggle.
- LT/RT: left Shift/left Ctrl.
- Left stick: W/A/S/D plus ConsolePort H/V helpers; right stick: focused camera movement.
- L3/R3: held left/right mouse buttons.
- Guide/Misc1 and supported paddles: Numpad `*`, `+`, and 0-3.

Right-stick camera control temporarily moves the native cursor only while WoW is foreground, then
restores it. Touchscreen contact suspends controller mouse output until touch promotion has ended
and the stick/buttons return neutral, preventing touch clicks from inheriting mouselook.

Copy `wxl-controller-input.cfg.example` beside the DLL as `wxl-controller-input.cfg` to customize
technical thresholds. Gameplay actions remain owned by ConsolePortLK.

## Build

The project pins WarcraftXL v1.1.247 and SDL 3.4.10. Policy tests can be built without the DLL:

```powershell
cmake -S . -B build -DWXL_CONTROLLER_BUILD_EXTENSION=OFF
cmake --build build --config Release --target wxl-controller-tests
ctest --test-dir build -C Release --output-on-failure
```

The extension requires a Win32 toolchain and `WXL_CORE_ROOT` pointing to the pinned core checkout.
SDL is statically linked, so no `SDL3.dll` is shipped.

## Status

Version 0.2.2 is an implementation candidate. Automated policy coverage is included; the Windows,
controller, ConsolePortLK, GameNative, Wine, and Proton hardware matrix remains the release gate.

License: GPL-3.0-or-later. SDL uses the zlib license.
