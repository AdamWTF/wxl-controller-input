#include "diagnostics/DebugPanel.hpp"

#include "input/Deadzone.hpp"
#include "runtime/FeatureController.hpp"
#include "wxl/PluginApi.h"

#include <cstdio>

namespace wxl::controller {

void DrawDebugPanel(const WXL_Api &api, const FeatureController &controller) noexcept {
    char text[192]{};
    api.UiText("Status: diagnostic input only (game output unavailable)");
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
    std::snprintf(text, sizeof(text), "Recent cancellation: %s", controller.CancellationReason());
    api.UiText(text);
    api.UiText("Camera path: disabled; upstream semantic API required");
}

} // namespace wxl::controller
