#pragma once

#include "bindings/Bindings.hpp"
#include "camera/CameraController.hpp"
#include "config/Config.hpp"
#include "controller/ControllerTypes.hpp"
#include "input/ModifierController.hpp"
#include "movement/MovementController.hpp"

#include <memory>
#include <string>

struct WXL_Api;

namespace wxl::controller {
class SdlControllerBackend;

class FeatureController final : private MovementSink, private CameraSink, private BindingSink {
public:
    FeatureController(const WXL_Api& api, Config config);
    ~FeatureController();
    bool Initialize() noexcept;
    void OnUpdate() noexcept;
    void OnWorldEnter() noexcept;
    void OnWorldLeave(const char* reason) noexcept;
    void OnFocus(bool focused) noexcept;
    void CancelAll(const char* reason) noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] const DeviceInfo* CurrentDevice() const noexcept;
    [[nodiscard]] bool InWorld() const noexcept { return inWorld_; }
    [[nodiscard]] bool Focused() const noexcept { return focused_; }
    [[nodiscard]] Layer CurrentLayer() const noexcept { return modifiers_.CurrentLayer(); }
    [[nodiscard]] const Snapshot& CurrentSnapshot() const noexcept { return current_; }
    [[nodiscard]] const char* CancellationReason() const noexcept { return cancellationReason_.c_str(); }
    [[nodiscard]] const Config& Settings() const noexcept { return config_; }

private:
    void SetMovement(Movement movement, bool down) noexcept override;
    bool Begin(CameraPath path) noexcept override;
    bool Move(float horizontal, float vertical) noexcept override;
    void End() noexcept override;
    bool Press(const Binding& binding) noexcept override;
    void Release(const Binding& binding) noexcept override;
    bool Neutral(const Snapshot& snapshot) const noexcept;

    const WXL_Api& api_;
    Config config_;
    BindingMap bindings_{BuiltInBindings()};
    MovementController movement_;
    CameraController camera_;
    ModifierController modifiers_;
    BindingController bindingController_;
    std::unique_ptr<SdlControllerBackend> backend_;
    Snapshot current_{};
    Snapshot previous_{};
    bool inWorld_{};
    bool focused_{true};
    bool waitingForNeutral_{true};
    std::string cancellationReason_{"startup"};
};

} // namespace wxl::controller
