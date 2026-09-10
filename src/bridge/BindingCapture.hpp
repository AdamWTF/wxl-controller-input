#pragma once

#include "controller/ControllerTypes.hpp"

#include <optional>

namespace wxl::controller {

class BindingCapture {
  public:
    void Begin() noexcept;
    void Cancel() noexcept;
    void Update(const Snapshot &snapshot) noexcept;

    [[nodiscard]] bool Active() const noexcept {
        return active_;
    }
    [[nodiscard]] bool WaitingForNeutral() const noexcept {
        return waitingForNeutral_;
    }
    [[nodiscard]] std::optional<Button> Captured() const noexcept {
        return captured_;
    }

  private:
    static bool Neutral(const Snapshot &snapshot) noexcept;

    bool active_{};
    bool waitingForNeutral_{};
    std::optional<Button> captured_;
    std::array<bool, static_cast<std::size_t>(Button::Count)> previous_{};
};

} // namespace wxl::controller
