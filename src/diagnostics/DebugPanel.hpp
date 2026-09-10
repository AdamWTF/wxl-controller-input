#pragma once

struct WXL_Api;

namespace wxl::controller {
class FeatureController;
void DrawDebugPanel(const WXL_Api& api, const FeatureController& controller) noexcept;
} // namespace wxl::controller

