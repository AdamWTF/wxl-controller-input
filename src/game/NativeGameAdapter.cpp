#include "game/NativeGameAdapter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>

namespace wxl::controller {
namespace {
// These bindings are confined to this adapter and to the exact supported client executable:
// WoW 3.3.5a build 12340, SHA-256
// B8BD1A0DA194A4098B32D2B6F5798286CD1B07CDEEBDB0E7A1424235CB9DD379.
constexpr std::uintptr_t kInputControl = 0x00C24954;
constexpr std::uintptr_t kInputBegin = 0x005FA170;
constexpr std::uintptr_t kInputEnd = 0x005FA450;
constexpr std::uintptr_t kInputCommit = 0x005FBBC0;
constexpr std::uintptr_t kUseAction = 0x005ABBC0;

constexpr std::uint32_t kForward = 0x0010;
constexpr std::uint32_t kBackward = 0x0020;
constexpr std::uint32_t kStrafeLeft = 0x0040;
constexpr std::uint32_t kStrafeRight = 0x0080;
constexpr std::uint32_t kJump = 0x2000;
constexpr float kCameraPixelsPerSecond = 900.0F;

using BeginFn = int(__thiscall *)(void *, std::uint32_t, std::uint32_t);
using EndFn = int(__thiscall *)(void *, std::uint32_t, std::uint32_t, int);
using CommitFn = void(__thiscall *)(void *, std::uint32_t, int);
struct TargetContext {
    std::uint32_t low{};
    std::uint32_t high{};
};
using UseActionFn = void(__cdecl *)(int, const TargetContext *, const char *);

template <typename T> T Native(std::uintptr_t address) noexcept {
    return reinterpret_cast<T>(address);
}

std::optional<unsigned> VirtualKey(const std::string &name) noexcept {
    if (name.size() == 1) {
        const unsigned char value = static_cast<unsigned char>(name.front());
        if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))
            return value;
        if (value == '-')
            return VK_OEM_MINUS;
        if (value == '=')
            return VK_OEM_PLUS;
    }
    struct NamedKey {
        const char *name;
        unsigned key;
    };
    static constexpr NamedKey keys[]{
        {"BACKSPACE", VK_BACK},  {"DELETE", VK_DELETE}, {"DOWN", VK_DOWN},
        {"END", VK_END},         {"ENTER", VK_RETURN},  {"ESCAPE", VK_ESCAPE},
        {"HOME", VK_HOME},       {"INSERT", VK_INSERT}, {"LEFT", VK_LEFT},
        {"NUMLOCK", VK_NUMLOCK}, {"PAGEDOWN", VK_NEXT}, {"PAGEUP", VK_PRIOR},
        {"RIGHT", VK_RIGHT},     {"SPACE", VK_SPACE},   {"TAB", VK_TAB},
        {"UP", VK_UP},           {"F1", VK_F1},         {"F2", VK_F2},
        {"F3", VK_F3},           {"F4", VK_F4},         {"F5", VK_F5},
        {"F6", VK_F6},           {"F7", VK_F7},         {"F8", VK_F8},
        {"F9", VK_F9},           {"F10", VK_F10},       {"F11", VK_F11},
        {"F12", VK_F12}};
    for (const auto &entry : keys)
        if (name == entry.name)
            return entry.key;
    return std::nullopt;
}

std::optional<unsigned> ModifierKey(const std::string &name) noexcept {
    if (name == "ALT")
        return VK_MENU;
    if (name == "CTRL")
        return VK_CONTROL;
    if (name == "SHIFT")
        return VK_SHIFT;
    return std::nullopt;
}

