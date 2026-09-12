#pragma once

#include "config/Config.hpp"
#include "controller/ControllerTypes.hpp"
#include "input/WoWInputSink.hpp"

#include <array>

namespace wxl::controller {

struct MapperState {
    std::array<bool, Index(Key::Count)> keys{};
    std::array<bool, Index(MouseButton::Count)> mouseButtons{};
    float pointerX{};
    float pointerY{};
    bool cameraActive{};
};

class ConsolePortMapper final {
  public:
    ConsolePortMapper(IWoWInputSink &sink, Config config) noexcept;
    bool Update(const Snapshot &snapshot, bool allowMouse = true) noexcept;
    void CancelMouse() noexcept;
    void ReleaseAll() noexcept;
    [[nodiscard]] const MapperState &State() const noexcept { return state_; }
    [[nodiscard]] const Config &Settings() const noexcept { return config_; }

  private:
    bool SetKey(Key key, bool down) noexcept;
    bool SetMouseButton(MouseButton button, bool down) noexcept;
    bool SetCameraActive(bool active) noexcept;
    static float CurveAxis(float axis, float speed, float curve) noexcept;
    void ProcessPointer(float x, float y, float &outX, float &outY) const noexcept;

    IWoWInputSink &sink_;
    Config config_;
    MapperState state_{};
};

} // namespace wxl::controller
