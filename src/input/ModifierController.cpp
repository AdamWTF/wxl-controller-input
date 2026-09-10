#include "input/ModifierController.hpp"

#include <algorithm>

namespace wxl::controller {

ModifierController::ModifierController(float activate, float release)
    : activate_(std::clamp(activate, 0.0F, 1.0F)),
      release_(std::clamp(release, 0.0F, activate_)) {}

void ModifierController::UpdateOne(float value, float activate, float release, bool& state) noexcept {
    if (!state && value >= activate) state = true;
    else if (state && value < release) state = false;
}

Layer ModifierController::Update(float leftTrigger, float rightTrigger) noexcept {
    UpdateOne(leftTrigger, activate_, release_, left_);
    UpdateOne(rightTrigger, activate_, release_, right_);
    return CurrentLayer();
}

void ModifierController::Cancel() noexcept { left_ = right_ = false; }

Layer ModifierController::CurrentLayer() const noexcept {
    if (left_ && right_) return Layer::LTRT;
    if (left_) return Layer::LT;
    if (right_) return Layer::RT;
    return Layer::Base;
}

} // namespace wxl::controller

