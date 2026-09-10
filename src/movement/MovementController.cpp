#include "movement/MovementController.hpp"

#include <cmath>

namespace wxl::controller {

void MovementController::Update(float x, float y) noexcept {
    const Vec2 value = ApplyRadialDeadzone(x, y, deadzone_);
    constexpr float directionThreshold = 0.001F;
    std::array<bool, 4> next{
        value.y<-directionThreshold, value.y> directionThreshold,
        value.x<-directionThreshold, value.x> directionThreshold,
    };

    // All releases precede all presses, making opposite-direction transitions deterministic.
    for (std::size_t i = 0; i < state_.size(); ++i) {
        if (state_[i] && !next[i])
            sink_.SetMovement(static_cast<Movement>(i), false);
    }
    for (std::size_t i = 0; i < state_.size(); ++i) {
        if (!state_[i] && next[i])
            sink_.SetMovement(static_cast<Movement>(i), true);
    }
    state_ = next;
}

void MovementController::Cancel() noexcept {
    for (std::size_t i = 0; i < state_.size(); ++i) {
        if (state_[i])
            sink_.SetMovement(static_cast<Movement>(i), false);
    }
    state_.fill(false);
}

} // namespace wxl::controller
