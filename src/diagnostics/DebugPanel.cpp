#include "diagnostics/DebugPanel.hpp"

#include "bridge/LuaBridge.hpp"
#include "input/Deadzone.hpp"
#include "runtime/FeatureController.hpp"
#include "wxl/PluginApi.h"

#include <cstdio>
#include <string>

namespace wxl::controller {
namespace {
constexpr const char *ButtonName(Button button) {
    constexpr const char *names[]{"FaceSouth",    "FaceEast",      "FaceWest",  "FaceNorth",
                                  "DPadUp",       "DPadRight",     "DPadDown",  "DPadLeft",
                                  "LeftShoulder", "RightShoulder", "LeftStick", "RightStick",
                                  "View",         "Menu"};
    return names[Index(button)];
}
} // namespace

void DrawDebugPanel(const WXL_Api &api, const FeatureController &controller) noexcept {
    char text[192]{};
    api.UiText("Status: native build-12340 game output candidate");
    api.UiText(controller.Connected() ? "Controller 1: connected" : "Controller 1: unavailable");
    if (const auto *device = controller.CurrentDevice()) {
        std::snprintf(text, sizeof(text), "Device: %s (%s)", device->name.c_str(),
                      device->family.c_str());
        api.UiText(text);
    }
    std::snprintf(text, sizeof(text), "Context: world=%s focus=%s layer=%u",
                  controller.InWorld() ? "yes" : "no", controller.Focused() ? "yes" : "no",
                  static_cast<unsigned>(controller.CurrentLayer()));
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Lua bridge: %s%s%s", LuaBridge::Ready() ? "ready" : "degraded",
                  LuaBridge::Ready() ? "" : " - ",
                  LuaBridge::Ready() ? "" : LuaBridge::DegradedReason());
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Text entry: known=%s active=%s",
                  controller.TextEntryKnown() ? "yes" : "no",
                  controller.TextEntryActive() ? "yes" : "no");
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Action context: %s page=%u",
                  controller.EffectiveActionSlotsValid() ? "resolved" : "suppressed",
                  controller.ActionPage());
    api.UiText(text);
    const auto &s = controller.CurrentSnapshot();
    std::snprintf(text, sizeof(text), "Left raw: %.3f, %.3f   Right raw: %.3f, %.3f", s.leftX,
                  s.leftY, s.rightX, s.rightY);
    api.UiText(text);
    const auto left = ApplyRadialDeadzone(s.leftX, s.leftY, controller.Settings().movementDeadzone);
    const auto right =
        ApplyRadialDeadzone(s.rightX, s.rightY, controller.Settings().cameraDeadzone);
    std::snprintf(text, sizeof(text), "Processed: left %.3f, %.3f   right %.3f, %.3f", left.x,
                  left.y, right.x, right.y);
    api.UiText(text);
    std::snprintf(text, sizeof(text), "Triggers: LT %.3f  RT %.3f", s.leftTrigger, s.rightTrigger);
    api.UiText(text);
    std::string pressed = "Pressed:";
    for (std::size_t i = 0; i < s.buttons.size(); ++i) {
        if (s.buttons[i]) {
            pressed.push_back(' ');
            pressed += ButtonName(static_cast<Button>(i));
        }
    }
    if (pressed == "Pressed:")
        pressed += " none";
    api.UiText(pressed.c_str());
    if (controller.CaptureActive()) {
        const auto captured = controller.CapturedButton();
        std::snprintf(text, sizeof(text), "Binding capture: active%s%s",
                      captured ? " - captured " : " - waiting",
                      captured ? ButtonName(*captured) : "");
        api.UiText(text);
    } else {
        api.UiText("Binding capture: inactive");
    }
    std::snprintf(text, sizeof(text), "Recent cancellation: %s", controller.CancellationReason());
    api.UiText(text);
    api.UiText("Camera path: synchronous RMB compatibility fallback");
}

} // namespace wxl::controller
