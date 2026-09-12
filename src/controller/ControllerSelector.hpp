#pragma once

#include "controller/ControllerTypes.hpp"

#include <optional>
#include <vector>

namespace wxl::controller {

class ControllerSelector {
  public:
    std::optional<DeviceInfo> SelectInitial(const std::vector<DeviceInfo> &devices);
    bool Disconnect(std::uint32_t instanceId) noexcept;
    std::optional<DeviceInfo> Reconnect(const std::vector<DeviceInfo> &devices);
    [[nodiscard]] const std::optional<DeviceInfo> &Active() const noexcept {
        return active_;
    }
    [[nodiscard]] bool HasIdentity() const noexcept { return !controllerOneIdentity_.empty(); }

  private:
    std::optional<DeviceInfo> active_;
    std::string controllerOneIdentity_;
};

} // namespace wxl::controller
