#pragma once

#include "controller/ControllerSelector.hpp"
#include "controller/ControllerTypes.hpp"

#include <functional>
#include <vector>

struct SDL_Gamepad;

namespace wxl::controller {

class SdlControllerBackend {
public:
    using DisconnectHandler = std::function<void()>;

    explicit SdlControllerBackend(DisconnectHandler disconnected);
    ~SdlControllerBackend();
    SdlControllerBackend(const SdlControllerBackend&) = delete;
    SdlControllerBackend& operator=(const SdlControllerBackend&) = delete;

    bool Initialize() noexcept;
    bool Poll(Snapshot& snapshot) noexcept;
    void Shutdown() noexcept;
    [[nodiscard]] const std::optional<DeviceInfo>& Active() const noexcept { return selector_.Active(); }

private:
    std::vector<DeviceInfo> Enumerate() noexcept;
    bool Open(const DeviceInfo& device) noexcept;
    void ProcessEvents() noexcept;
    static DeviceInfo Describe(SDL_Gamepad* gamepad, std::uint32_t id);

    DisconnectHandler disconnected_;
    ControllerSelector selector_;
    SDL_Gamepad* gamepad_{};
    bool initialized_{};
};

} // namespace wxl::controller

