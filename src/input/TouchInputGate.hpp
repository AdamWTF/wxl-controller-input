#pragma once

#include <cstdint>

namespace wxl::controller {

class TouchInputGate final {
  public:
    bool Begin(std::uint32_t pointerId) noexcept {
        if (active_)
            return false;
        active_ = true;
        pointerId_ = pointerId;
        waitingForNeutral_ = true;
        guardUntil_ = 0;
        return true;
    }

    bool End(std::uint32_t pointerId, std::uint64_t now) noexcept {
        if (!active_ || pointerId != pointerId_)
            return false;
        active_ = false;
        pointerId_ = 0;
        guardUntil_ = now + kPromotionGuardMilliseconds;
        return true;
    }

    bool Allow(std::uint64_t now, bool neutral) noexcept {
        if (active_ || now < guardUntil_)
            return false;
        if (waitingForNeutral_ && neutral)
            waitingForNeutral_ = false;
        return !waitingForNeutral_;
    }

    void Cancel() noexcept {
        active_ = false;
        pointerId_ = 0;
        guardUntil_ = 0;
        waitingForNeutral_ = true;
    }

    [[nodiscard]] bool Active() const noexcept { return active_; }
    [[nodiscard]] bool WaitingForNeutral() const noexcept { return waitingForNeutral_; }

  private:
    static constexpr std::uint64_t kPromotionGuardMilliseconds = 250;
    bool active_{};
    bool waitingForNeutral_{};
    std::uint32_t pointerId_{};
    std::uint64_t guardUntil_{};
};

} // namespace wxl::controller
