#pragma once

#include "input/Deadzone.hpp"

#include <array>

namespace wxl::controller {

enum class Movement : unsigned char {
    Forward,
    Backward,
    StrafeLeft,
    StrafeRight,
    Count
};

class MovementSink {
  public:
    virtual ~MovementSink() = default;
    virtual void SetMovement(Movement movement, bool down) noexcept = 0;
};

class MovementController {
  public:
    explicit MovementController(MovementSink &sink, float deadzone = 0.18F)
        : sink_(sink), deadzone_(deadzone) {
    }
    void Update(float x, float y) noexcept;
    void Cancel() noexcept;
    [[nodiscard]] const std::array<bool, 4> &State() const noexcept {
        return state_;
    }

  private:
    MovementSink &sink_;
    float deadzone_;
    std::array<bool, 4> state_{};
};

} // namespace wxl::controller
