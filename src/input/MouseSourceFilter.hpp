#pragma once

#include <cstdint>

namespace wxl::controller {

inline constexpr std::uintptr_t kTouchSyntheticMouseTag = 0x57584C54; // "WXLT"
inline constexpr std::uintptr_t kPointerMessageSignature = 0xFF515700;
inline constexpr std::uintptr_t kPointerMessageSignatureMask = 0xFFFFFF00;
inline constexpr std::uintptr_t kTouchMessageFlag = 0x80;

inline constexpr bool IsTouchOwnedMouseExtraInfo(std::uintptr_t extraInfo) noexcept {
    return extraInfo == kTouchSyntheticMouseTag ||
           ((extraInfo & kPointerMessageSignatureMask) == kPointerMessageSignature &&
            (extraInfo & kTouchMessageFlag) != 0);
}

} // namespace wxl::controller
