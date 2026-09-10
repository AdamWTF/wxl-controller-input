# Architecture

The runtime is split into bounded, testable policy and narrow side-effect adapters.

```text
SDL3 Gamepad -> canonical Snapshot -> context/neutral gate
                                      |-> ModifierController
                                      |-> MovementController -> MovementSink
                                      |-> CameraController   -> CameraSink
                                      `-> BindingController  -> BindingSink
```

`SdlControllerBackend` initializes only `SDL_INIT_GAMEPAD`, polls from WarcraftXL's main-thread
`OnUpdate` event, translates SDL's standardized layout into physical-position-neutral names, and
owns the one open Controller 1 handle. There is no worker thread and no unbounded wait.

`ControllerSelector` chooses player index 0 when present and otherwise the first enumerated SDL
Gamepad. It records Controller 1's strongest available identity (serial, then device path, then a
GUID/vendor/product/name fallback). Disconnect never promotes another pad. Reconnect only accepts
the recorded identity. Two truly identical devices with no serial or distinct path cannot be
distinguished reliably; the fallback is deterministic but this remains a documented limitation.

`FeatureController` owns all cancellation. Focus loss, world leave, overlay takeover, disconnect,
disable, and shutdown release every policy-owned state. After cancellation, input is gated until a
fully neutral physical snapshot has been observed, preventing held controls from replaying.

`BindingStore` parses schema version 1 into temporary maps and commits them only after the complete
document is valid. Unknown future fields and malformed individual binding entries are ignored;
malformed JSON or an unsupported schema leaves the prior valid state intact. Global and character
overrides remain sparse and resolve over built-in defaults. Configuration and binding writes use a
same-directory temporary file followed by Windows write-through replacement, so a failed save does
not truncate the previous file.

`BindingCapture` first requires a completely neutral controller snapshot, then records one fresh
canonical digital down transition while all gameplay policy remains suppressed. `ControllerBridge`
publishes the versioned `wxl.controller-input` C table through the core interface registry. The
bridge reports only implemented capability flags and contains no addon visual policy.

`NativeGameAdapter` is the sole game-side implementation. It ports the earlier Companion Screen
controller's validated build-12340 calls: native movement begin/end/commit, native action-slot
execution, synchronous auxiliary key messages, and synchronous RMB camera messages. Its fixed
bindings are restricted to the exact supported executable hash and do not leak into policy code.
No WarcraftXL core source or binary is modified, and no `SendInput` activity is used.

## Ownership

- Observed: SDL Gamepad state, WarcraftXL world lifecycle, overlay-open state, application focus.
- Owned: the SDL Gamepad subsystem and Controller 1 handle; internal logical state; native movement
  states and any synchronous RMB/key state begun by this adapter.
- Synthesized in 0.1.0: synchronous, WoW-window-only auxiliary keys and RMB camera messages.
- Passed untouched: physical keyboard, mouse, touch, and unrelated window messages.

## Load contract

`WXL_Query` returns static metadata and performs no I/O, logging, enumeration, allocation, callback
registration, or SDL initialization. `WXL_Load` requires the exact ABI version and table size and
validates every function pointer before config or SDL initialization.
