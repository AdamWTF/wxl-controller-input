#pragma once

#include <filesystem>

namespace wxl::controller {

struct Config {
    bool enabled{true};
    bool debugLogging{false};
    float movementThreshold{40.0F};
    float leftTriggerThreshold{80.0F};
    float rightTriggerThreshold{80.0F};
    float cursorDeadzone{20.0F};
    float cursorSpeed{16.0F};
    float cursorCurve{4.0F};
    bool simpleRadial{false};
    bool swapSticks{false};
};

Config LoadConfig(const std::filesystem::path &path) noexcept;

} // namespace wxl::controller
