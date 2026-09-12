# WarcraftXL SDK decision

The implementation targets WarcraftXL v1.1.247 (`bc2fefd93fb195da05138f548c9c87db98413a49`), ABI
manifest 1.1, API version 1, and WoW build 12340.

The pinned public API provides logging, typed lifecycle/update/input events, focus-visible window
messages, and diagnostics. It does not expose semantic keyboard, mouse, or relative-input output.

For this extension-only release, `IWoWInputSink` therefore uses the explicitly selected
window-scoped compatibility fallback. No core source is changed. The extension includes no internal
offset tables or client addresses and declares no hooks. A future portable native sink can implement
the same interface if wxl-core later publishes a typed game-input capability.
