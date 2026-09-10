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

The policy sinks currently have no game-side implementation. That is intentional: the pinned SDK
does not expose the needed semantics. The only live side effects are SDL Gamepad access, core log
messages, event subscriptions, and diagnostic overlay text. Physical keyboard/mouse messages are
observed only for `WM_ACTIVATEAPP`; they are never marked handled.

## Ownership

- Observed: SDL Gamepad state, WarcraftXL world lifecycle, overlay-open state, application focus.
- Owned: the SDL Gamepad subsystem and Controller 1 handle; internal logical state.
- Synthesized in 0.1.0: nothing.
- Passed untouched: physical keyboard, mouse, touch, window messages, and all game actions.

## Load contract

`WXL_Query` returns static metadata and performs no I/O, logging, enumeration, allocation, callback
registration, or SDL initialization. `WXL_Load` requires the exact ABI version and table size and
validates every function pointer before config or SDL initialization.

