#pragma once

#include <array>
#include <cstddef>

namespace wxl::controller {

enum class KeyTransition {
    None,
    SendDown,
    SendUp,
    Rejected
};

class KeyOwnership final {
  public:
    KeyTransition Acquire(std::size_t key, bool physicallyDown) noexcept {
        if (key >= owners_.size() || owners_[key] == static_cast<unsigned char>(0xff))
            return KeyTransition::Rejected;
        if (owners_[key]++ != 0)
            return KeyTransition::None;
        downSent_[key] = !physicallyDown;
        return physicallyDown ? KeyTransition::None : KeyTransition::SendDown;
    }

    KeyTransition Release(std::size_t key, bool physicallyDown) noexcept {
        if (key >= owners_.size() || owners_[key] == 0)
            return KeyTransition::None;
        if (--owners_[key] != 0)
            return KeyTransition::None;
        const bool sendUp = downSent_[key] && !physicallyDown;
        downSent_[key] = false;
        return sendUp ? KeyTransition::SendUp : KeyTransition::None;
    }

    [[nodiscard]] unsigned Owners(std::size_t key) const noexcept {
        return key < owners_.size() ? owners_[key] : 0;
    }

  private:
    std::array<unsigned char, 256> owners_{};
    std::array<bool, 256> downSent_{};
};

} // namespace wxl::controller
