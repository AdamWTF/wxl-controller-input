#pragma once

#include <cmath>

namespace wxl::controller {

struct RelativeStep { int x{}; int y{}; };

class RelativeAccumulator final {
  public:
    RelativeStep Add(float x, float y) noexcept {
        remainderX_ += x;
        remainderY_ += y;
        const RelativeStep result{static_cast<int>(std::trunc(remainderX_)),
                                  static_cast<int>(std::trunc(remainderY_))};
        remainderX_ -= static_cast<float>(result.x);
        remainderY_ -= static_cast<float>(result.y);
        return result;
    }
    void Reset() noexcept { remainderX_ = remainderY_ = 0.0F; }

  private:
    float remainderX_{};
    float remainderY_{};
};

} // namespace wxl::controller
