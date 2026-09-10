#pragma once

#include "bindings/Bindings.hpp"
#include "bridge/BindingCapture.hpp"
#include "camera/CameraController.hpp"
#include "config/Config.hpp"
#include "controller/ControllerTypes.hpp"
#include "input/ModifierController.hpp"
#include "movement/MovementController.hpp"
#include "persistence/BindingStore.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

struct WXL_Api;

namespace wxl::controller {
class SdlControllerBackend;
class NativeGameAdapter;

enum class RuntimeResult {
    Ok,
    InvalidArgument,
    Unsupported,
    NoCharacter,
    CaptureActive,
    SaveFailed
};

class FeatureController final : private MovementSink, private CameraSink, private BindingSink {
  public:
    FeatureController(const WXL_Api &api, Config config, BindingStore store,
                      std::filesystem::path configPath, std::filesystem::path bindingsPath);
    ~FeatureController();
    bool Initialize() noexcept;
    void OnUpdate(float deltaSeconds, std::uint32_t timeMs) noexcept;
    void OnWorldEnter() noexcept;
    void OnWorldLeave(const char *reason) noexcept;
    void OnFocus(bool focused) noexcept;
    void CancelAll(const char *reason) noexcept;
    bool BeginBindingCapture() noexcept;
    void CancelBindingCapture() noexcept;
    RuntimeResult SetBinding(bool characterScope, Layer layer, Button button,
                             const Binding &binding);
    RuntimeResult ResetBinding(bool characterScope, Layer layer, Button button);
    RuntimeResult ResetLayer(bool characterScope, Layer layer);
    RuntimeResult ResetProfile(bool characterScope);
    RuntimeResult SetCharacter(std::string realm, std::string character);
    RuntimeResult ClearCharacter();
    RuntimeResult SetEnabled(bool enabled);
    RuntimeResult SetOption(const std::string &name, double value, bool booleanValue,
                            bool valueIsBoolean);
    [[nodiscard]] std::optional<double> GetNumericOption(const std::string &name) const noexcept;
    [[nodiscard]] std::optional<bool> GetBooleanOption(const std::string &name) const noexcept;
    [[nodiscard]] std::optional<ResolvedBinding> GetBinding(bool characterScope, Layer layer,
                                                            Button button) const;
    void SetTextEntryState(bool known, bool active) noexcept;
    void SetEffectiveActionSlots(const std::array<unsigned, 12> &slots, bool valid,
                                 unsigned page) noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] const DeviceInfo *CurrentDevice() const noexcept;
    [[nodiscard]] bool InWorld() const noexcept {
        return inWorld_;
    }
    [[nodiscard]] bool Focused() const noexcept {
        return focused_;
    }
    [[nodiscard]] bool OverlayOpen() const noexcept;
    [[nodiscard]] bool RuntimeReady() const noexcept {
        return input_ != nullptr;
    }
    [[nodiscard]] bool TextEntryKnown() const noexcept {
        return textEntryKnown_;
    }
    [[nodiscard]] bool TextEntryActive() const noexcept {
        return textEntryActive_;
    }
    [[nodiscard]] Layer CurrentLayer() const noexcept {
        return modifiers_.CurrentLayer();
    }
    [[nodiscard]] bool LeftModifierActive() const noexcept {
        return modifiers_.LeftActive();
    }
    [[nodiscard]] bool RightModifierActive() const noexcept {
        return modifiers_.RightActive();
    }
    [[nodiscard]] bool CaptureActive() const noexcept {
        return capture_.Active();
    }
    [[nodiscard]] std::optional<Button> CapturedButton() const noexcept {
        return capture_.Captured();
    }
    [[nodiscard]] const Snapshot &CurrentSnapshot() const noexcept {
        return current_;
    }
    [[nodiscard]] const char *CancellationReason() const noexcept {
        return cancellationReason_.c_str();
    }
    [[nodiscard]] const Config &Settings() const noexcept {
        return config_;
    }
    [[nodiscard]] const std::string &Realm() const noexcept {
        return realm_;
    }
    [[nodiscard]] const std::string &Character() const noexcept {
        return character_;
    }
    [[nodiscard]] bool HasCharacter() const noexcept {
        return identity_.has_value();
    }
    [[nodiscard]] bool EffectiveActionSlotsValid() const noexcept {
        return effectiveActionSlotsValid_;
    }
    [[nodiscard]] const std::array<unsigned, 12> &EffectiveActionSlots() const noexcept {
        return effectiveActionSlots_;
    }
    [[nodiscard]] unsigned ActionPage() const noexcept {
        return actionPage_;
    }

  private:
    void SetMovement(Movement movement, bool down) noexcept override;
    bool Begin(CameraPath path) noexcept override;
    bool Move(float horizontal, float vertical) noexcept override;
    void End() noexcept override;
    bool Press(const Binding &binding) noexcept override;
    void Release(const Binding &binding) noexcept override;
    bool Neutral(const Snapshot &snapshot) const noexcept;
    static bool ValidBindingKey(Layer layer, Button button) noexcept;
    static bool ValidIdentityPart(const std::string &value) noexcept;
    RuntimeResult CommitStore(BindingStore candidate, const char *reason);
    RuntimeResult CommitConfig(Config candidate, const char *reason);
    void RebuildBindings();
    void ApplyConfig() noexcept;
    [[nodiscard]] std::optional<unsigned> ResolveActionSlot(unsigned logical) const noexcept;

    const WXL_Api &api_;
    Config config_;
    BindingStore store_;
    std::filesystem::path configPath_;
    std::filesystem::path bindingsPath_;
    std::optional<std::string> identity_;
    std::string realm_;
    std::string character_;
    BindingMap bindings_;
    MovementController movement_;
    CameraController camera_;
    ModifierController modifiers_;
    BindingCapture capture_;
    BindingController bindingController_;
    std::unique_ptr<SdlControllerBackend> backend_;
    std::unique_ptr<NativeGameAdapter> input_;
    Snapshot current_{};
    Snapshot previous_{};
    bool inWorld_{};
    bool focused_{true};
    bool textEntryKnown_{};
    bool textEntryActive_{};
    bool waitingForNeutral_{true};
    bool effectiveActionSlotsValid_{};
    std::array<unsigned, 12> effectiveActionSlots_{};
    unsigned actionPage_{};
    float deltaSeconds_{};
    std::string cancellationReason_{"startup"};
};

} // namespace wxl::controller
