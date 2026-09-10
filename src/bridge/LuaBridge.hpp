#pragma once

struct WXL_Api;

namespace wxl::controller {
class FeatureController;

class LuaBridge final {
  public:
    static bool AttachValidator(const WXL_Api &api) noexcept;
    static void Bind(FeatureController *controller) noexcept;
    static void Tick() noexcept;
    static bool Ready() noexcept;
    static const char *DegradedReason() noexcept;
};

} // namespace wxl::controller