std::optional<KeyBinding> NamedBinding(const std::string &command) {
    if (command == "JUMP")
        return std::nullopt;
    if (command == "TARGETNEARESTENEMY")
        return KeyBinding{"TAB", {}};
    if (command == "TARGETPREVIOUSENEMY")
        return KeyBinding{"TAB", {"SHIFT"}};
    if (command == "TARGETNEARESTFRIEND")
        return KeyBinding{"TAB", {"CTRL"}};
    if (command == "TARGETPREVIOUSFRIEND")
        return KeyBinding{"TAB", {"CTRL", "SHIFT"}};
    if (command == "TOGGLEAUTORUN")
        return KeyBinding{"NUMLOCK", {}};
    if (command == "TOGGLEGAMEMENU")
        return KeyBinding{"ESCAPE", {}};
    if (command == "TOGGLEWORLDMAP")
        return KeyBinding{"M", {}};
    return std::nullopt;
}

std::uint32_t MovementControl(Movement movement) noexcept {
    switch (movement) {
    case Movement::Forward:
        return kForward;
    case Movement::Backward:
        return kBackward;
    case Movement::StrafeLeft:
        return kStrafeLeft;
    case Movement::StrafeRight:
        return kStrafeRight;
    }
    return 0;
}
} // namespace

NativeGameAdapter::NativeGameAdapter() noexcept {
    RefreshWindow();
}

void NativeGameAdapter::SetTime(std::uint32_t timeMs) noexcept {
    timeMs_ = timeMs;
}

bool NativeGameAdapter::RefreshWindow() noexcept {
    if (window_ && IsWindow(window_))
        return true;
    window_ = FindWindowW(L"GxWindowClassD3d", nullptr);
    return window_ != nullptr;
}

bool NativeGameAdapter::SetNativeControl(std::uint32_t control, bool down) noexcept {
    void *const input = *reinterpret_cast<void **>(kInputControl);
    if (!input || control == 0)
        return false;
    const bool changed = down ? Native<BeginFn>(kInputBegin)(input, control, timeMs_) != 0
                              : Native<EndFn>(kInputEnd)(input, control, timeMs_, 0) != 0;
    if (changed)
        Native<CommitFn>(kInputCommit)(input, timeMs_, 1);
    return changed;
}

bool NativeGameAdapter::SetMovement(Movement movement, bool down) noexcept {
    return SetNativeControl(MovementControl(movement), down);
}

bool NativeGameAdapter::SendKey(unsigned virtualKey, bool down) noexcept {
    if (!RefreshWindow() || virtualKey >= keys_.size() || keys_[virtualKey] == down)
        return false;
    const unsigned scan = MapVirtualKeyA(virtualKey, MAPVK_VK_TO_VSC);
    LPARAM details = 1L | (static_cast<LPARAM>(scan) << 16);
    if (!down)
        details |= static_cast<LPARAM>(3u) << 30;
    SendMessageA(window_, down ? WM_KEYDOWN : WM_KEYUP, virtualKey, details);
    keys_[virtualKey] = down;
    return true;
}

bool NativeGameAdapter::PressKeyBinding(const KeyBinding &binding) noexcept {
    const auto key = VirtualKey(binding.key);
    if (!key)
        return false;
    for (const auto &name : binding.modifiers) {
        const auto modifier = ModifierKey(name);
        if (!modifier)
            return false;
        SendKey(*modifier, true);
    }
    return SendKey(*key, true);
}

void NativeGameAdapter::ReleaseKeyBinding(const KeyBinding &binding) noexcept {
    if (const auto key = VirtualKey(binding.key))
        SendKey(*key, false);
    for (auto it = binding.modifiers.rbegin(); it != binding.modifiers.rend(); ++it)
        if (const auto modifier = ModifierKey(*it))
            SendKey(*modifier, false);
}

bool NativeGameAdapter::Press(const Binding &binding) noexcept {
    return std::visit(
        [this](const auto &value) -> bool {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ActionSlot>) {
                if (value.slot < 1 || value.slot > 120)
                    return false;
                const TargetContext noTarget{};
                Native<UseActionFn>(kUseAction)(static_cast<int>(value.slot - 1), &noTarget,
                                                nullptr);
                return true;
            } else if constexpr (std::is_same_v<T, WowBinding>) {
                if (value.command == "JUMP")
                    return SetNativeControl(kJump, true);
                if (const auto key = NamedBinding(value.command))
                    return PressKeyBinding(*key);
                return false;
            } else if constexpr (std::is_same_v<T, KeyBinding>) {
                return PressKeyBinding(value);
            } else {
                return false;
            }
        },
        binding);
}

