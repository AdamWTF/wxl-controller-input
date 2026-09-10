#include "bindings/Bindings.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace wxl::controller {

BindingMap BuiltInBindings() {
    BindingMap result;
    constexpr std::array<Button, 8> buttons{Button::FaceSouth, Button::FaceEast, Button::FaceWest,
                                            Button::FaceNorth, Button::DPadUp,   Button::DPadRight,
                                            Button::DPadDown,  Button::DPadLeft};
    constexpr std::array<unsigned, 8> base{1, 2, 3, 4, 5, 6, 7, 8};
    constexpr std::array<unsigned, 8> lt{49, 50, 51, 52, 53, 54, 55, 56};
    constexpr std::array<unsigned, 8> rt{61, 62, 63, 64, 65, 66, 67, 68};
    constexpr std::array<unsigned, 8> both{9, 10, 11, 12, 57, 58, 59, 60};
    const auto addLayer = [&](Layer layer, const auto &slots) {
        for (std::size_t i = 0; i < buttons.size(); ++i)
            result[{layer, buttons[i]}] = ActionSlot{slots[i]};
    };
    addLayer(Layer::Base, base);
    addLayer(Layer::LT, lt);
    addLayer(Layer::RT, rt);
    addLayer(Layer::LTRT, both);
    result[{Layer::Base, Button::LeftShoulder}] = WowBinding{"TARGETPREVIOUSENEMY"};
    result[{Layer::Base, Button::RightShoulder}] = WowBinding{"TARGETNEARESTENEMY"};
    result[{Layer::Base, Button::LeftStick}] = WowBinding{"TARGETPREVIOUSFRIEND"};
    result[{Layer::Base, Button::RightStick}] = WowBinding{"TARGETNEARESTFRIEND"};
    result[{Layer::Base, Button::View}] = KeyBinding{"M", {}};
    result[{Layer::Base, Button::Menu}] = KeyBinding{"ESCAPE", {}};
    return result;
}

bool IsValid(const Binding &binding) noexcept {
    return std::visit(
        [](const auto &value) noexcept {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ActionSlot>) {
                return value.slot >= 1 && value.slot <= 120;
            } else if constexpr (std::is_same_v<T, WowBinding>) {
                static const std::set<std::string> supported{"JUMP",
                                                             "MOVEFORWARD",
                                                             "MOVEBACKWARD",
                                                             "STRAFELEFT",
                                                             "STRAFERIGHT",
                                                             "TARGETNEARESTENEMY",
                                                             "TARGETPREVIOUSENEMY",
                                                             "TARGETNEARESTFRIEND",
                                                             "TARGETPREVIOUSFRIEND",
                                                             "TOGGLEAUTORUN",
                                                             "TOGGLEGAMEMENU",
                                                             "TOGGLEWORLDMAP"};
                return supported.contains(value.command);
            } else if constexpr (std::is_same_v<T, KeyBinding>) {
                static const std::set<std::string> modifiers{"ALT", "CTRL", "SHIFT"};
                static const std::set<std::string> namedKeys{
                    "BACKSPACE", "DELETE", "DOWN", "END",      "ENTER",  "ESCAPE",
                    "HOME",      "INSERT", "LEFT", "PAGEDOWN", "PAGEUP", "RIGHT",
                    "SPACE",     "TAB",    "UP",   "F1",       "F2",     "F3",
                    "F4",        "F5",     "F6",   "F7",       "F8",     "F9",
                    "F10",       "F11",    "F12",  "NUMLOCK",  "-",      "="};
                const bool singleKey = value.key.size() == 1 &&
                                       std::isalnum(static_cast<unsigned char>(value.key.front()));
                return (singleKey || namedKeys.contains(value.key)) &&
                       value.modifiers.size() <= 3 &&
                       std::all_of(value.modifiers.begin(), value.modifiers.end(),
                                   [&](const std::string &m) { return modifiers.contains(m); });
            } else {
                return true;
            }
        },
        binding);
}

void BindingController::Update(Button button, bool down, Layer layer) noexcept {
    auto &owned = owned_[Index(button)];
    if (down) {
        if (owned)
            return;
        auto found = bindings_.find({layer, button});
        if (found == bindings_.end() && layer != Layer::Base)
            found = bindings_.find({Layer::Base, button});
        if (found == bindings_.end() || !IsValid(found->second))
            return;
        if (sink_.Press(found->second))
            owned = found->second;
    } else if (owned) {
        sink_.Release(*owned);
        owned.reset();
    }
}

void BindingController::Cancel() noexcept {
    for (auto &binding : owned_) {
        if (binding)
            sink_.Release(*binding);
        binding.reset();
    }
}

} // namespace wxl::controller
