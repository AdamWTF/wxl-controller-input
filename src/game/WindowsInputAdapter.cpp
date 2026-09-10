#include "game/WindowsInputAdapter.hpp"

#include "game/CompatibilityBindings.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace wxl::controller {
namespace {
constexpr LPARAM kSyntheticKeyTag = 1L << 25;
constexpr ULONG_PTR kSyntheticMouseTag = 0x57584C43u;
constexpr float kCameraPixelsPerSecond = 900.0F;

std::optional<unsigned> VirtualKey(const std::string &name) {
    if (name.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(name.front());
        if (c >= 'A' && c <= 'Z')
            return c;
        if (c >= '0' && c <= '9')
            return c;
        if (c == '-')
            return VK_OEM_MINUS;
        if (c == '=')
            return VK_OEM_PLUS;
    }
    struct NamedKey {
        const char *name;
        unsigned key;
    };
    static constexpr NamedKey keys[]{{"BACKSPACE", VK_BACK},
                                     {"DELETE", VK_DELETE},
                                     {"DOWN", VK_DOWN},
                                     {"END", VK_END},
                                     {"ENTER", VK_RETURN},
                                     {"ESCAPE", VK_ESCAPE},
                                     {"HOME", VK_HOME},
                                     {"INSERT", VK_INSERT},
                                     {"LEFT", VK_LEFT},
                                     {"NUMLOCK", VK_NUMLOCK},
                                     {"PAGEDOWN", VK_NEXT},
                                     {"PAGEUP", VK_PRIOR},
                                     {"RIGHT", VK_RIGHT},
                                     {"SPACE", VK_SPACE},
                                     {"TAB", VK_TAB},
                                     {"UP", VK_UP},
                                     {"F1", VK_F1},
                                     {"F2", VK_F2},
                                     {"F3", VK_F3},
                                     {"F4", VK_F4},
                                     {"F5", VK_F5},
                                     {"F6", VK_F6},
                                     {"F7", VK_F7},
                                     {"F8", VK_F8},
                                     {"F9", VK_F9},
                                     {"F10", VK_F10},
                                     {"F11", VK_F11},
                                     {"F12", VK_F12}};
    for (const auto &entry : keys)
        if (name == entry.name)
            return entry.key;
    return std::nullopt;
}

std::optional<unsigned> ModifierKey(const std::string &name) {
    if (name == "ALT")
        return VK_MENU;
    if (name == "CTRL")
        return VK_CONTROL;
    if (name == "SHIFT")
        return VK_SHIFT;
    return std::nullopt;
}

unsigned MovementKey(Movement movement) {
    switch (movement) {
    case Movement::Forward:
        return 'W';
    case Movement::Backward:
        return 'S';
    case Movement::StrafeLeft:
        return 'Q';
    case Movement::StrafeRight:
        return 'E';
    }
    return 0;
}

bool ExtendedKey(unsigned key) {
    return key == VK_INSERT || key == VK_DELETE || key == VK_HOME || key == VK_END ||
           key == VK_PRIOR || key == VK_NEXT || key == VK_LEFT || key == VK_RIGHT ||
           key == VK_UP || key == VK_DOWN || key == VK_NUMLOCK;
}
} // namespace

WindowsInputAdapter::WindowsInputAdapter() noexcept {
    RefreshTarget();
}

bool WindowsInputAdapter::RefreshTarget() noexcept {
    if (target_ && IsWindow(target_))
        return true;
    target_ = FindWindowW(L"GxWindowClassD3d", nullptr);
    return target_ != nullptr;
}

bool WindowsInputAdapter::PostKey(unsigned virtualKey, bool down) noexcept {
    if (!RefreshTarget() || virtualKey >= keyReferences_.size())
        return false;
    const UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    LPARAM details = 1L | (static_cast<LPARAM>(scanCode) << 16) | kSyntheticKeyTag;
    if (ExtendedKey(virtualKey))
        details |= 1L << 24;
    if (!down)
        details |= (1L << 30) | (1L << 31);
    return PostMessageW(target_, down ? WM_KEYDOWN : WM_KEYUP, virtualKey, details) != FALSE;
}

bool WindowsInputAdapter::AcquireKey(unsigned virtualKey) noexcept {
    if (virtualKey >= keyReferences_.size())
        return false;
    auto &references = keyReferences_[virtualKey];
    if (references == 0 && !PostKey(virtualKey, true))
        return false;
    if (references != USHRT_MAX)
        ++references;
    return true;
}

void WindowsInputAdapter::ReleaseKey(unsigned virtualKey) noexcept {
    if (virtualKey >= keyReferences_.size())
        return;
    auto &references = keyReferences_[virtualKey];
    if (references == 0)
        return;
    --references;
    if (references == 0 && (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) == 0)
        PostKey(virtualKey, false);
}

