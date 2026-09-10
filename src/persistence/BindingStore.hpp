#pragma once

#include "bindings/Bindings.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace wxl::controller {

enum class BindingLoadResult {
    Missing,
    Loaded,
    Invalid,
    UnsupportedVersion,
    IoError
};

class BindingStore {
  public:
    static constexpr unsigned SchemaVersion = 1;

    BindingLoadResult Load(const std::filesystem::path &path) noexcept;
    bool SaveAtomic(const std::filesystem::path &path) const noexcept;

    [[nodiscard]] BindingMap Effective(const std::optional<std::string> &identity) const;
    [[nodiscard]] const BindingMap &Global() const noexcept {
        return global_;
    }
    [[nodiscard]] const std::map<std::string, BindingMap> &Characters() const noexcept {
        return characters_;
    }

    void SetGlobal(BindingKey key, Binding binding);
    void SetCharacter(const std::string &identity, BindingKey key, Binding binding);
    void ResetGlobal(BindingKey key) noexcept;
    void ResetCharacter(const std::string &identity, BindingKey key) noexcept;
    void ResetLayer(std::optional<std::string> identity, Layer layer) noexcept;
    void ResetGlobalAll() noexcept {
        global_.clear();
    }
    void ResetCharacterAll(const std::string &identity) noexcept {
        characters_.erase(identity);
    }

  private:
    BindingMap global_;
    std::map<std::string, BindingMap> characters_;
};

} // namespace wxl::controller
