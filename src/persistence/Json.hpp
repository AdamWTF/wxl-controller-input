#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace wxl::controller::json {

struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;
    Storage data{nullptr};

    Value() = default;
    template <class T> Value(T value) : data(std::move(value)) {
    }

    [[nodiscard]] const Object *AsObject() const noexcept;
    [[nodiscard]] const Array *AsArray() const noexcept;
    [[nodiscard]] const std::string *AsString() const noexcept;
    [[nodiscard]] const double *AsNumber() const noexcept;
};

std::optional<Value> Parse(std::string_view input) noexcept;
std::string Serialize(const Value &value);

} // namespace wxl::controller::json
