#include "input/WindowInputSink.hpp"
#include "input/MouseSourceFilter.hpp"

#include <algorithm>
#include <cmath>

namespace wxl::controller {
namespace {
thread_local unsigned g_dispatchDepth{};

struct DispatchScope final {
    DispatchScope() noexcept { ++g_dispatchDepth; }
    ~DispatchScope() { --g_dispatchDepth; }
};

}

WindowInputSink::WindowInputSink() noexcept {
    RefreshWindow();
    physicalMouse_[Index(MouseButton::Left)] =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    physicalMouse_[Index(MouseButton::Right)] =
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}
WindowInputSink::~WindowInputSink() { ReleaseAll(); }

unsigned WindowInputSink::VirtualKey(Key key) noexcept {
    constexpr std::array<unsigned, Index(Key::Count)> keys{
        VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        VK_SHIFT, VK_CONTROL, 'W', 'A', 'S', 'D', 'H', 'V',
        VK_MULTIPLY, VK_ADD, VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4};
    const auto index = Index(key);
    return index < keys.size() ? keys[index] : 0;
}

bool WindowInputSink::RefreshWindow() noexcept {
    if (window_ && IsWindow(window_))
        return true;
    window_ = FindWindowW(L"GxWindowClassD3d", nullptr);
    virtualPointerReady_ = false;
    return window_ != nullptr;
}

bool WindowInputSink::Foreground() noexcept {
    return RefreshWindow() && GetForegroundWindow() == window_;
}

bool WindowInputSink::Ready() noexcept { return RefreshWindow(); }

bool WindowInputSink::SendKey(unsigned virtualKey, bool down) noexcept {
    if (!RefreshWindow() || virtualKey == 0)
        return false;
    const unsigned scan = MapVirtualKeyA(virtualKey, MAPVK_VK_TO_VSC);
    LPARAM details = 1L | (static_cast<LPARAM>(scan) << 16);
    if (!down)
        details |= (static_cast<LPARAM>(3u) << 30);
    SendMessageA(window_, down ? WM_KEYDOWN : WM_KEYUP, virtualKey, details);
    return true;
}

WPARAM WindowInputSink::MouseFlags() const noexcept {
    WPARAM flags{};
    if (DesiredMouse(MouseButton::Left) || physicalMouse_[Index(MouseButton::Left)])
        flags |= MK_LBUTTON;
    if (DesiredMouse(MouseButton::Right) || physicalMouse_[Index(MouseButton::Right)])
        flags |= MK_RBUTTON;
    return flags;
}

bool WindowInputSink::DesiredMouse(MouseButton button) const noexcept {
    return buttonDesired_[Index(button)] ||
           (button == MouseButton::Right && cameraActive_);
}

bool WindowInputSink::InitializeVirtualPointer() noexcept {
    if (virtualPointerReady_)
        return true;
    if (!RefreshWindow() || !GetCursorPos(&virtualPointer_))
        return false;
    if (!ScreenToClient(window_, &virtualPointer_))
        return false;
    virtualPointerReady_ = true;
    return true;
}

bool WindowInputSink::SendMouseButton(MouseButton button, bool down) noexcept {
    if (!InitializeVirtualPointer())
        return false;
    const UINT message = button == MouseButton::Left
                             ? (down ? WM_LBUTTONDOWN : WM_LBUTTONUP)
                             : (down ? WM_RBUTTONDOWN : WM_RBUTTONUP);
    DispatchScope dispatch;
    SendMessageA(window_, message, MouseFlags(),
                 MAKELPARAM(virtualPointer_.x, virtualPointer_.y));
    return true;
}

bool WindowInputSink::ReconcileKey(unsigned virtualKey, bool allowNewInput) noexcept {
    const bool physical = (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
    if (keys_[virtualKey].Desired() && (!allowNewInput || !Foreground()))
        return false;
    const OwnershipTransition transition = keys_[virtualKey].Reconcile(physical, allowNewInput);
    if (transition == OwnershipTransition::Blocked)
        return false;
    if (transition == OwnershipTransition::SendDown ||
        transition == OwnershipTransition::ResendDown) {
        if (SendKey(virtualKey, true))
            return true;
        keys_[virtualKey].Undo(transition);
        return false;
    }
    if (transition == OwnershipTransition::SendUp) {
        if (SendKey(virtualKey, false))
            return true;
        keys_[virtualKey].Undo(transition);
        return false;
    }
    return true;
}

bool WindowInputSink::ReconcileMouse(MouseButton button, bool allowNewInput) noexcept {
    const auto index = Index(button);
    const bool physical = physicalMouse_[index];
    mouse_[index].SetDesired(DesiredMouse(button));
    if (mouse_[index].Desired() && (!allowNewInput || !Foreground()))
        return false;
    const OwnershipTransition transition = mouse_[index].Reconcile(physical, allowNewInput);
    if (transition == OwnershipTransition::Blocked)
        return false;
    if (transition == OwnershipTransition::SendDown ||
        transition == OwnershipTransition::ResendDown) {
        if (SendMouseButton(button, true))
            return true;
        mouse_[index].Undo(transition);
        return false;
    }
    if (transition == OwnershipTransition::SendUp) {
        if (SendMouseButton(button, false))
            return true;
        mouse_[index].Undo(transition);
        return false;
    }
    return true;
}

bool WindowInputSink::SetKey(Key key, bool down) noexcept {
    const unsigned virtualKey = VirtualKey(key);
    if (!virtualKey)
        return false;
    keys_[virtualKey].SetDesired(down);
    return ReconcileKey(virtualKey, down);
}

bool WindowInputSink::SetMouseButton(MouseButton button, bool down) noexcept {
    buttonDesired_[Index(button)] = down;
    return ReconcileMouse(button, DesiredMouse(button));
}

bool WindowInputSink::SetCameraActive(bool active) noexcept {
    if (active == cameraActive_)
        return !active || Foreground();
    if (active) {
        if (!Foreground() || !InitializeVirtualPointer() ||
            !GetCursorPos(&savedCursorPosition_))
            return false;
        savedCursor_ = true;
        cameraActive_ = true;
        if (ReconcileMouse(MouseButton::Right, true))
            return true;
        cameraActive_ = false;
        savedCursor_ = false;
        return false;
    }

    cameraActive_ = false;
    const bool ok = ReconcileMouse(MouseButton::Right, DesiredMouse(MouseButton::Right));
    accumulator_.Reset();
    RestoreCameraCursor();
    return ok;
}

void WindowInputSink::RestoreCameraCursor() noexcept {
    if (!savedCursor_)
        return;
    const bool physicalRight = physicalMouse_[Index(MouseButton::Right)];
    if (!physicalRight && window_ && GetForegroundWindow() == window_)
        SetCursorPos(savedCursorPosition_.x, savedCursorPosition_.y);
    savedCursor_ = false;
}

bool WindowInputSink::MoveCameraRelative(float dx, float dy) noexcept {
    if (!cameraActive_ || !Foreground())
        return false;
    const RelativeStep move = accumulator_.Add(dx, dy);
    const int moveX = move.x;
    const int moveY = move.y;
    if (!moveX && !moveY)
        return true;
    POINT cursor{};
    RECT client{};
    if (!GetCursorPos(&cursor))
        return false;
    if (!GetClientRect(window_, &client) || client.right <= client.left ||
        client.bottom <= client.top)
        return false;
    POINT low{client.left, client.top};
    POINT high{client.right - 1, client.bottom - 1};
    if (!ClientToScreen(window_, &low) || !ClientToScreen(window_, &high))
        return false;
    cursor.x = std::clamp(cursor.x + moveX, low.x, high.x);
    cursor.y = std::clamp(cursor.y + moveY, low.y, high.y);
    if (!SetCursorPos(cursor.x, cursor.y) || !ScreenToClient(window_, &cursor))
        return false;
    virtualPointer_ = cursor;
    virtualPointerReady_ = true;
    DispatchScope dispatch;
    SendMessageA(window_, WM_MOUSEMOVE, MouseFlags(),
                 MAKELPARAM(virtualPointer_.x, virtualPointer_.y));
    return true;
}

bool WindowInputSink::Reconcile(bool allowNewInput) noexcept {
    bool ok = true;
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i].Desired() || keys_[i].Sent())
            ok = ReconcileKey(static_cast<unsigned>(i), allowNewInput) && ok;
    }
    for (std::size_t i = 0; i < mouse_.size(); ++i) {
        if (mouse_[i].Desired() || mouse_[i].Sent())
            ok = ReconcileMouse(static_cast<MouseButton>(i), allowNewInput) && ok;
    }
    return ok;
}

