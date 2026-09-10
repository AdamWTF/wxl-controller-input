#pragma once

#include "bindings/Bindings.hpp"
#include "movement/MovementController.hpp"

#include <Windows.h>

#include <array>
#include <cstdint>

namespace wxl::controller {

class WindowsInputAdapter final {
  public:
    WindowsInputAdapter() noexcept;

    bool SetMovement(Movement movement, bool down) noexcept;
    bool Press(const Binding &binding) noexcept;
    void Release(const Binding &binding) noexcept;

    bool BeginCamera() noexcept;
    bool MoveCamera(float horizontal, float vertical, float deltaSeconds) noexcept;
    void EndCamera() noexcept;

    void HandleWindowMessage(std::uint32_t message, std::uintptr_t wparam,
                             std::uintptr_t lparam) noexcept;

  private:
    bool RefreshTarget() noexcept;
    bool AcquireKey(unsigned virtualKey) noexcept;
    void ReleaseKey(unsigned virtualKey) noexcept;
    bool PressChord(const KeyBinding &binding) noexcept;
    void ReleaseChord(const KeyBinding &binding) noexcept;
    bool SendKey(unsigned virtualKey, bool down) noexcept;
    bool SendRightButton(bool down) noexcept;

    HWND target_{};
    std::array<unsigned short, 256> keyReferences_{};
    std::array<bool, 256> physicalKeys_{};
    bool cameraRequested_{};
    bool syntheticRightButton_{};
    bool physicalRightButton_{};
    bool savedCursor_{};
    long savedCursorX_{};
    long savedCursorY_{};
};

} // namespace wxl::controller