bool WindowsInputAdapter::PressChord(const KeyBinding &binding) noexcept {
    const auto key = VirtualKey(binding.key);
    if (!key)
        return false;
    std::vector<unsigned> acquired;
    acquired.reserve(binding.modifiers.size() + 1);
    for (const auto &name : binding.modifiers) {
        const auto modifier = ModifierKey(name);
        if (!modifier || !AcquireKey(*modifier)) {
            for (auto it = acquired.rbegin(); it != acquired.rend(); ++it)
                ReleaseKey(*it);
            return false;
        }
        acquired.push_back(*modifier);
    }
    if (!AcquireKey(*key)) {
        for (auto it = acquired.rbegin(); it != acquired.rend(); ++it)
            ReleaseKey(*it);
        return false;
    }
    return true;
}

void WindowsInputAdapter::ReleaseChord(const KeyBinding &binding) noexcept {
    if (const auto key = VirtualKey(binding.key))
        ReleaseKey(*key);
    for (auto it = binding.modifiers.rbegin(); it != binding.modifiers.rend(); ++it)
        if (const auto modifier = ModifierKey(*it))
            ReleaseKey(*modifier);
}

bool WindowsInputAdapter::SetMovement(Movement movement, bool down) noexcept {
    const unsigned key = MovementKey(movement);
    if (!key)
        return false;
    if (down)
        return AcquireKey(key);
    ReleaseKey(key);
    return true;
}

bool WindowsInputAdapter::Press(const Binding &binding) noexcept {
    const auto resolved = ResolveCompatibilityBinding(binding);
    return resolved && PressChord(*resolved);
}

void WindowsInputAdapter::Release(const Binding &binding) noexcept {
    if (const auto resolved = ResolveCompatibilityBinding(binding))
        ReleaseChord(*resolved);
}

bool WindowsInputAdapter::PostRightButton(bool down) noexcept {
    if (!RefreshTarget())
        return false;
    POINT cursor{};
    if (!GetCursorPos(&cursor))
        return false;
    ScreenToClient(target_, &cursor);
    const LPARAM position = MAKELPARAM(cursor.x, cursor.y);
    return PostMessageW(target_, down ? WM_RBUTTONDOWN : WM_RBUTTONUP,
                        down ? MK_RBUTTON : 0, position) != FALSE;
}

bool WindowsInputAdapter::BeginCamera() noexcept {
    if (!RefreshTarget() || GetForegroundWindow() != target_)
        return false;
    cameraRequested_ = true;
    POINT cursor{};
    if (GetCursorPos(&cursor)) {
        savedCursorX_ = cursor.x;
        savedCursorY_ = cursor.y;
        savedCursor_ = true;
    }
    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)
        return true;
    syntheticRightButton_ = PostRightButton(true);
    return syntheticRightButton_;
}

bool WindowsInputAdapter::MoveCamera(float horizontal, float vertical,
                                     float deltaSeconds) noexcept {
    if (!cameraRequested_ || !RefreshTarget() || GetForegroundWindow() != target_)
        return false;
    if (!syntheticRightButton_ && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0) {
        syntheticRightButton_ = PostRightButton(true);
        if (!syntheticRightButton_)
            return false;
    }
    const float dt = std::clamp(deltaSeconds, 0.0F, 0.1F);
    const LONG dx = static_cast<LONG>(std::lround(horizontal * kCameraPixelsPerSecond * dt));
    const LONG dy = static_cast<LONG>(std::lround(vertical * kCameraPixelsPerSecond * dt));
    if (dx == 0 && dy == 0)
        return true;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dwExtraInfo = kSyntheticMouseTag;
    return SendInput(1, &input, sizeof(input)) == 1;
}

void WindowsInputAdapter::EndCamera() noexcept {
    cameraRequested_ = false;
    const bool physicalRightButton = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (syntheticRightButton_ && !physicalRightButton)
        PostRightButton(false);
    syntheticRightButton_ = false;
    if (savedCursor_ && !physicalRightButton && target_ && GetForegroundWindow() == target_)
        SetCursorPos(savedCursorX_, savedCursorY_);
    savedCursor_ = false;
}

void WindowsInputAdapter::HandleWindowMessage(std::uint32_t message, std::uintptr_t wparam,
                                               std::uintptr_t lparam) noexcept {
    if (message == WM_KEYUP && (static_cast<LPARAM>(lparam) & kSyntheticKeyTag) == 0) {
        const unsigned key = static_cast<unsigned>(wparam);
        if (key < keyReferences_.size() && keyReferences_[key] > 0)
            PostKey(key, true);
    } else if (message == WM_RBUTTONUP && cameraRequested_) {
        syntheticRightButton_ = PostRightButton(true);
    }
}

} // namespace wxl::controller
