#pragma once

#include <array>
#include <optional>

namespace wxl::controller {

inline std::optional<unsigned>
ResolveEffectiveActionSlot(unsigned logical, const std::array<unsigned, 12> &effective,
                           bool mainBarContextValid) noexcept {
    if (logical < 1 || logical > 120)
        return std::nullopt;
    if (logical > 12)
        return logical;
    if (!mainBarContextValid)
        return std::nullopt;
    const unsigned resolved = effective[logical - 1];
    return resolved >= 1 && resolved <= 120 ? std::optional<unsigned>{resolved} : std::nullopt;
}

} // namespace wxl::controller
