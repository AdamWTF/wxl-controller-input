#pragma once

#include "input/KeyOwnership.hpp"

namespace wxl::controller {

class MouseButtonOwnership final {
  public:
    KeyTransition Begin(bool physicallyDown) noexcept {
        if (owned_ || physicallyDown)
            return KeyTransition::None;
        owned_ = true;
        return KeyTransition::SendDown;
    }

    KeyTransition Ensure(bool physicallyDown) noexcept {
        return Begin(physicallyDown);
    }

    KeyTransition End(bool physicallyDown) noexcept {
        if (!owned_)
            return KeyTransition::None;
        owned_ = false;
        return physicallyDown ? KeyTransition::None : KeyTransition::SendUp;
    }

    [[nodiscard]] bool Owned() const noexcept {
        return owned_;
    }

  private:
    bool owned_{};
};

} // namespace wxl::controller
