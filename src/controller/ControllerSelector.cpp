#include "controller/ControllerSelector.hpp"

#include <algorithm>

namespace wxl::controller {

std::optional<DeviceInfo> ControllerSelector::SelectInitial(const std::vector<DeviceInfo>& devices) {
    if (active_ || !controllerOneIdentity_.empty() || devices.empty()) return active_;
    auto chosen = std::find_if(devices.begin(), devices.end(), [](const DeviceInfo& d) { return d.playerIndex == 0; });
    if (chosen == devices.end()) chosen = devices.begin();
    active_ = *chosen;
    controllerOneIdentity_ = chosen->stableId;
    return active_;
}

bool ControllerSelector::Disconnect(std::uint32_t instanceId) noexcept {
    if (!active_ || active_->instanceId != instanceId) return false;
    active_.reset();
    return true;
}

std::optional<DeviceInfo> ControllerSelector::Reconnect(const std::vector<DeviceInfo>& devices) {
    if (active_ || controllerOneIdentity_.empty()) return active_;
    const auto match = std::find_if(devices.begin(), devices.end(), [&](const DeviceInfo& d) {
        return d.stableId == controllerOneIdentity_;
    });
    if (match != devices.end()) active_ = *match;
    return active_;
}

} // namespace wxl::controller