void WindowInputSink::ObserveWindowMessage(std::uint32_t message, std::uintptr_t wparam,
                                           std::uintptr_t lparam,
                                           std::uintptr_t extraInfo) noexcept {
    if (message == WM_MOUSEMOVE) {
        const POINT observed{static_cast<short>(LOWORD(lparam)),
                             static_cast<short>(HIWORD(lparam))};
        if (virtualPointerReady_ &&
            (observed.x != virtualPointer_.x || observed.y != virtualPointer_.y))
            accumulator_.Reset();
        virtualPointer_ = observed;
        virtualPointerReady_ = true;
    }

    if (message == WM_ACTIVATEAPP && wparam != 0) {
        physicalMouse_[Index(MouseButton::Left)] =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        physicalMouse_[Index(MouseButton::Right)] =
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        return;
    }
    if (g_dispatchDepth != 0 || IsTouchOwnedMouseExtraInfo(extraInfo))
        return;
    if (message == WM_LBUTTONDOWN)
        physicalMouse_[Index(MouseButton::Left)] = true;
    else if (message == WM_LBUTTONUP)
        physicalMouse_[Index(MouseButton::Left)] = false;
    else if (message == WM_RBUTTONDOWN)
        physicalMouse_[Index(MouseButton::Right)] = true;
    else if (message == WM_RBUTTONUP)
        physicalMouse_[Index(MouseButton::Right)] = false;
}

void WindowInputSink::ReleaseAll() noexcept {
    for (auto &key : keys_)
        key.SetDesired(false);
    buttonDesired_.fill(false);
    cameraActive_ = false;
    for (auto &button : mouse_)
        button.SetDesired(false);
    Reconcile(false);
    RestoreCameraCursor();
    virtualPointerReady_ = false;
    accumulator_.Reset();
}

} // namespace wxl::controller
