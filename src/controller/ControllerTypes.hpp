#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace wxl::controller {

enum class Button : std::uint8_t {
    FaceSouth, FaceEast, FaceWest, FaceNorth,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    LeftShoulder, RightShoulder, LeftStick, RightStick, View, Menu,
    Count
};

enum class Layer : std::uint8_t { Base, LT, RT, LTRT };

struct Snapshot {
    float leftX{};
    float leftY{};
    float rightX{};
    float rightY{};
    float leftTrigger{};
    float rightTrigger{};
    std::array<bool, static_cast<std::size_t>(Button::Count)> buttons{};
};

struct DeviceInfo {
    std::uint32_t instanceId{};
    int playerIndex{-1};
    std::string stableId;
    std::string name;
    std::string family;
};

inline constexpr std::size_t Index(Button button) {
    return static_cast<std::size_t>(button);
}

} // namespace wxl::controller

