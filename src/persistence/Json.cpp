#include "persistence/Json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace wxl::controller::json {

const Object *Value::AsObject() const noexcept {
    return std::get_if<Object>(&data);
}
const Array *Value::AsArray() const noexcept {
    return std::get_if<Array>(&data);
}
const std::string *Value::AsString() const noexcept {
    return std::get_if<std::string>(&data);
}
const double *Value::AsNumber() const noexcept {
    return std::get_if<double>(&data);
}

namespace {
class Parser {
  public:
    explicit Parser(std::string_view input) : input_(input) {
    }

    std::optional<Value> Run() {
        Skip();
        auto value = ParseValue();
        Skip();
        if (!value || position_ != input_.size())
            return std::nullopt;
        return value;
    }

  private:
    void Skip() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\t' ||
                input_[position_] == '\r' || input_[position_] == '\n'))
            ++position_;
    }

    bool Consume(char expected) {
        Skip();
        if (position_ >= input_.size() || input_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    std::optional<Value> ParseValue() {
        Skip();
        if (position_ >= input_.size())
            return std::nullopt;
        switch (input_[position_]) {
        case '{':
            return ParseObject();
        case '[':
            return ParseArray();
        case '"': {
            auto text = ParseString();
            return text ? std::optional<Value>(*text) : std::nullopt;
        }
        case 't':
            return Literal("true", Value{true});
        case 'f':
            return Literal("false", Value{false});
        case 'n':
            return Literal("null", Value{});
        default:
            return ParseNumber();
        }
    }

    std::optional<Value> Literal(std::string_view text, Value value) {
        if (input_.substr(position_, text.size()) != text)
            return std::nullopt;
        position_ += text.size();
        return value;
    }

    static bool Hex(char c, unsigned &value) {
        if (c >= '0' && c <= '9')
            value = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f')
            value = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            value = static_cast<unsigned>(c - 'A' + 10);
        else
            return false;
        return true;
    }

    static void AppendUtf8(std::string &output, unsigned codepoint) {
        if (codepoint <= 0x7F)
            output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            if (codepoint > 0xFFFF) {
                output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                return;
            }
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::optional<std::string> ParseString() {
        if (!Consume('"'))
            return std::nullopt;
        std::string output;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"')
                return output;
            if (static_cast<unsigned char>(c) < 0x20)
                return std::nullopt;
            if (c != '\\') {
                output.push_back(c);
                continue;
            }
            if (position_ >= input_.size())
                return std::nullopt;
            const char escape = input_[position_++];
            switch (escape) {
            case '"':
                output.push_back('"');
                break;
            case '\\':
                output.push_back('\\');
                break;
            case '/':
                output.push_back('/');
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u': {
                if (position_ + 4 > input_.size())
                    return std::nullopt;
                unsigned codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    unsigned digit{};
                    if (!Hex(input_[position_++], digit))
                        return std::nullopt;
                    codepoint = codepoint * 16 + digit;
                }
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if (position_ + 6 > input_.size() || input_[position_] != '\\' ||
                        input_[position_ + 1] != 'u')
                        return std::nullopt;
                    position_ += 2;
                    unsigned low = 0;
                    for (int i = 0; i < 4; ++i) {
                        unsigned digit{};
                        if (!Hex(input_[position_++], digit))
                            return std::nullopt;
                        low = low * 16 + digit;
                    }
                    if (low < 0xDC00 || low > 0xDFFF)
                        return std::nullopt;
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                    return std::nullopt;
                }
                AppendUtf8(output, codepoint);
                break;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<Value> ParseNumber() {
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-')
            ++position_;
        if (position_ >= input_.size())
            return std::nullopt;
        if (input_[position_] == '0')
            ++position_;
        else {
            if (input_[position_] < '1' || input_[position_] > '9')
                return std::nullopt;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const std::size_t fraction = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
            if (fraction == position_)
                return std::nullopt;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                ++position_;
            const std::size_t exponent = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9')
                ++position_;
            if (exponent == position_)
                return std::nullopt;
        }
        double value{};
        const auto result =
            std::from_chars(input_.data() + start, input_.data() + position_, value);
        if (result.ec != std::errc{} || !std::isfinite(value))
            return std::nullopt;
        return Value{value};
    }

    std::optional<Value> ParseObject() {
        if (!Consume('{'))
            return std::nullopt;
        Object object;
        Skip();
        if (Consume('}'))
            return Value{std::move(object)};
        while (true) {
            auto key = ParseString();
            if (!key || !Consume(':'))
                return std::nullopt;
            auto value = ParseValue();
            if (!value || object.contains(*key))
                return std::nullopt;
            object.emplace(std::move(*key), std::move(*value));
            Skip();
            if (Consume('}'))
                return Value{std::move(object)};
            if (!Consume(','))
                return std::nullopt;
        }
    }

    std::optional<Value> ParseArray() {
        if (!Consume('['))
            return std::nullopt;
        Array array;
        Skip();
        if (Consume(']'))
            return Value{std::move(array)};
        while (true) {
            auto value = ParseValue();
            if (!value)
                return std::nullopt;
            array.push_back(std::move(*value));
            Skip();
            if (Consume(']'))
                return Value{std::move(array)};
            if (!Consume(','))
                return std::nullopt;
        }
    }

    std::string_view input_;
    std::size_t position_{};
};

void Escape(std::ostringstream &output, const std::string &text) {
    output << '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (c < 0x20) {
                char escaped[7]{};
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                output << escaped;
            } else
                output << static_cast<char>(c);
        }
    }
    output << '"';
}

void Write(std::ostringstream &output, const Value &value) {
    std::visit(
        [&](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>)
                output << "null";
            else if constexpr (std::is_same_v<T, bool>)
                output << (item ? "true" : "false");
            else if constexpr (std::is_same_v<T, double>)
                output << item;
            else if constexpr (std::is_same_v<T, std::string>)
                Escape(output, item);
            else if constexpr (std::is_same_v<T, Object>) {
                output << '{';
                bool first = true;
                for (const auto &[key, child] : item) {
                    if (!first)
                        output << ',';
                    first = false;
                    Escape(output, key);
                    output << ':';
                    Write(output, child);
                }
                output << '}';
            } else {
                output << '[';
                bool first = true;
                for (const auto &child : item) {
                    if (!first)
                        output << ',';
                    first = false;
                    Write(output, child);
                }
                output << ']';
            }
        },
        value.data);
}
} // namespace

std::optional<Value> Parse(std::string_view input) noexcept {
    try {
        return Parser(input).Run();
    } catch (...) {
        return std::nullopt;
    }
}

std::string Serialize(const Value &value) {
    std::ostringstream output;
    Write(output, value);
    output << '\n';
    return output.str();
}

} // namespace wxl::controller::json
