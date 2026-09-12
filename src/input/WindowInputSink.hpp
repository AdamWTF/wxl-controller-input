#pragma once

#include "input/WoWInputSink.hpp"
#include "input/DigitalOwnership.hpp"
#include "input/RelativeAccumulator.hpp"

#include <Windows.h>

#include <array>

namespace wxl::controller {

class WindowInputSink final : public IWoWInputSink {
  public:
    WindowInputSink() noexcept;
    ~WindowInputSink() override;
    bool SetKey(Key key, bool down) noexcept override;
    bool SetMouseButton(MouseButton button, bool down) noexcept override;
    bool SetCameraActive(bool active) noexcept override;
    bool MoveCameraRelative(float dx, float dy) noexcept override;
    bool Reconcile(bool allowNewInput) noexcept override;
    void ReleaseAll() noexcept override;
    void ObserveWindowMessage(std::uint32_t message, std::uintptr_t wparam,
                              std::uintptr_t lparam, std::uintptr_t extraInfo) noexcept;
    [[nodiscard]] bool Ready() noexcept;
    [[nodiscard]] static const char *Name() noexcept { return "focused WoW camera/window input"; }

  private:
    bool RefreshWindow() noexcept;
    bool Foreground() noexcept;
    bool ReconcileKey(unsigned virtualKey, bool allowNewInput) noexcept;
    bool ReconcileMouse(MouseButton button, bool allowNewInput) noexcept;
    bool SendKey(unsigned virtualKey, bool down) noexcept;
    bool SendMouseButton(MouseButton button, bool down) noexcept;
    bool DesiredMouse(MouseButton button) const noexcept;
    void RestoreCameraCursor() noexcept;
    WPARAM MouseFlags() const noexcept;
    bool InitializeVirtualPointer() noexcept;
    static unsigned VirtualKey(Key key) noexcept;

    HWND window_{};
    std::array<DigitalOwnership, 256> keys_{};
    std::array<DigitalOwnership, Index(MouseButton::Count)> mouse_{};
    std::array<bool, Index(MouseButton::Count)> buttonDesired_{};
    std::array<bool, Index(MouseButton::Count)> physicalMouse_{};
    bool cameraActive_{};
    bool savedCursor_{};
    POINT savedCursorPosition_{};
    bool virtualPointerReady_{};
    POINT virtualPointer_{};
    RelativeAccumulator accumulator_;
};

} // namespace wxl::controller
