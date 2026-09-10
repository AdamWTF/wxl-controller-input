#include "camera/CameraController.hpp"

namespace wxl::controller {

void CameraController::Update(float x, float y) noexcept {
    const Vec2 value = ApplyRadialDeadzone(x, y, deadzone_);
    const bool displaced = value.x != 0.0F || value.y != 0.0F;
    if (path_ == CameraPath::Disabled || !displaced) {
        Cancel();
        return;
    }
    if (!active_) {
        active_ = sink_.Begin(path_);
        if (!active_) return;
    }
    const float outputY = value.y * vertical_ * (invertY_ ? -1.0F : 1.0F);
    if (!sink_.Move(value.x * horizontal_, outputY)) Cancel();
}

void CameraController::Cancel() noexcept {
    if (active_) sink_.End();
    active_ = false;
}

} // namespace wxl::controller

