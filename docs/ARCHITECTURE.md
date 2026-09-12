# Architecture

```text
SdlControllerBackend -> Snapshot -> ConsolePortMapper -> IWoWInputSink
                                                        `-> WindowInputSink
```

`SdlControllerBackend` owns SDL's Gamepad subsystem, enumeration, one sticky Controller 1 handle,
hotplug events, and normalization to WoWpadX-compatible stick and trigger ranges. It polls once per
WarcraftXL `OnUpdate`; there is no worker, busy loop, or unbounded queue.

`ConsolePortMapper` is platform-independent policy. It owns every logical key and mouse state,
emits transitions only, computes W/A/S/D and H/V movement, applies optional axis swapping, and
calculates the specified radial right-stick curve. It contains no Windows or WoW implementation
knowledge.

`WindowInputSink` is the only side-effect adapter. It sends synchronous messages only to the WoW
window, admits new downs and motion only while that window is foreground, and never calls
`SendInput`. Camera movement uses `SetCursorPos` only while WoW is foreground, preserves fractional
movement, and restores the saved cursor when camera ownership ends.
Digital ownership observes the real keyboard/mouse state so one input source cannot incorrectly
release the other.

`FeatureController` owns lifecycle gates and `ReleaseAll()`. World/focus/overlay loss, disable,
disconnect, SDL failure, sink failure, and shutdown all converge on cleanup followed by a neutral
controller gate. Touch contact suspends the controller mouse path and requires neutral input before
rearming it. SDL failure degrades only controller support.

The extension has no Lua bridge, action-slot execution, binding profiles, client addresses, offset
headers, Pixel Bridge, or companion dependency.
