#include "persistence/BindingStore.hpp"

#include "persistence/AtomicFile.hpp"
#include "persistence/Json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace wxl::controller {
namespace {
using json::Array;
using json::Object;
using json::Value;

const Value *Member(const Object &object, const std::string &name) {
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &found->second;
}

std::optional<Layer> ParseLayer(std::string_view name) {
    if (name == "Base")
        return Layer::Base;
    if (name == "LT")
        return Layer::LT;
    if (name == "RT")
        return Layer::RT;
    if (name == "LT+RT")
        return Layer::LTRT;
    return std::nullopt;
}

std::optional<Button> ParseButton(std::string_view name) {
    constexpr std::array<std::pair<std::string_view, Button>, 14> names{{
        {"FaceSouth", Button::FaceSouth},
        {"FaceEast", Button::FaceEast},
        {"FaceWest", Button::FaceWest},
        {"FaceNorth", Button::FaceNorth},
        {"DPadUp", Button::DPadUp},
        {"DPadRight", Button::DPadRight},
        {"DPadDown", Button::DPadDown},
        {"DPadLeft", Button::DPadLeft},
        {"LeftShoulder", Button::LeftShoulder},
        {"RightShoulder", Button::RightShoulder},
        {"LeftStick", Button::LeftStick},
        {"RightStick", Button::RightStick},
        {"View", Button::View},
        {"Menu", Button::Menu},
    }};
    for (const auto &[text, button] : names)
        if (name == text)
            return button;
    return std::nullopt;
}

std::optional<BindingKey> ParseKey(const std::string &text) {
    const auto dot = text.find('.');
    if (dot == std::string::npos) {
        const auto button = ParseButton(text);
        if (!button || *button == Button::Menu)
            return std::nullopt;
        return BindingKey{Layer::Base, *button};
    }
    const auto layer = ParseLayer(std::string_view(text).substr(0, dot));
    const auto button = ParseButton(std::string_view(text).substr(dot + 1));
    if (!layer || !button || Index(*button) > Index(Button::DPadLeft))
        return std::nullopt;
    return BindingKey{*layer, *button};
}

const char *LayerName(Layer layer) {
    switch (layer) {
    case Layer::Base:
        return "Base";
    case Layer::LT:
        return "LT";
    case Layer::RT:
        return "RT";
    case Layer::LTRT:
        return "LT+RT";
    }
    return "Base";
}

const char *ButtonName(Button button) {
    constexpr std::array<const char *, 14> names{
        "FaceSouth", "FaceEast",   "FaceWest", "FaceNorth",    "DPadUp",
        "DPadRight", "DPadDown",   "DPadLeft", "LeftShoulder", "RightShoulder",
        "LeftStick", "RightStick", "View",     "Menu"};
    return names[Index(button)];
}

std::string KeyName(BindingKey key) {
    if (Index(key.button) > Index(Button::DPadLeft))
        return ButtonName(key.button);
    return std::string(LayerName(key.layer)) + '.' + ButtonName(key.button);
}

std::optional<Binding> ParseBinding(const Value &value) {
    const auto *object = value.AsObject();
    if (!object)
        return std::nullopt;
    const Value *typeValue = Member(*object, "type");
    const auto *type = typeValue ? typeValue->AsString() : nullptr;
    if (!type)
        return std::nullopt;
    Binding binding;
    if (*type == "Unassigned")
        binding = Unassigned{};
    else if (*type == "ActionSlot") {
        const Value *slotValue = Member(*object, "slot");
        const auto *slot = slotValue ? slotValue->AsNumber() : nullptr;
        if (!slot || *slot != std::floor(*slot) || *slot < 1 || *slot > 120)
            return std::nullopt;
        binding = ActionSlot{static_cast<unsigned>(*slot)};
    } else if (*type == "WoWBinding") {
        const Value *commandValue = Member(*object, "command");
        const auto *command = commandValue ? commandValue->AsString() : nullptr;
        if (!command)
            return std::nullopt;
        binding = WowBinding{*command};
    } else if (*type == "KeyBinding") {
        const Value *keyValue = Member(*object, "key");
        const auto *key = keyValue ? keyValue->AsString() : nullptr;
        if (!key)
            return std::nullopt;
        KeyBinding parsed{*key, {}};
        if (const Value *modifiersValue = Member(*object, "modifiers")) {
            const auto *modifiers = modifiersValue->AsArray();
            if (!modifiers)
                return std::nullopt;
            for (const auto &modifier : *modifiers) {
                const auto *text = modifier.AsString();
                if (!text)
                    return std::nullopt;
                parsed.modifiers.push_back(*text);
            }
        }
        binding = std::move(parsed);
    } else
        return std::nullopt;
    return IsValid(binding) ? std::optional<Binding>(std::move(binding)) : std::nullopt;
}

BindingMap ParseBindings(const Value *value) {
    BindingMap bindings;
    const auto *object = value ? value->AsObject() : nullptr;
    if (!object)
        return bindings;
    for (const auto &[name, rawBinding] : *object) {
        const auto key = ParseKey(name);
        const auto binding = ParseBinding(rawBinding);
        if (key && binding)
            bindings[*key] = *binding;
    }
    return bindings;
}

Value BindingValue(const Binding &binding) {
    return std::visit(
        [](const auto &value) -> Value {
            using T = std::decay_t<decltype(value)>;
            Object object;
            if constexpr (std::is_same_v<T, ActionSlot>) {
                object["slot"] = static_cast<double>(value.slot);
                object["type"] = std::string("ActionSlot");
            } else if constexpr (std::is_same_v<T, WowBinding>) {
                object["command"] = value.command;
                object["type"] = std::string("WoWBinding");
            } else if constexpr (std::is_same_v<T, KeyBinding>) {
                Array modifiers;
                for (const auto &m : value.modifiers)
                    modifiers.emplace_back(m);
                object["key"] = value.key;
                object["modifiers"] = std::move(modifiers);
                object["type"] = std::string("KeyBinding");
            } else
                object["type"] = std::string("Unassigned");
            return object;
        },
        binding);
}

Value BindingsValue(const BindingMap &bindings) {
    Object object;
    for (const auto &[key, binding] : bindings)
        object[KeyName(key)] = BindingValue(binding);
    return object;
}
} // namespace

