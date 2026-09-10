#pragma once

namespace wxl::controller {

struct Vec2 {
    float x{};
    float y{};
};

Vec2 ApplyRadialDeadzone(float x, float y, float deadzone) noexcept;

} // namespace wxl::controller
