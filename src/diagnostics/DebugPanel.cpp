#include "diagnostics/DebugPanel.hpp"

#include "input/WindowInputSink.hpp"
#include "runtime/FeatureController.hpp"
#include "wxl/PluginApi.h"

#include <cstdio>
#include <string>

namespace wxl::controller {
namespace {
constexpr const char *ButtonName(Button button) {
    constexpr const char *names[]{
        "South", "East", "West", "North", "DPadUp", "DPadRight", "DPadDown", "DPadLeft",
        "LB", "RB", "L3", "R3", "Back", "Start", "Guide", "Misc1",
        "RightPaddle1", "RightPaddle2", "LeftPaddle1", "LeftPaddle2"};
    return names[Index(button)];
}

constexpr const char *KeyName(Key key) {
    constexpr const char *names[]{
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "LShift", "LCtrl", "W", "A", "S", "D", "H", "V", "Num*", "Num+",
        "Num0", "Num1", "Num2", "Num3", "Num4"};
    return names[Index(key)];
}
}

void DrawDebugPanel(const WXL_Api &api, const FeatureController &controller) noexcept {
    char text[256]{};
    api.UiText("Status: native ConsolePort compatibility backend");
    std::snprintf(text, sizeof(text), "SDL: %s, detected=%d, Controller 1=%s",
                  controller.SdlReady() ? "ready" : "disabled", controller.DetectedCount(),
                  controller.Connected() ? "connected" : "unavailable");
    api.UiText(text);
    if (const auto *device = controller.CurrentDevice()) {
        std::snprintf(text, sizeof(text), "Device: %s (%s)", device->name.c_str(),
                      device->family.c_str());
        api.UiText(text);
        std::snprintf(text, sizeof(text), "Identity: %s", device->stableId.c_str());
        api.UiText(text);
    }
    std::snprintf(text, sizeof(text), "Context: world=%s focus=%s overlay=%s neutral-gate=%s",
                  controller.InWorld() ? "yes" : "no", controller.Focused() ? "yes" : "no",
                  controller.OverlayOpen() ? "open" : "closed",
                  controller.WaitingForNeutral() ? "waiting" : "clear");
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Mouse gate: touch=%s neutral=%s camera=%s",
                  controller.TouchActive() ? "active" : "idle",
                  controller.MouseWaitingForNeutral() ? "waiting" : "clear",
                  controller.LogicalState().cameraActive ? "active" : "idle");
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Input sink: %s (%s)", WindowInputSink::Name(),
                  controller.InputReady() ? "ready" : "waiting for WoW window");
    api.UiText(text);

    const auto &snapshot = controller.CurrentSnapshot();
    std::snprintf(text, sizeof(text), "Sticks: left %.1f, %.1f  right %.1f, %.1f", snapshot.leftX,
                  snapshot.leftY, snapshot.rightX, snapshot.rightY);
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Triggers: LT %.1f / %.1f  RT %.1f / %.1f",
                  snapshot.leftTrigger, controller.Settings().leftTriggerThreshold,
                  snapshot.rightTrigger, controller.Settings().rightTriggerThreshold);
    api.UiText(text);

    std::string pressed = "Physical:";
    for (std::size_t i = 0; i < snapshot.buttons.size(); ++i) {
        if (snapshot.buttons[i]) {
            pressed.push_back(' ');
            pressed += ButtonName(static_cast<Button>(i));
        }
    }
    if (pressed == "Physical:")
        pressed += " none";
    api.UiText(pressed.c_str());

    const auto &logical = controller.LogicalState();
    std::string keys = "Logical keys:";
    for (std::size_t i = 0; i < logical.keys.size(); ++i) {
        if (logical.keys[i]) {
            keys.push_back(' ');
            keys += KeyName(static_cast<Key>(i));
        }
    }
    if (keys == "Logical keys:")
        keys += " none";
    api.UiText(keys.c_str());
    std::snprintf(text, sizeof(text), "Pointer output: %.2f, %.2f  LMB=%s RMB=%s",
                  logical.pointerX, logical.pointerY,
                  logical.mouseButtons[Index(MouseButton::Left)] ? "down" : "up",
                  logical.mouseButtons[Index(MouseButton::Right)] ? "down" : "up");
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Config: move=%.1f cursor=%.1f/%.1f/%.1f radial=%s swap=%s",
                  controller.Settings().movementThreshold, controller.Settings().cursorDeadzone,
                  controller.Settings().cursorSpeed, controller.Settings().cursorCurve,
                  controller.Settings().simpleRadial ? "simple" : "16-way",
                  controller.Settings().swapSticks ? "yes" : "no");
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Recent cancellation: %s",
                  controller.CancellationReason());
    api.UiText(text);
}

} // namespace wxl::controller
