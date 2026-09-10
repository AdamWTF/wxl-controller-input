#pragma once

#include "controller/ControllerTypes.hpp"

namespace wxl::controller {

class ModifierController {
  public:
    ModifierController(float activate = 0.50F, float release = 0.40F);
    Layer Update(float leftTrigger, float rightTrigger) noexcept;
    void Cancel() noexcept;
    [[nodiscard]] Layer CurrentLayer() const noexcept;
    [[nodiscard]] bool LeftActive() const noexcept {
        return left_;
    }
    [[nodiscard]] bool RightActive() const noexcept {
        return right_;
    }

  private:
    static void UpdateOne(float value, float activate, float release, bool &state) noexcept;
    float activate_;
    float release_;
    bool left_{};
    bool right_{};
};

} // namespace wxl::controller
