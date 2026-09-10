#pragma once

struct WXL_ControllerInputApiV1;

namespace wxl::controller {
class FeatureController;

class ControllerBridge {
  public:
    static void Bind(FeatureController *controller) noexcept;
    static WXL_ControllerInputApiV1 *Interface() noexcept;
};

} // namespace wxl::controller
