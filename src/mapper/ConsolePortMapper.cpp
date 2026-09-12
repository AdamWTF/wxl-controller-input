#include "mapper/ConsolePortMapper.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace wxl::controller {
namespace {
struct ButtonKey {
    Button button;
    Key key;
};

constexpr std::array<ButtonKey, 18> kButtonKeys{{
    {Button::DPadUp, Key::F1}, {Button::DPadRight, Key::F2},
    {Button::DPadDown, Key::F3}, {Button::DPadLeft, Key::F4},
    {Button::View, Key::F5}, {Button::Menu, Key::F6},
    {Button::LeftShoulder, Key::F7}, {Button::RightShoulder, Key::F8},
    {Button::FaceNorth, Key::Numpad4}, {Button::FaceEast, Key::F10},
    {Button::FaceSouth, Key::F11}, {Button::FaceWest, Key::F12},
    {Button::Guide, Key::NumpadMultiply}, {Button::Misc1, Key::NumpadAdd},
    {Button::RightPaddle1, Key::Numpad0}, {Button::RightPaddle2, Key::Numpad1},
    {Button::LeftPaddle1, Key::Numpad2}, {Button::LeftPaddle2, Key::Numpad3},
}};
}

ConsolePortMapper::ConsolePortMapper(IWoWInputSink &sink, Config config) noexcept
    : sink_(sink), config_(config) {}

bool ConsolePortMapper::SetKey(Key key, bool down) noexcept {
    bool &current = state_.keys[Index(key)];
    if (current == down)
        return true;
    current = down;
    return sink_.SetKey(key, down);
}

bool ConsolePortMapper::SetMouseButton(MouseButton button, bool down) noexcept {
    bool &current = state_.mouseButtons[Index(button)];
    if (current == down)
        return true;
    current = down;
    return sink_.SetMouseButton(button, down);
}

bool ConsolePortMapper::SetCameraActive(bool active) noexcept {
    if (state_.cameraActive == active)
        return true;
    state_.cameraActive = active;
    return sink_.SetCameraActive(active);
}

float ConsolePortMapper::CurveAxis(float axis, float speed, float curve) noexcept {
    const float scaled = std::abs(axis) * speed * curve * 0.05F;
    return std::copysign(scaled * scaled + scaled, axis);
}

void ConsolePortMapper::ProcessPointer(float x, float y, float &outX, float &outY) const noexcept {
    const float magnitude = std::sqrt(x * x + y * y);
    if (magnitude < config_.cursorDeadzone || magnitude == 0.0F) {
        outX = outY = 0.0F;
        return;
    }
    const float adjusted = (magnitude - config_.cursorDeadzone) /
                           (127.0F - config_.cursorDeadzone);
    outX = CurveAxis(x / magnitude * adjusted, config_.cursorSpeed, config_.cursorCurve);
    outY = CurveAxis(y / magnitude * adjusted, config_.cursorSpeed, config_.cursorCurve);
}

bool ConsolePortMapper::Update(const Snapshot &snapshot, bool allowMouse) noexcept {
    bool ok = true;
    const float movementX = config_.swapSticks ? snapshot.rightX : snapshot.leftX;
    const float movementY = config_.swapSticks ? snapshot.rightY : snapshot.leftY;
    const float pointerX = config_.swapSticks ? snapshot.leftX : snapshot.rightX;
    const float pointerY = config_.swapSticks ? snapshot.leftY : snapshot.rightY;

    ok = SetKey(Key::LeftShift, snapshot.leftTrigger > config_.leftTriggerThreshold) && ok;
    ok = SetKey(Key::LeftControl, snapshot.rightTrigger > config_.rightTriggerThreshold) && ok;

    for (const auto &mapping : kButtonKeys)
        ok = SetKey(mapping.key, snapshot.buttons[Index(mapping.button)]) && ok;
    ok = SetMouseButton(MouseButton::Left,
                       allowMouse && snapshot.buttons[Index(Button::LeftStick)]) && ok;
    ok = SetMouseButton(MouseButton::Right,
                       allowMouse && snapshot.buttons[Index(Button::RightStick)]) && ok;

    const bool left = movementX < -config_.movementThreshold;
    const bool right = movementX > config_.movementThreshold;
    const bool up = movementY < -config_.movementThreshold;
    const bool down = movementY > config_.movementThreshold;
    ok = SetKey(Key::W, up) && ok;
    ok = SetKey(Key::A, left) && ok;
    ok = SetKey(Key::S, down) && ok;
    ok = SetKey(Key::D, right) && ok;

    const float absX = std::abs(movementX);
    const float absY = std::abs(movementY);
    const bool diagonal = (left || right) && (up || down);
    const bool sendH = !config_.simpleRadial && diagonal && absX > absY * 1.5F;
    const bool sendV = !config_.simpleRadial && diagonal && absY > absX * 1.5F;
    ok = SetKey(Key::H, sendH) && ok;
    ok = SetKey(Key::V, sendV) && ok;

    ProcessPointer(pointerX, pointerY, state_.pointerX, state_.pointerY);
    const bool cameraActive = allowMouse &&
                              (state_.pointerX != 0.0F || state_.pointerY != 0.0F);
    ok = SetCameraActive(cameraActive) && ok;
    if (cameraActive)
        ok = sink_.MoveCameraRelative(state_.pointerX, state_.pointerY) && ok;
    return ok;
}

void ConsolePortMapper::CancelMouse() noexcept {
    SetMouseButton(MouseButton::Left, false);
    SetMouseButton(MouseButton::Right, false);
    SetCameraActive(false);
    state_.pointerX = 0.0F;
    state_.pointerY = 0.0F;
}

void ConsolePortMapper::ReleaseAll() noexcept {
    CancelMouse();
    for (std::size_t i = 0; i < state_.keys.size(); ++i) {
        if (state_.keys[i])
            sink_.SetKey(static_cast<Key>(i), false);
    }
    state_ = {};
    sink_.ReleaseAll();
}

} // namespace wxl::controller