BindingLoadResult BindingStore::Load(const std::filesystem::path &path) noexcept {
    try {
        if (!std::filesystem::exists(path))
            return BindingLoadResult::Missing;
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return BindingLoadResult::IoError;
        std::ostringstream contents;
        contents << input.rdbuf();
        if (!input.good() && !input.eof())
            return BindingLoadResult::IoError;
        const auto rootValue = json::Parse(contents.str());
        const auto *root = rootValue ? rootValue->AsObject() : nullptr;
        if (!root)
            return BindingLoadResult::Invalid;
        const Value *schemaValue = Member(*root, "schemaVersion");
        const auto *schema = schemaValue ? schemaValue->AsNumber() : nullptr;
        if (!schema || *schema != std::floor(*schema))
            return BindingLoadResult::Invalid;
        if (static_cast<unsigned>(*schema) != SchemaVersion)
            return BindingLoadResult::UnsupportedVersion;

        BindingMap nextGlobal;
        std::map<std::string, BindingMap> nextCharacters;
        if (const Value *globalValue = Member(*root, "global")) {
            const auto *global = globalValue->AsObject();
            if (!global)
                return BindingLoadResult::Invalid;
            nextGlobal = ParseBindings(Member(*global, "bindings"));
        }
        if (const Value *charactersValue = Member(*root, "characters")) {
            const auto *characters = charactersValue->AsObject();
            if (!characters)
                return BindingLoadResult::Invalid;
            for (const auto &[identity, profileValue] : *characters) {
                const auto *profile = profileValue.AsObject();
                if (!profile || identity.empty())
                    continue;
                nextCharacters[identity] = ParseBindings(Member(*profile, "bindings"));
            }
        }
        global_ = std::move(nextGlobal);
        characters_ = std::move(nextCharacters);
        return BindingLoadResult::Loaded;
    } catch (...) {
        return BindingLoadResult::Invalid;
    }
}

bool BindingStore::SaveAtomic(const std::filesystem::path &path) const noexcept {
    try {
        Object root;
        root["schemaVersion"] = static_cast<double>(SchemaVersion);
        root["global"] = Object{{"bindings", BindingsValue(global_)}};
        Object characters;
        for (const auto &[identity, bindings] : characters_)
            characters[identity] = Object{{"bindings", BindingsValue(bindings)}};
        root["characters"] = std::move(characters);
        return WriteTextFileAtomically(path, json::Serialize(root));
    } catch (...) {
        return false;
    }
}

BindingMap BindingStore::Effective(const std::optional<std::string> &identity) const {
    BindingMap result = BuiltInBindings();
    for (const auto &[key, binding] : global_)
        result[key] = binding;
    if (identity) {
        if (const auto found = characters_.find(*identity); found != characters_.end())
            for (const auto &[key, binding] : found->second)
                result[key] = binding;
    }
    return result;
}

void BindingStore::SetGlobal(BindingKey key, Binding binding) {
    if (key.button != Button::Menu && IsValid(binding))
        global_[key] = std::move(binding);
}
void BindingStore::SetCharacter(const std::string &identity, BindingKey key, Binding binding) {
    if (!identity.empty() && key.button != Button::Menu && IsValid(binding))
        characters_[identity][key] = std::move(binding);
}
void BindingStore::ResetGlobal(BindingKey key) noexcept {
    global_.erase(key);
}
void BindingStore::ResetCharacter(const std::string &identity, BindingKey key) noexcept {
    const auto profile = characters_.find(identity);
    if (profile == characters_.end())
        return;
    profile->second.erase(key);
    if (profile->second.empty())
        characters_.erase(profile);
}
void BindingStore::ResetLayer(std::optional<std::string> identity, Layer layer) noexcept {
    BindingMap *bindings = &global_;
    if (identity) {
        const auto profile = characters_.find(*identity);
        if (profile == characters_.end())
            return;
        bindings = &profile->second;
    }
    for (auto it = bindings->begin(); it != bindings->end();) {
        if (it->first.layer == layer && Index(it->first.button) <= Index(Button::DPadLeft))
            it = bindings->erase(it);
        else
            ++it;
    }
    if (identity && bindings->empty())
        characters_.erase(*identity);
}

} // namespace wxl::controller
