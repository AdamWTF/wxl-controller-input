# Hardware and integration test checklist

Record controller model, connection mode, Windows/runtime version, WarcraftXL commit, extension
commit, DLL SHA-256, and observed result for each pass.

## Packaging and lifecycle

- [ ] Win32 DLL loads on Windows 10 and Windows 11 with no `SDL3.dll`.
- [ ] WoW starts normally with no gamepad; later hotplug activates Controller 1.
- [ ] Xbox/XInput, PlayStation, and AYN Thor/GameNative devices use positional controls correctly.
- [ ] A secondary pad produces no input and is not promoted after Controller 1 disconnects.
- [ ] Same-identity reconnect resumes after a neutral snapshot without restarting WoW.
- [ ] Disconnect, focus loss, zoning/logout, overlay open, disable, and shutdown release all states.
- [ ] No controller or sink failure crashes WoW or spams the log.

## ConsolePortLK contract

- [ ] With WoWpadX absent and ConsolePortLK unmodified, D-pad maps to F1-F4.
- [ ] Back/Start map to F5/F6; LB/RB map to F7/F8.
- [ ] North/East/South/West map to Numpad 4/F10/F11/F12.
- [ ] LT, RT, and LT+RT remain held as Shift, Ctrl, and Shift+Ctrl around face-button presses.
- [ ] Left stick produces all W/A/S/D cardinal and diagonal combinations with clean centre release.
- [ ] H/V dominance helpers work with `SimpleRadial=false` and never appear when true.
- [ ] Right stick rotates the camera smoothly in all directions, restores the cursor at centre, and has no centre drift.
- [ ] `SwapSticks=true` swaps only axes; L3/R3 still hold left/right mouse buttons.
- [ ] Touch tap, double tap, UI drag, and world drag never inherit mouselook or flip the camera.
- [ ] Guide, Misc1, and each available paddle produce the documented Numpad key.

## Coexistence and platforms

- [ ] Physical keyboard/mouse input remains functional during and after controller-held overlap.
- [ ] Controller input never reaches another application while WoW is unfocused.
- [ ] Windowed, fullscreen-windowed, and supported fullscreen modes behave correctly.
- [ ] AYN Thor/GameNative validation passes before platform-specific changes are considered.
- [ ] Wine and Steam Deck/Proton validation passes or documents window-message limitations.
