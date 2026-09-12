#include "config/Config.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <string>

namespace wxl::controller {
namespace {
bool ParseBool(std::string value, bool &output) {
    if (value == "true" || value == "1") {
        output = true;
        return true;
    }
    if (value == "false" || value == "0") {
        output = false;
        return true;
    }
    return false;
}
bool ParseFloat(const std::string &value, float &output) {
    float parsed{};
    const char *end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
        return false;
    output = parsed;
    return true;
}
} // namespace

Config LoadConfig(const std::filesystem::path &path) noexcept {
    Config config;
    try {
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;
            const auto equals = line.find('=');
            if (equals == std::string::npos)
                continue;
            const std::string key = line.substr(0, equals);
            const std::string value = line.substr(equals + 1);
            if (key == "Enabled")
                ParseBool(value, config.enabled);
            else if (key == "DebugLogging")
                ParseBool(value, config.debugLogging);
            else if (key == "MovementThreshold")
                ParseFloat(value, config.movementThreshold);
            else if (key == "LeftTriggerThreshold")
                ParseFloat(value, config.leftTriggerThreshold);
            else if (key == "RightTriggerThreshold")
                ParseFloat(value, config.rightTriggerThreshold);
            else if (key == "CursorDeadzone")
                ParseFloat(value, config.cursorDeadzone);
            else if (key == "CursorSpeed")
                ParseFloat(value, config.cursorSpeed);
            else if (key == "CursorCurve")
                ParseFloat(value, config.cursorCurve);
            else if (key == "SimpleRadial")
                ParseBool(value, config.simpleRadial);
            else if (key == "SwapSticks")
                ParseBool(value, config.swapSticks);
        }
    } catch (...) {
        return Config{};
    }
    config.movementThreshold = std::clamp(config.movementThreshold, 0.0F, 127.0F);
    config.leftTriggerThreshold = std::clamp(config.leftTriggerThreshold, 0.0F, 250.0F);
    config.rightTriggerThreshold = std::clamp(config.rightTriggerThreshold, 0.0F, 250.0F);
    config.cursorDeadzone = std::clamp(config.cursorDeadzone, 0.0F, 126.0F);
    config.cursorSpeed = std::clamp(config.cursorSpeed, 0.0F, 64.0F);
    config.cursorCurve = std::clamp(config.cursorCurve, 0.0F, 16.0F);
    return config;
}

} // namespace wxl::controller
