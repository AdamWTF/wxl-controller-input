#include "bridge/BindingCapture.hpp"

#include <cmath>

namespace wxl::controller {

void BindingCapture::Begin() noexcept {
    active_ = true;
    waitingForNeutral_ = true;
    captured_.reset();
    previous_.fill(false);
}

void BindingCapture::Cancel() noexcept {
    active_ = false;
    waitingForNeutral_ = false;
    captured_.reset();
    previous_.fill(false);
}

bool BindingCapture::Neutral(const Snapshot &snapshot) noexcept {
    constexpr float epsilon = 0.02F;
    if (std::abs(snapshot.leftX) > epsilon || std::abs(snapshot.leftY) > epsilon ||
        std::abs(snapshot.rightX) > epsilon || std::abs(snapshot.rightY) > epsilon ||
        snapshot.leftTrigger > epsilon || snapshot.rightTrigger > epsilon)
        return false;
    for (bool down : snapshot.buttons)
        if (down)
            return false;
    return true;
}

void BindingCapture::Update(const Snapshot &snapshot) noexcept {
    if (!active_ || captured_)
        return;
    if (waitingForNeutral_) {
        if (!Neutral(snapshot))
            return;
        waitingForNeutral_ = false;
        previous_ = snapshot.buttons;
        return;
    }
    for (std::size_t i = 0; i < snapshot.buttons.size(); ++i) {
        if (snapshot.buttons[i] && !previous_[i]) {
            captured_ = static_cast<Button>(i);
            break;
        }
    }
    previous_ = snapshot.buttons;
}

} // namespace wxl::controller
