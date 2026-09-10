#include "config/Config.hpp"
#include "persistence/AtomicFile.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
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
std::string FormatFloat(float value) {
    char buffer[64]{};
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return result.ec == std::errc{} ? std::string(buffer, result.ptr) : "0";
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
            else if (key == "MovementDeadzone")
                ParseFloat(value, config.movementDeadzone);
            else if (key == "CameraDeadzone")
                ParseFloat(value, config.cameraDeadzone);
            else if (key == "CameraHorizontalSensitivity")
                ParseFloat(value, config.cameraHorizontalSensitivity);
            else if (key == "CameraVerticalSensitivity")
                ParseFloat(value, config.cameraVerticalSensitivity);
            else if (key == "InvertCameraY")
                ParseBool(value, config.invertCameraY);
            else if (key == "TriggerActivateThreshold")
                ParseFloat(value, config.triggerActivateThreshold);
            else if (key == "TriggerReleaseThreshold")
                ParseFloat(value, config.triggerReleaseThreshold);
            else if (key == "EnableAnalogWalk")
                ParseBool(value, config.enableAnalogWalk);
            else if (key == "WalkRunThreshold")
                ParseFloat(value, config.walkRunThreshold);
        }
    } catch (...) {
        return Config{};
    }
    config.movementDeadzone = std::clamp(config.movementDeadzone, 0.0F, 0.95F);
    config.cameraDeadzone = std::clamp(config.cameraDeadzone, 0.0F, 0.95F);
    config.cameraHorizontalSensitivity =
        std::clamp(config.cameraHorizontalSensitivity, 0.05F, 10.0F);
    config.cameraVerticalSensitivity = std::clamp(config.cameraVerticalSensitivity, 0.05F, 10.0F);
    config.triggerActivateThreshold = std::clamp(config.triggerActivateThreshold, 0.01F, 1.0F);
    config.triggerReleaseThreshold =
        std::clamp(config.triggerReleaseThreshold, 0.0F, config.triggerActivateThreshold);
    config.walkRunThreshold = std::clamp(config.walkRunThreshold, config.movementDeadzone, 1.0F);
    return config;
}

bool SaveConfigAtomic(const std::filesystem::path &path, const Config &config) noexcept {
    try {
        std::ostringstream output;
        output << std::boolalpha;
        output << "Enabled=" << config.enabled << '\n';
        output << "DebugLogging=" << config.debugLogging << "\n\n";
        output << "MovementDeadzone=" << FormatFloat(config.movementDeadzone) << '\n';
        output << "CameraDeadzone=" << FormatFloat(config.cameraDeadzone) << '\n';
        output << "CameraHorizontalSensitivity=" << FormatFloat(config.cameraHorizontalSensitivity)
               << '\n';
        output << "CameraVerticalSensitivity=" << FormatFloat(config.cameraVerticalSensitivity)
               << '\n';
        output << "InvertCameraY=" << config.invertCameraY << "\n\n";
        output << "TriggerActivateThreshold=" << FormatFloat(config.triggerActivateThreshold)
               << '\n';
        output << "TriggerReleaseThreshold=" << FormatFloat(config.triggerReleaseThreshold)
               << "\n\n";
        output << "EnableAnalogWalk=" << config.enableAnalogWalk << '\n';
        output << "WalkRunThreshold=" << FormatFloat(config.walkRunThreshold) << '\n';
        return WriteTextFileAtomically(path, output.str());
    } catch (...) {
        return false;
    }
}

} // namespace wxl::controller
