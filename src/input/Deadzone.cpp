#include "input/Deadzone.hpp"

#include <algorithm>
#include <cmath>

namespace wxl::controller {

Vec2 ApplyRadialDeadzone(float x, float y, float deadzone) noexcept {
    deadzone = std::clamp(deadzone, 0.0F, 0.95F);
    const float magnitude = std::sqrt(x * x + y * y);
    if (magnitude <= deadzone || magnitude == 0.0F) return {};
    const float clamped = std::min(magnitude, 1.0F);
    const float scaled = (clamped - deadzone) / (1.0F - deadzone);
    return {x / magnitude * scaled, y / magnitude * scaled};
}

} // namespace wxl::controller

