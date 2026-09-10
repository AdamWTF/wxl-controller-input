#pragma once

#include <filesystem>

namespace wxl::controller {

struct Config {
    bool enabled{true};
    bool debugLogging{false};
    float movementDeadzone{0.18F};
    float cameraDeadzone{0.15F};
    float cameraHorizontalSensitivity{1.0F};
    float cameraVerticalSensitivity{1.0F};
    bool invertCameraY{false};
    float triggerActivateThreshold{0.50F};
    float triggerReleaseThreshold{0.40F};
    bool enableAnalogWalk{false};
    float walkRunThreshold{0.50F};
};

Config LoadConfig(const std::filesystem::path &path) noexcept;
bool SaveConfigAtomic(const std::filesystem::path &path, const Config &config) noexcept;

} // namespace wxl::controller
