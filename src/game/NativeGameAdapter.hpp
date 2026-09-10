#pragma once

#include "bindings/Bindings.hpp"
#include "input/KeyOwnership.hpp"
#include "input/MouseButtonOwnership.hpp"
#include "movement/MovementController.hpp"

#include <Windows.h>

#include <array>
#include <cstdint>

namespace wxl::controller {

class NativeGameAdapter final {
  public:
    NativeGameAdapter() noexcept;

    void SetTime(std::uint32_t timeMs) noexcept;
    bool SetMovement(Movement movement, bool down) noexcept;
    bool Press(const Binding &binding) noexcept;
    void Release(const Binding &binding) noexcept;

    bool BeginCamera() noexcept;
    bool MoveCamera(float horizontal, float vertical, float deltaSeconds) noexcept;
    void EndCamera() noexcept;

  private:
    bool RefreshWindow() noexcept;
    bool SetNativeControl(std::uint32_t control, bool down) noexcept;
    bool AcquireKey(unsigned virtualKey) noexcept;
    void ReleaseKey(unsigned virtualKey) noexcept;
    bool PressKeyBinding(const KeyBinding &binding) noexcept;
    void ReleaseKeyBinding(const KeyBinding &binding) noexcept;
    bool SetRightButton(bool down) noexcept;

    HWND window_{};
    KeyOwnership keys_;
    std::uint32_t timeMs_{};
    MouseButtonOwnership rightButton_;
    bool savedCursor_{};
    POINT savedCursorPosition_{};
    float cameraRemainderX_{};
    float cameraRemainderY_{};
};

} // namespace wxl::controller
