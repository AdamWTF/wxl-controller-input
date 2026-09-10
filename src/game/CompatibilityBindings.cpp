#include "game/CompatibilityBindings.hpp"

#include <array>

namespace wxl::controller {
namespace {
constexpr std::array<const char *, 12> actionKeys{"1", "2", "3", "4", "5", "6",
                                                   "7", "8", "9", "0", "-", "="};

std::optional<KeyBinding> ResolveActionSlot(unsigned slot) {
    if (slot >= 1 && slot <= 12)
        return KeyBinding{actionKeys[slot - 1], {}};
    if (slot >= 49 && slot <= 60)
        return KeyBinding{actionKeys[slot - 49], {"CTRL"}};
    if (slot >= 61 && slot <= 72)
        return KeyBinding{actionKeys[slot - 61], {"SHIFT"}};
    return std::nullopt;
}

std::optional<KeyBinding> ResolveWowBinding(const std::string &command) {
    if (command == "JUMP")
        return KeyBinding{"SPACE", {}};
    if (command == "MOVEFORWARD")
        return KeyBinding{"W", {}};
    if (command == "MOVEBACKWARD")
        return KeyBinding{"S", {}};
    if (command == "STRAFELEFT")
        return KeyBinding{"Q", {}};
    if (command == "STRAFERIGHT")
        return KeyBinding{"E", {}};
    if (command == "TARGETNEARESTENEMY")
        return KeyBinding{"TAB", {}};
    if (command == "TARGETPREVIOUSENEMY")
        return KeyBinding{"TAB", {"SHIFT"}};
    if (command == "TARGETNEARESTFRIEND")
        return KeyBinding{"TAB", {"CTRL"}};
    if (command == "TARGETPREVIOUSFRIEND")
        return KeyBinding{"TAB", {"CTRL", "SHIFT"}};
    if (command == "TOGGLEAUTORUN")
        return KeyBinding{"NUMLOCK", {}};
    if (command == "TOGGLEGAMEMENU")
        return KeyBinding{"ESCAPE", {}};
    if (command == "TOGGLEWORLDMAP")
        return KeyBinding{"M", {}};
    return std::nullopt;
}
} // namespace

std::optional<KeyBinding> ResolveCompatibilityBinding(const Binding &binding) noexcept {
    try {
        return std::visit(
            [](const auto &value) -> std::optional<KeyBinding> {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, ActionSlot>)
                    return ResolveActionSlot(value.slot);
                else if constexpr (std::is_same_v<T, WowBinding>)
                    return ResolveWowBinding(value.command);
                else if constexpr (std::is_same_v<T, KeyBinding>)
                    return IsValid(value) ? std::optional<KeyBinding>{value} : std::nullopt;
                else
                    return std::nullopt;
            },
            binding);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace wxl::controller
