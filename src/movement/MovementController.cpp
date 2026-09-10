#include "movement/MovementController.hpp"

#include <cmath>

namespace wxl::controller {
namespace {
constexpr float kDirectionPressThreshold = 0.35F;
constexpr float kDirectionReleaseThreshold = 0.25F;

bool DirectionActive(float value, bool active) noexcept {
    return active ? value > kDirectionReleaseThreshold : value >= kDirectionPressThreshold;
}
} // namespace

void MovementController::Update(float x, float y) noexcept {
    const Vec2 value = ApplyRadialDeadzone(x, y, deadzone_);
    std::array<bool, 4> next{
        DirectionActive(-value.y, state_[0]),
        DirectionActive(value.y, state_[1]),
        DirectionActive(-value.x, state_[2]),
        DirectionActive(value.x, state_[3]),
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
