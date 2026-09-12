#pragma once

#include <cstddef>
#include <cstdint>

namespace wxl::controller {

enum class Key : std::uint8_t {
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift, LeftControl,
    W, A, S, D, H, V,
    NumpadMultiply, NumpadAdd, Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Count
};

enum class MouseButton : std::uint8_t { Left, Right, Count };

inline constexpr std::size_t Index(Key key) noexcept {
    return static_cast<std::size_t>(key);
}
inline constexpr std::size_t Index(MouseButton button) noexcept {
    return static_cast<std::size_t>(button);
}

class IWoWInputSink {
  public:
    virtual ~IWoWInputSink() = default;
    virtual bool SetKey(Key key, bool down) noexcept = 0;
    virtual bool SetMouseButton(MouseButton button, bool down) noexcept = 0;
    virtual bool SetCameraActive(bool active) noexcept = 0;
    virtual bool MoveCameraRelative(float dx, float dy) noexcept = 0;
    virtual bool Reconcile(bool allowNewInput) noexcept = 0;
    virtual void ReleaseAll() noexcept = 0;
};

} // namespace wxl::controller