void NativeGameAdapter::Release(const Binding &binding) noexcept {
    std::visit(
        [this](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, WowBinding>) {
                if (value.command == "JUMP")
                    SetNativeControl(kJump, false);
                else if (const auto key = NamedBinding(value.command))
                    ReleaseKeyBinding(*key);
            } else if constexpr (std::is_same_v<T, KeyBinding>) {
                ReleaseKeyBinding(value);
            }
        },
        binding);
}

bool NativeGameAdapter::SetRightButton(bool down) noexcept {
    if (!RefreshWindow() || rightButtonOwned_ == down)
        return false;
    POINT cursor{};
    if (!GetCursorPos(&cursor))
        return false;
    ScreenToClient(window_, &cursor);
    SendMessageA(window_, down ? WM_RBUTTONDOWN : WM_RBUTTONUP, down ? MK_RBUTTON : 0,
                 MAKELPARAM(cursor.x, cursor.y));
    rightButtonOwned_ = down;
    return true;
}

bool NativeGameAdapter::BeginCamera() noexcept {
    if (!RefreshWindow() || GetForegroundWindow() != window_)
        return false;
    savedCursor_ = GetCursorPos(&savedCursorPosition_) != FALSE;
    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)
        return true;
    return SetRightButton(true);
}

bool NativeGameAdapter::MoveCamera(float horizontal, float vertical, float deltaSeconds) noexcept {
    if (!RefreshWindow() || GetForegroundWindow() != window_)
        return false;
    const bool physical = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (!rightButtonOwned_ && !physical && !SetRightButton(true))
        return false;
    const float dt = std::clamp(deltaSeconds, 0.0F, 0.1F);
    cameraRemainderX_ += horizontal * kCameraPixelsPerSecond * dt;
    cameraRemainderY_ += vertical * kCameraPixelsPerSecond * dt;
    const int dx = static_cast<int>(std::trunc(cameraRemainderX_));
    const int dy = static_cast<int>(std::trunc(cameraRemainderY_));
    cameraRemainderX_ -= static_cast<float>(dx);
    cameraRemainderY_ -= static_cast<float>(dy);
    if (dx == 0 && dy == 0)
        return true;
    POINT cursor{};
    RECT client{};
    if (!GetCursorPos(&cursor) || !GetClientRect(window_, &client))
        return false;
    cursor.x += dx;
    cursor.y += dy;
    POINT low{client.left, client.top};
    POINT high{client.right - 1, client.bottom - 1};
    ClientToScreen(window_, &low);
    ClientToScreen(window_, &high);
    cursor.x = std::clamp(cursor.x, low.x, high.x);
    cursor.y = std::clamp(cursor.y, low.y, high.y);
    SetCursorPos(cursor.x, cursor.y);
    ScreenToClient(window_, &cursor);
    SendMessageA(window_, WM_MOUSEMOVE, (rightButtonOwned_ || physical) ? MK_RBUTTON : 0,
                 MAKELPARAM(cursor.x, cursor.y));
    return true;
}

void NativeGameAdapter::EndCamera() noexcept {
    const bool physical = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (rightButtonOwned_ && !physical)
        SetRightButton(false);
    else
        rightButtonOwned_ = false;
    cameraRemainderX_ = 0.0F;
    cameraRemainderY_ = 0.0F;
    if (savedCursor_ && !physical && window_ && GetForegroundWindow() == window_)
        SetCursorPos(savedCursorPosition_.x, savedCursorPosition_.y);
    savedCursor_ = false;
}

} // namespace wxl::controller
