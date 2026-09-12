#include "controller/SdlControllerBackend.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace wxl::controller {
namespace {
float Axis(SDL_Gamepad *pad, SDL_GamepadAxis axis) {
    return static_cast<float>(SDL_GetGamepadAxis(pad, axis)) / 256.0F;
}
float Trigger(SDL_Gamepad *pad, SDL_GamepadAxis axis) {
    return std::clamp(static_cast<float>(SDL_GetGamepadAxis(pad, axis)) * 250.0F / 32767.0F,
                      0.0F, 250.0F);
}
const char *Safe(const char *value) {
    return value ? value : "";
}
} // namespace

SdlControllerBackend::SdlControllerBackend(DisconnectHandler disconnected)
    : disconnected_(std::move(disconnected)) {
}

SdlControllerBackend::~SdlControllerBackend() {
    Shutdown();
}

bool SdlControllerBackend::Initialize() noexcept {
    try {
        if (initialized_)
            return true;
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            error_ = Safe(SDL_GetError());
            return false;
        }
        initialized_ = true;
        const auto devices = Enumerate();
        const auto selected = selector_.SelectInitial(devices);
        if (selected && !Open(*selected))
            selector_.Disconnect(selected->instanceId);
        return true;
    } catch (...) {
        Shutdown();
        return false;
    }
}

DeviceInfo SdlControllerBackend::Describe(SDL_Gamepad *gamepad, std::uint32_t id) {
    DeviceInfo info;
    info.instanceId = id;
    info.playerIndex = SDL_GetGamepadPlayerIndex(gamepad);
    info.name = Safe(SDL_GetGamepadName(gamepad));
    if (const char *family = SDL_GetGamepadStringForType(SDL_GetGamepadType(gamepad)))
        info.family = family;
    const char *serial = SDL_GetGamepadSerial(gamepad);
    const char *path = SDL_GetGamepadPath(gamepad);
    if (serial && *serial)
        info.stableId = std::string("serial:") + serial;
    else if (path && *path)
        info.stableId = std::string("path:") + path;
    else {
        char guid[64]{};
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(static_cast<SDL_JoystickID>(id)), guid,
                         sizeof(guid));
        char fallback[256]{};
        std::snprintf(fallback, sizeof(fallback), "guid:%s:%04x:%04x:%s", guid,
                      SDL_GetGamepadVendor(gamepad), SDL_GetGamepadProduct(gamepad),
                      info.name.c_str());
        info.stableId = fallback;
    }
    return info;
}

std::vector<DeviceInfo> SdlControllerBackend::Enumerate() {
    std::vector<DeviceInfo> result;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids) {
        detectedCount_ = 0;
        return result;
    }
    detectedCount_ = count;
    for (int i = 0; i < count; ++i) {
        if (SDL_Gamepad *pad = SDL_OpenGamepad(ids[i])) {
            result.push_back(Describe(pad, static_cast<std::uint32_t>(ids[i])));
            SDL_CloseGamepad(pad);
        }
    }
    SDL_free(ids);
    return result;
}

bool SdlControllerBackend::Open(const DeviceInfo &device) {
    if (gamepad_)
        SDL_CloseGamepad(gamepad_);
    gamepad_ = SDL_OpenGamepad(static_cast<SDL_JoystickID>(device.instanceId));
    if (!gamepad_)
        error_ = Safe(SDL_GetError());
    return gamepad_ != nullptr;
}

void SdlControllerBackend::ProcessEvents() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            const auto active = selector_.Active();
            if (active && active->instanceId == static_cast<std::uint32_t>(event.gdevice.which)) {
                selector_.Disconnect(active->instanceId);
                if (gamepad_)
                    SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
                if (disconnected_)
                    disconnected_();
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && !selector_.Active()) {
            const auto devices = Enumerate();
            const auto selected = selector_.HasIdentity() ? selector_.Reconnect(devices)
                                                          : selector_.SelectInitial(devices);
            if (selected && !Open(*selected))
                selector_.Disconnect(selected->instanceId);
        }
    }
}

bool SdlControllerBackend::Poll(Snapshot &snapshot) noexcept {
    try {
        if (!initialized_)
            return false;
        SDL_UpdateGamepads();
        ProcessEvents();
        if (!gamepad_)
            return false;
        if (!SDL_GamepadConnected(gamepad_)) {
            const auto active = selector_.Active();
            if (active)
                selector_.Disconnect(active->instanceId);
            SDL_CloseGamepad(gamepad_);
            gamepad_ = nullptr;
            if (disconnected_)
                disconnected_();
            return false;
        }
        snapshot = {};
        snapshot.leftX = Axis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX);
        snapshot.leftY = Axis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY);
        snapshot.rightX = Axis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX);
        snapshot.rightY = Axis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY);
        snapshot.leftTrigger = Trigger(gamepad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        snapshot.rightTrigger = Trigger(gamepad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        constexpr std::array<SDL_GamepadButton, static_cast<std::size_t>(Button::Count)> map{
            SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
            SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_DPAD_UP,       SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_DPAD_DOWN,     SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_BACK,          SDL_GAMEPAD_BUTTON_START,
            SDL_GAMEPAD_BUTTON_GUIDE,         SDL_GAMEPAD_BUTTON_MISC1,
            SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
            SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,  SDL_GAMEPAD_BUTTON_LEFT_PADDLE2};
        for (std::size_t i = 0; i < map.size(); ++i)
            snapshot.buttons[i] = SDL_GetGamepadButton(gamepad_, map[i]);
        return true;
    } catch (...) {
        if (disconnected_)
            disconnected_();
        Shutdown();
        return false;
    }
}

const char *SdlControllerBackend::SdlVersion() noexcept {
    static char version[32]{};
    if (!version[0])
        std::snprintf(version, sizeof(version), "%d.%d.%d", SDL_MAJOR_VERSION,
                      SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    return version;
}

void SdlControllerBackend::Shutdown() noexcept {
    if (gamepad_)
        SDL_CloseGamepad(gamepad_);
    gamepad_ = nullptr;
    if (initialized_)
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    initialized_ = false;
}

} // namespace wxl::controller
