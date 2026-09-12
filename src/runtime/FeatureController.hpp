#pragma once

#include "config/Config.hpp"
#include "controller/ControllerTypes.hpp"
#include "input/WoWInputSink.hpp"
#include "input/TouchInputGate.hpp"
#include "mapper/ConsolePortMapper.hpp"

#include <cstdint>
#include <memory>

struct WXL_Api;

namespace wxl::controller {
class SdlControllerBackend;
class WindowInputSink;

class FeatureController final {
  public:
    FeatureController(const WXL_Api &api, Config config);
    ~FeatureController();
    bool Initialize();
    void OnUpdate(float deltaSeconds, std::uint32_t timeMs) noexcept;
    void OnWorldEnter() noexcept;
    void OnWorldLeave(const char *reason) noexcept;
    void OnFocus(bool focused) noexcept;
    void OnWindowInput(std::uint32_t message, std::uintptr_t wparam,
                       std::uintptr_t lparam, std::uintptr_t extraInfo) noexcept;
    void ReleaseAll(const char *reason) noexcept;

    [[nodiscard]] bool Connected() const noexcept;
    [[nodiscard]] const DeviceInfo *CurrentDevice() const noexcept;
    [[nodiscard]] bool InWorld() const noexcept { return inWorld_; }
    [[nodiscard]] bool Focused() const noexcept { return focused_; }
    [[nodiscard]] bool OverlayOpen() const noexcept;
    [[nodiscard]] bool SdlReady() const noexcept;
    [[nodiscard]] bool InputReady() const noexcept;
    [[nodiscard]] int DetectedCount() const noexcept;
    [[nodiscard]] const Snapshot &CurrentSnapshot() const noexcept { return current_; }
    [[nodiscard]] const MapperState &LogicalState() const noexcept;
    [[nodiscard]] const Config &Settings() const noexcept { return config_; }
    [[nodiscard]] const char *CancellationReason() const noexcept { return cancellationReason_; }
    [[nodiscard]] bool WaitingForNeutral() const noexcept { return waitingForNeutral_; }
    [[nodiscard]] bool TouchActive() const noexcept { return touchGate_.Active(); }
    [[nodiscard]] bool MouseWaitingForNeutral() const noexcept {
        return touchGate_.WaitingForNeutral();
    }

  private:
    bool Neutral(const Snapshot &snapshot) const noexcept;
    bool MouseNeutral(const Snapshot &snapshot) const noexcept;
    void InputFailure() noexcept;
    void LogTransitions(const MapperState &before, const MapperState &after) noexcept;

    const WXL_Api &api_;
    Config config_;
    std::unique_ptr<SdlControllerBackend> backend_;
    std::unique_ptr<WindowInputSink> input_;
    std::unique_ptr<ConsolePortMapper> mapper_;
    Snapshot current_{};
    bool inWorld_{};
    bool focused_{true};
    bool waitingForNeutral_{true};
    bool inputFailureLogged_{};
    TouchInputGate touchGate_;
    const char *cancellationReason_{"startup"};
};

} // namespace wxl::controller
