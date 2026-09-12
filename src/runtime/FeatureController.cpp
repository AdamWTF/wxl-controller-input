#include "runtime/FeatureController.hpp"

#include "controller/SdlControllerBackend.hpp"
#include "input/WindowInputSink.hpp"
#include "wxl/PluginApi.h"

#include <Windows.h>
#include <windowsx.h>

#include <cmath>

namespace wxl::controller {
namespace {
const MapperState kEmptyMapperState{};
}

FeatureController::FeatureController(const WXL_Api &api, Config config)
    : api_(api), config_(config) {}

FeatureController::~FeatureController() {
    ReleaseAll("shutdown");
    if (backend_)
        backend_->Shutdown();
    api_.Log(WXL_LOG_INFO, "controller-input", "shutdown");
}

bool FeatureController::Initialize() {
    input_ = std::make_unique<WindowInputSink>();
    mapper_ = std::make_unique<ConsolePortMapper>(*input_, config_);
    backend_ = std::make_unique<SdlControllerBackend>([this] {
        ReleaseAll("controller disconnect");
        current_ = {};
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 disconnected");
    });

    api_.Log(WXL_LOG_INFO, "controller-input", "controller backend %s",
             config_.enabled ? "enabled" : "disabled by configuration");
    api_.Log(WXL_LOG_INFO, "controller-input", "SDL %s; input sink: %s",
             SdlControllerBackend::SdlVersion(), WindowInputSink::Name());
    api_.Log(WXL_LOG_INFO, "controller-input",
             "config move=%.1f triggers=%.1f/%.1f cursor=%.1f/%.1f/%.1f radial=%s swap=%s",
             config_.movementThreshold, config_.leftTriggerThreshold,
             config_.rightTriggerThreshold, config_.cursorDeadzone, config_.cursorSpeed,
             config_.cursorCurve, config_.simpleRadial ? "simple" : "16-way",
             config_.swapSticks ? "yes" : "no");
    if (!backend_->Initialize()) {
        api_.Log(WXL_LOG_ERROR, "controller-input",
                 "SDL gamepad initialization failed; controller support disabled: %s",
                 backend_->Error().c_str());
        return true;
    }
    api_.Log(WXL_LOG_INFO, "controller-input", "detected gamepads: %d",
             backend_->DetectedCount());
    if (const auto &active = backend_->Active(); active)
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 selected: %s (%s, %s)",
                 active->name.c_str(), active->family.c_str(), active->stableId.c_str());
    return true;
}

bool FeatureController::Neutral(const Snapshot &state) const noexcept {
    constexpr float stickTolerance = 2.0F;
    constexpr float triggerTolerance = 2.0F;
    if (std::abs(state.leftX) > stickTolerance || std::abs(state.leftY) > stickTolerance ||
        std::abs(state.rightX) > stickTolerance || std::abs(state.rightY) > stickTolerance ||
        state.leftTrigger > triggerTolerance || state.rightTrigger > triggerTolerance)
        return false;
    for (bool down : state.buttons)
        if (down)
            return false;
    return true;
}

bool FeatureController::MouseNeutral(const Snapshot &state) const noexcept {
    const float x = config_.swapSticks ? state.leftX : state.rightX;
    const float y = config_.swapSticks ? state.leftY : state.rightY;
    const float magnitude = std::sqrt(x * x + y * y);
    return magnitude <= config_.cursorDeadzone &&
           !state.buttons[Index(Button::LeftStick)] &&
           !state.buttons[Index(Button::RightStick)];
}

void FeatureController::InputFailure() noexcept {
    ReleaseAll("input sink failure");
    if (!inputFailureLogged_) {
        api_.Log(WXL_LOG_ERROR, "controller-input",
                 "WoW window input failed; output paused until a neutral retry");
        inputFailureLogged_ = true;
    }
}

void FeatureController::LogTransitions(const MapperState &before,
                                       const MapperState &after) noexcept {
    if (!config_.debugLogging)
        return;
    for (std::size_t i = 0; i < before.keys.size(); ++i) {
        if (before.keys[i] != after.keys[i])
            api_.Log(WXL_LOG_DEBUG, "controller-input", "logical key %u %s",
                     static_cast<unsigned>(i), after.keys[i] ? "down" : "up");
    }
    for (std::size_t i = 0; i < before.mouseButtons.size(); ++i) {
        if (before.mouseButtons[i] != after.mouseButtons[i])
            api_.Log(WXL_LOG_DEBUG, "controller-input", "logical mouse %u %s",
                     static_cast<unsigned>(i), after.mouseButtons[i] ? "down" : "up");
    }
}

