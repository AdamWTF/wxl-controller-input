#pragma once

#include "bindings/Bindings.hpp"

#include <optional>
#include <string>

namespace wxl::controller {

enum class BindingSource {
    BuiltIn,
    Global,
    Character
};
struct ResolvedBinding {
    Binding binding;
    BindingSource source;
};

class ProfileResolver {
  public:
    ProfileResolver();
    BindingMap &Global() noexcept {
        return global_;
    }
    BindingMap &Character() noexcept {
        return character_;
    }
    void SetCharacterIdentity(std::optional<std::string> identity) {
        identity_ = std::move(identity);
    }
    [[nodiscard]] std::optional<ResolvedBinding> Resolve(BindingKey key) const;

  private:
    BindingMap builtIn_;
    BindingMap global_;
    BindingMap character_;
    std::optional<std::string> identity_;
};

} // namespace wxl::controller
