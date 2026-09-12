#pragma once

namespace wxl::controller {

enum class OwnershipTransition { None, SendDown, ResendDown, SendUp, Blocked };

class DigitalOwnership final {
  public:
    void SetDesired(bool desired) noexcept { desired_ = desired; }
    [[nodiscard]] bool Desired() const noexcept { return desired_; }
    [[nodiscard]] bool Sent() const noexcept { return sent_; }

    OwnershipTransition Reconcile(bool physicallyDown, bool allowDown) noexcept {
        const bool physicalReleased = previousPhysical_ && !physicallyDown;
        previousPhysical_ = physicallyDown;
        if (desired_ && sent_ && physicalReleased)
            return allowDown ? OwnershipTransition::ResendDown : OwnershipTransition::Blocked;
        if (desired_ && !sent_ && !physicallyDown) {
            if (!allowDown)
                return OwnershipTransition::Blocked;
            sent_ = true;
            return OwnershipTransition::SendDown;
        }
        if (!desired_ && sent_) {
            sent_ = false;
            return physicallyDown ? OwnershipTransition::None : OwnershipTransition::SendUp;
        }
        return OwnershipTransition::None;
    }

    void Undo(OwnershipTransition transition) noexcept {
        if (transition == OwnershipTransition::SendDown)
            sent_ = false;
        else if (transition == OwnershipTransition::SendUp)
            sent_ = true;
    }

  private:
    bool desired_{};
    bool sent_{};
    bool previousPhysical_{};
};

} // namespace wxl::controller
