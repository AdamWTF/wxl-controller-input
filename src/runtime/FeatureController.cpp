#include "runtime/FeatureController.hpp"

#include "controller/SdlControllerBackend.hpp"
#include "wxl/PluginApi.h"

#include <cmath>

namespace wxl::controller {

FeatureController::FeatureController(const WXL_Api &api, Config config, BindingMap bindings)
    : api_(api), config_(config), bindings_(std::move(bindings)),
      movement_(*this, config.movementDeadzone),
      camera_(*this, CameraPath::Disabled, config.cameraDeadzone,
              config.cameraHorizontalSensitivity, config.cameraVerticalSensitivity,
              config.invertCameraY),
      modifiers_(config.triggerActivateThreshold, config.triggerReleaseThreshold),
      bindingController_(*this, bindings_) {
}

FeatureController::~FeatureController() {
    CancelAll("shutdown");
    api_.Log(WXL_LOG_INFO, "controller-input", "shutdown");
}

bool FeatureController::Initialize() noexcept {
    backend_ = std::make_unique<SdlControllerBackend>([this] {
        CancelAll("controller disconnect");
        current_ = {};
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 disconnected");
    });
    if (!backend_->Initialize()) {
        api_.Log(WXL_LOG_ERROR, "controller-input", "SDL gamepad initialization failed");
        return false;
    }
    api_.Log(WXL_LOG_WARN, "controller-input",
             "diagnostic mode: WarcraftXL v1.1.247 has no semantic gameplay-input interface; game "
             "output disabled");
    if (const auto &active = backend_->Active(); active) {
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 connected: %s",
                 active->name.c_str());
    }
    return true;
}

bool FeatureController::Neutral(const Snapshot &state) const noexcept {
    constexpr float epsilon = 0.02F;
    if (std::abs(state.leftX) > epsilon || std::abs(state.leftY) > epsilon ||
        std::abs(state.rightX) > epsilon || std::abs(state.rightY) > epsilon ||
        state.leftTrigger > epsilon || state.rightTrigger > epsilon)
        return false;
    for (bool down : state.buttons)
        if (down)
            return false;
    return true;
}

void FeatureController::OnUpdate() noexcept {
    if (!backend_)
        return;
    const bool connectedBefore = Connected();
    Snapshot next{};
    if (!backend_->Poll(next))
        return;
    if (!connectedBefore && Connected()) {
        const auto *device = CurrentDevice();
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 reconnected: %s",
                 device ? device->name.c_str() : "unknown");
    }
    current_ = next;
    const bool contextActive = config_.enabled && inWorld_ && focused_ && !api_.UiIsOpen();
    if (!contextActive) {
        CancelAll(api_.UiIsOpen() ? "overlay takeover" : "inactive gameplay context");
        previous_ = next;
        return;
    }
    if (waitingForNeutral_) {
        if (!Neutral(next)) {
            previous_ = next;
            return;
        }
        waitingForNeutral_ = false;
        previous_ = next;
        return;
    }

    const Layer layer = modifiers_.Update(next.leftTrigger, next.rightTrigger);
    movement_.Update(next.leftX, next.leftY);
    camera_.Update(next.rightX, next.rightY);
    for (std::size_t i = 0; i < next.buttons.size(); ++i) {
        if (next.buttons[i] != previous_.buttons[i])
            bindingController_.Update(static_cast<Button>(i), next.buttons[i], layer);
    }
    previous_ = next;
}

void FeatureController::OnWorldEnter() noexcept {
    inWorld_ = true;
    waitingForNeutral_ = true;
}
void FeatureController::OnWorldLeave(const char *reason) noexcept {
    inWorld_ = false;
    CancelAll(reason);
}
void FeatureController::OnFocus(bool focused) noexcept {
    focused_ = focused;
    if (!focused)
        CancelAll("focus loss");
}

void FeatureController::CancelAll(const char *reason) noexcept {
    movement_.Cancel();
    camera_.Cancel();
    bindingController_.Cancel();
    modifiers_.Cancel();
    waitingForNeutral_ = true;
    previous_ = {};
    cancellationReason_ = reason ? reason : "unknown";
}

bool FeatureController::Connected() const noexcept {
    return backend_ && backend_->Active().has_value();
}
const DeviceInfo *FeatureController::CurrentDevice() const noexcept {
    return Connected() ? &*backend_->Active() : nullptr;
}

// These adapters intentionally do not synthesize input. They are the narrow seams to be backed by
// a future published WarcraftXL semantic API.
void FeatureController::SetMovement(Movement, bool) noexcept {
}
bool FeatureController::Begin(CameraPath) noexcept {
    return false;
}
bool FeatureController::Move(float, float) noexcept {
    return false;
}
void FeatureController::End() noexcept {
}
bool FeatureController::Press(const Binding &) noexcept {
    return false;
}
void FeatureController::Release(const Binding &) noexcept {
}

} // namespace wxl::controller