void FeatureController::OnUpdate(float, std::uint32_t) noexcept {
    if (!backend_ || !backend_->Initialized() || !mapper_ || !input_)
        return;

    const bool connectedBefore = Connected();
    Snapshot next{};
    if (!backend_->Poll(next)) {
        input_->Reconcile(false);
        return;
    }
    if (!connectedBefore && Connected()) {
        const auto *device = CurrentDevice();
        api_.Log(WXL_LOG_INFO, "controller-input", "Controller 1 reconnected: %s",
                 device ? device->name.c_str() : "unknown");
    }
    current_ = next;

    const bool contextActive = config_.enabled && inWorld_ && focused_ && !api_.UiIsOpen();
    if (!contextActive) {
        const char *reason = api_.UiIsOpen() ? "overlay takeover"
                             : !config_.enabled ? "controller disabled"
                             : !focused_ ? "focus loss"
                                         : "inactive gameplay context";
        ReleaseAll(reason);
        return;
    }
    if (waitingForNeutral_) {
        if (!Neutral(next))
            return;
        waitingForNeutral_ = false;
        inputFailureLogged_ = false;
        return;
    }
    const bool allowMouse = touchGate_.Allow(GetTickCount64(), MouseNeutral(next));

    const MapperState before = mapper_->State();
    if (!input_->Reconcile(true) || !mapper_->Update(next, allowMouse)) {
        InputFailure();
        return;
    }
    LogTransitions(before, mapper_->State());
}

void FeatureController::OnWorldEnter() noexcept {
    inWorld_ = true;
    waitingForNeutral_ = true;
    api_.Log(WXL_LOG_INFO, "controller-input",
             "world rendering detected; controller dispatch enabled after neutral");
}

void FeatureController::OnWorldLeave(const char *reason) noexcept {
    inWorld_ = false;
    ReleaseAll(reason);
    api_.Log(WXL_LOG_INFO, "controller-input", "world leave; controller dispatch disabled");
}

void FeatureController::OnFocus(bool focused) noexcept {
    focused_ = focused;
    if (!focused)
        ReleaseAll("focus loss");
}

void FeatureController::OnWindowInput(std::uint32_t message, std::uintptr_t wparam,
                                      std::uintptr_t lparam, std::uintptr_t extraInfo) noexcept {
    if (input_)
        input_->ObserveWindowMessage(message, wparam, lparam, extraInfo);

    if (message == WM_POINTERDOWN) {
        const auto pointerId = GET_POINTERID_WPARAM(wparam);
        POINTER_INPUT_TYPE type = PT_POINTER;
        if (GetPointerType(pointerId, &type) && type == PT_TOUCH &&
            touchGate_.Begin(pointerId)) {
            if (mapper_)
                mapper_->CancelMouse();
            cancellationReason_ = "touch takeover";
        }
    } else if ((message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED) &&
               touchGate_.End(GET_POINTERID_WPARAM(wparam), GetTickCount64())) {
        cancellationReason_ = "touch release; waiting for neutral";
    } else if (message == WM_KILLFOCUS || message == WM_CANCELMODE ||
               message == WM_DESTROY || message == WM_NCDESTROY) {
        touchGate_.Cancel();
    }
}

void FeatureController::ReleaseAll(const char *reason) noexcept {
    if (mapper_)
        mapper_->ReleaseAll();
    else if (input_)
        input_->ReleaseAll();
    touchGate_.Cancel();
    waitingForNeutral_ = true;
    cancellationReason_ = reason ? reason : "unknown";
}

bool FeatureController::Connected() const noexcept {
    return backend_ && backend_->Active().has_value();
}
const DeviceInfo *FeatureController::CurrentDevice() const noexcept {
    return Connected() ? &*backend_->Active() : nullptr;
}
bool FeatureController::OverlayOpen() const noexcept { return api_.UiIsOpen() != 0; }
bool FeatureController::SdlReady() const noexcept {
    return backend_ && backend_->Initialized();
}
bool FeatureController::InputReady() const noexcept { return input_ && input_->Ready(); }
int FeatureController::DetectedCount() const noexcept {
    return backend_ ? backend_->DetectedCount() : 0;
}
const MapperState &FeatureController::LogicalState() const noexcept {
    return mapper_ ? mapper_->State() : kEmptyMapperState;
}

} // namespace wxl::controller
