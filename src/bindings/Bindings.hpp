#pragma once

#include "controller/ControllerTypes.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace wxl::controller {

struct ActionSlot { unsigned slot{}; friend bool operator==(const ActionSlot&, const ActionSlot&) = default; };
struct WowBinding { std::string command; friend bool operator==(const WowBinding&, const WowBinding&) = default; };
struct KeyBinding { std::string key; std::vector<std::string> modifiers; friend bool operator==(const KeyBinding&, const KeyBinding&) = default; };
struct Unassigned { friend bool operator==(const Unassigned&, const Unassigned&) = default; };
using Binding = std::variant<ActionSlot, WowBinding, KeyBinding, Unassigned>;

struct BindingKey {
    Layer layer{Layer::Base};
    Button button{Button::FaceSouth};
    friend auto operator<=>(const BindingKey&, const BindingKey&) = default;
};

using BindingMap = std::map<BindingKey, Binding>;
BindingMap BuiltInBindings();
bool IsValid(const Binding& binding) noexcept;

class BindingSink {
public:
    virtual ~BindingSink() = default;
    virtual bool Press(const Binding& binding) noexcept = 0;
    virtual void Release(const Binding& binding) noexcept = 0;
};

class BindingController {
public:
    BindingController(BindingSink& sink, const BindingMap& bindings)
        : sink_(sink), bindings_(bindings) {}
    void Update(Button button, bool down, Layer layer) noexcept;
    void Cancel() noexcept;

private:
    BindingSink& sink_;
    const BindingMap& bindings_;
    std::array<std::optional<Binding>, static_cast<std::size_t>(Button::Count)> owned_{};
};

} // namespace wxl::controller

