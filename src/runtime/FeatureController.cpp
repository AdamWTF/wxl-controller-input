#include "runtime/FeatureController.hpp"

#include "bindings/ActionSlots.hpp"
#include "controller/SdlControllerBackend.hpp"
#include "game/NativeGameAdapter.hpp"
#include "wxl/PluginApi.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>

namespace wxl::controller {

FeatureController::FeatureController(const WXL_Api &api, Config config, BindingStore store,
                                     std::filesystem::path configPath,
                                     std::filesystem::path bindingsPath)
    : api_(api), config_(config), store_(std::move(store)), configPath_(std::move(configPath)),
      bindingsPath_(std::move(bindingsPath)), bindings_(store_.Effective(std::nullopt)),
      movement_(*this, config.movementDeadzone),
      camera_(*this, CameraPath::MouseFallback, config.cameraDeadzone,
              config.cameraHorizontalSensitivity, config.cameraVerticalSensitivity,
              config.invertCameraY),
      modifiers_(config.triggerActivateThreshold, config.triggerReleaseThreshold),
      bindingController_(*this, bindings_) {
    for (unsigned i = 0; i < effectiveActionSlots_.size(); ++i)
        effectiveActionSlots_[i] = i + 1;
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
    input_ = std::make_unique<NativeGameAdapter>();
    api_.Log(WXL_LOG_INFO, "controller-input",
             "native build-12340 movement/action output and synchronous camera fallback active");
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

void FeatureController::OnUpdate(float deltaSeconds, std::uint32_t timeMs) noexcept {
    if (!backend_)
        return;
    deltaSeconds_ = deltaSeconds;
    if (input_)
        input_->SetTime(timeMs);
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
    const bool contextActive = config_.enabled && inWorld_ && focused_ && !api_.UiIsOpen() &&
                               textEntryKnown_ && !textEntryActive_;
    if (!contextActive) {
        const char *reason = api_.UiIsOpen()             ? "overlay takeover"
                             : textEntryActive_          ? "text entry takeover"
                             : !textEntryKnown_          ? "text entry context unavailable"
                                                         : "inactive gameplay context";
        CancelAll(reason);
        previous_ = next;
        return;
    }
    if (capture_.Active()) {
        capture_.Update(next);
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
    api_.Log(WXL_LOG_INFO, "controller-input",
             "world rendering detected; gameplay dispatch enabled");
}
void FeatureController::OnWorldLeave(const char *reason) noexcept {
    inWorld_ = false;
    CancelAll(reason);
    api_.Log(WXL_LOG_INFO, "controller-input", "world leave; gameplay dispatch disabled");
}
void FeatureController::OnFocus(bool focused) noexcept {
    focused_ = focused;
    if (!focused)
        CancelAll("focus loss");
}

bool FeatureController::OverlayOpen() const noexcept {
    return api_.UiIsOpen() != 0;
}

void FeatureController::CancelAll(const char *reason) noexcept {
    movement_.Cancel();
    camera_.Cancel();
    bindingController_.Cancel();
    modifiers_.Cancel();
    capture_.Cancel();
    waitingForNeutral_ = true;
    previous_ = {};
    cancellationReason_ = reason ? reason : "unknown";
}

bool FeatureController::BeginBindingCapture() noexcept {
    if (capture_.Active() || !config_.enabled || !inWorld_ || !focused_ || !Connected() ||
        api_.UiIsOpen() || !textEntryKnown_ || textEntryActive_)
        return false;
    CancelAll("binding capture");
    capture_.Begin();
    api_.Log(WXL_LOG_INFO, "controller-input", "binding capture started");
    return true;
}

void FeatureController::CancelBindingCapture() noexcept {
    if (!capture_.Active())
        return;
    capture_.Cancel();
    CancelAll("binding capture cancelled");
    api_.Log(WXL_LOG_INFO, "controller-input", "binding capture stopped");
}

bool FeatureController::ValidBindingKey(Layer layer, Button button) noexcept {
    if (button == Button::Menu || button >= Button::Count)
        return false;
    return Index(button) <= Index(Button::DPadLeft) || layer == Layer::Base;
}

bool FeatureController::ValidIdentityPart(const std::string &value) noexcept {
    if (value.empty() || value.size() > 127)
        return false;
    return std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20; });
}

RuntimeResult FeatureController::CommitStore(BindingStore candidate, const char *reason) {
    if (!candidate.SaveAtomic(bindingsPath_))
        return RuntimeResult::SaveFailed;
    CancelAll(reason);
    store_ = std::move(candidate);
    RebuildBindings();
    return RuntimeResult::Ok;
}

RuntimeResult FeatureController::CommitConfig(Config candidate, const char *reason) {
    if (!SaveConfigAtomic(configPath_, candidate))
        return RuntimeResult::SaveFailed;
    CancelAll(reason);
    config_ = candidate;
    ApplyConfig();
    return RuntimeResult::Ok;
}

void FeatureController::RebuildBindings() {
    bindings_ = store_.Effective(identity_);
}

void FeatureController::ApplyConfig() noexcept {
    movement_.SetDeadzone(config_.movementDeadzone);
    camera_.Configure(config_.cameraDeadzone, config_.cameraHorizontalSensitivity,
                      config_.cameraVerticalSensitivity, config_.invertCameraY);
    modifiers_.Configure(config_.triggerActivateThreshold, config_.triggerReleaseThreshold);
}

RuntimeResult FeatureController::SetBinding(bool characterScope, Layer layer, Button button,
                                            const Binding &binding) {
    if (!ValidBindingKey(layer, button) || !IsValid(binding))
        return RuntimeResult::InvalidArgument;
    if (characterScope && !identity_)
        return RuntimeResult::NoCharacter;
    if (capture_.Active()) {
        const auto captured = capture_.Captured();
        if (!captured || *captured != button)
            return RuntimeResult::CaptureActive;
    }
    BindingStore candidate = store_;
    if (characterScope)
        candidate.SetCharacter(*identity_, {layer, button}, binding);
    else
        candidate.SetGlobal({layer, button}, binding);
    return CommitStore(std::move(candidate), "binding changed");
}

RuntimeResult FeatureController::ResetBinding(bool characterScope, Layer layer,
                                              Button button) {
    if (!ValidBindingKey(layer, button))
        return RuntimeResult::InvalidArgument;
    if (capture_.Active())
        return RuntimeResult::CaptureActive;
    if (characterScope && !identity_)
        return RuntimeResult::NoCharacter;
    BindingStore candidate = store_;
    if (characterScope)
        candidate.ResetCharacter(*identity_, {layer, button});
    else
        candidate.ResetGlobal({layer, button});
    return CommitStore(std::move(candidate), "binding reset");
}

RuntimeResult FeatureController::ResetLayer(bool characterScope, Layer layer) {
    if (capture_.Active())
        return RuntimeResult::CaptureActive;
    if (characterScope && !identity_)
        return RuntimeResult::NoCharacter;
    BindingStore candidate = store_;
    candidate.ResetLayer(characterScope ? identity_ : std::nullopt, layer);
    return CommitStore(std::move(candidate), "layer reset");
}

RuntimeResult FeatureController::ResetProfile(bool characterScope) {
    if (capture_.Active())
        return RuntimeResult::CaptureActive;
    if (characterScope && !identity_)
        return RuntimeResult::NoCharacter;
    BindingStore candidate = store_;
    if (characterScope)
        candidate.ResetCharacterAll(*identity_);
    else
        candidate.ResetGlobalAll();
    return CommitStore(std::move(candidate), "profile reset");
}

RuntimeResult FeatureController::SetCharacter(std::string realm, std::string character) {
    if (!ValidIdentityPart(realm) || !ValidIdentityPart(character))
        return RuntimeResult::InvalidArgument;
    CancelAll("character profile changed");
    realm_ = std::move(realm);
    character_ = std::move(character);
    identity_ = std::to_string(realm_.size()) + ":" + realm_ + character_;
    RebuildBindings();
    return RuntimeResult::Ok;
}

RuntimeResult FeatureController::ClearCharacter() {
    CancelAll("character profile cleared");
    identity_.reset();
    realm_.clear();
    character_.clear();
    RebuildBindings();
    return RuntimeResult::Ok;
}

RuntimeResult FeatureController::SetEnabled(bool enabled) {
    Config candidate = config_;
    candidate.enabled = enabled;
    return CommitConfig(candidate, enabled ? "controller enabled" : "controller disabled");
}

RuntimeResult FeatureController::SetOption(const std::string &name, double value, bool booleanValue,
                                           bool valueIsBoolean) {
    Config candidate = config_;
    if (name == "InvertCameraY") {
        if (!valueIsBoolean)
            return RuntimeResult::InvalidArgument;
        candidate.invertCameraY = booleanValue;
    } else {
        if (valueIsBoolean || !std::isfinite(value))
            return RuntimeResult::InvalidArgument;
        const float number = static_cast<float>(value);
        if (name == "MovementDeadzone") {
            if (number < 0 || number > 0.95F)
                return RuntimeResult::InvalidArgument;
            candidate.movementDeadzone = number;
        } else if (name == "CameraDeadzone") {
            if (number < 0 || number > 0.95F)
                return RuntimeResult::InvalidArgument;
            candidate.cameraDeadzone = number;
        } else if (name == "CameraHorizontalSensitivity") {
            if (number < 0.05F || number > 10.0F)
                return RuntimeResult::InvalidArgument;
            candidate.cameraHorizontalSensitivity = number;
        } else if (name == "CameraVerticalSensitivity") {
            if (number < 0.05F || number > 10.0F)
                return RuntimeResult::InvalidArgument;
            candidate.cameraVerticalSensitivity = number;
        } else if (name == "TriggerActivateThreshold") {
            if (number < 0.01F || number > 1.0F || candidate.triggerReleaseThreshold > number)
                return RuntimeResult::InvalidArgument;
            candidate.triggerActivateThreshold = number;
        } else if (name == "TriggerReleaseThreshold") {
            if (number < 0 || number > candidate.triggerActivateThreshold)
                return RuntimeResult::InvalidArgument;
            candidate.triggerReleaseThreshold = number;
        } else {
            return RuntimeResult::Unsupported;
        }
    }
    return CommitConfig(candidate, "controller option changed");
}

std::optional<double> FeatureController::GetNumericOption(const std::string &name) const noexcept {
    if (name == "MovementDeadzone")
        return config_.movementDeadzone;
    if (name == "CameraDeadzone")
        return config_.cameraDeadzone;
    if (name == "CameraHorizontalSensitivity")
        return config_.cameraHorizontalSensitivity;
    if (name == "CameraVerticalSensitivity")
        return config_.cameraVerticalSensitivity;
    if (name == "TriggerActivateThreshold")
        return config_.triggerActivateThreshold;
    if (name == "TriggerReleaseThreshold")
        return config_.triggerReleaseThreshold;
    return std::nullopt;
}

std::optional<bool> FeatureController::GetBooleanOption(const std::string &name) const noexcept {
    if (name == "InvertCameraY")
        return config_.invertCameraY;
    return std::nullopt;
}

std::optional<ResolvedBinding> FeatureController::GetBinding(bool characterScope, Layer layer,
                                                            Button button) const {
    if (!ValidBindingKey(layer, button) && button != Button::Menu)
        return std::nullopt;
    if (characterScope && !identity_)
        return std::nullopt;
    return store_.Resolve(characterScope ? identity_ : std::nullopt, {layer, button});
}

void FeatureController::SetTextEntryState(bool known, bool active) noexcept {
    if (known == textEntryKnown_ && active == textEntryActive_)
        return;
    textEntryKnown_ = known;
    textEntryActive_ = known && active;
    if (!known || active)
        CancelAll(!known ? "text entry context unavailable" : "text entry takeover");
}

void FeatureController::SetEffectiveActionSlots(const std::array<unsigned, 12> &slots, bool valid,
                                                unsigned page) noexcept {
    if (valid && std::any_of(slots.begin(), slots.end(),
                            [](unsigned slot) { return slot < 1 || slot > 120; }))
        valid = false;
    if (effectiveActionSlotsValid_ != valid || effectiveActionSlots_ != slots)
        bindingController_.Cancel();
    effectiveActionSlots_ = slots;
    effectiveActionSlotsValid_ = valid;
    actionPage_ = valid ? page : 0;
}

std::optional<unsigned> FeatureController::ResolveActionSlot(unsigned logical) const noexcept {
    return ResolveEffectiveActionSlot(logical, effectiveActionSlots_, effectiveActionSlotsValid_);
}

bool FeatureController::Connected() const noexcept {
    return backend_ && backend_->Active().has_value();
}
const DeviceInfo *FeatureController::CurrentDevice() const noexcept {
    return Connected() ? &*backend_->Active() : nullptr;
}

void FeatureController::SetMovement(Movement movement, bool down) noexcept {
    if (input_)
        input_->SetMovement(movement, down);
}
bool FeatureController::Begin(CameraPath path) noexcept {
    return path == CameraPath::MouseFallback && input_ && input_->BeginCamera();
}
bool FeatureController::Move(float horizontal, float vertical) noexcept {
    return input_ && input_->MoveCamera(horizontal, vertical, deltaSeconds_);
}
void FeatureController::End() noexcept {
    if (input_)
        input_->EndCamera();
}
bool FeatureController::Press(const Binding &binding) noexcept {
    if (!input_)
        return false;
    if (const auto *action = std::get_if<ActionSlot>(&binding)) {
        const auto effective = ResolveActionSlot(action->slot);
        return effective && input_->Press(ActionSlot{*effective});
    }
    return input_->Press(binding);
}
void FeatureController::Release(const Binding &binding) noexcept {
    if (input_)
        input_->Release(binding);
}

} // namespace wxl::controller
