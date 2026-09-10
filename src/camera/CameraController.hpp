#pragma once

#include "input/Deadzone.hpp"

namespace wxl::controller {

enum class CameraPath { Disabled, Native, MouseFallback };

class CameraSink {
public:
    virtual ~CameraSink() = default;
    virtual bool Begin(CameraPath path) noexcept = 0;
    virtual bool Move(float horizontal, float vertical) noexcept = 0;
    virtual void End() noexcept = 0;
};

class CameraController {
public:
    CameraController(CameraSink& sink, CameraPath path, float deadzone = 0.15F,
                     float horizontalSensitivity = 1.0F,
                     float verticalSensitivity = 1.0F, bool invertY = false)
        : sink_(sink), path_(path), deadzone_(deadzone), horizontal_(horizontalSensitivity),
          vertical_(verticalSensitivity), invertY_(invertY) {}
    void Update(float x, float y) noexcept;
    void Cancel() noexcept;
    [[nodiscard]] bool Active() const noexcept { return active_; }

private:
    CameraSink& sink_;
    CameraPath path_;
    float deadzone_;
    float horizontal_;
    float vertical_;
    bool invertY_;
    bool active_{};
};

} // namespace wxl::controller

