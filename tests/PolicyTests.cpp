#include "bindings/Bindings.hpp"
#include "camera/CameraController.hpp"
#include "config/Config.hpp"
#include "controller/ControllerSelector.hpp"
#include "input/Deadzone.hpp"
#include "input/ModifierController.hpp"
#include "movement/MovementController.hpp"
#include "profiles/ProfileResolver.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace wxl::controller;
namespace {
int failures{};
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #x "\n";                 \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

struct MoveSink final : MovementSink {
    std::vector<std::pair<Movement, bool>> events;
    void SetMovement(Movement m, bool down) noexcept override {
        events.emplace_back(m, down);
    }
};
struct CamSink final : CameraSink {
    int begins{}, ends{}, moves{};
    float x{}, y{};
    bool allow{true};
    bool Begin(CameraPath) noexcept override {
        ++begins;
        return allow;
    }
    bool Move(float a, float b) noexcept override {
        ++moves;
        x = a;
        y = b;
        return allow;
    }
    void End() noexcept override {
        ++ends;
    }
};
struct BindSink final : BindingSink {
    std::vector<Binding> presses, releases;
    bool Press(const Binding &b) noexcept override {
        presses.push_back(b);
        return true;
    }
    void Release(const Binding &b) noexcept override {
        releases.push_back(b);
    }
};

void TestDeadzone() {
    CHECK(ApplyRadialDeadzone(0.18F, 0, 0.18F).x == 0);
    CHECK(ApplyRadialDeadzone(0.50F, 0, 0.18F).x > 0);
    const auto diagonal = ApplyRadialDeadzone(0.5F, 0.5F, 0.18F);
    CHECK(std::abs(diagonal.x - diagonal.y) < 0.0001F);
}

void TestMovement() {
    MoveSink sink;
    MovementController movement(sink);
    movement.Update(0, -1);
    CHECK((sink.events == std::vector<std::pair<Movement, bool>>{{Movement::Forward, true}}));
    movement.Update(0, 1);
    CHECK((sink.events[1] == std::pair{Movement::Forward, false}));
    CHECK((sink.events[2] == std::pair{Movement::Backward, true}));
    movement.Update(-1, -1);
    CHECK(movement.State()[0] && movement.State()[2]);
    movement.Cancel();
    const auto count = sink.events.size();
    movement.Cancel();
    CHECK(sink.events.size() == count);
}

void TestModifiers() {
    ModifierController m;
    CHECK(m.Update(0.49F, 0) == Layer::Base);
    CHECK(m.Update(0.50F, 0) == Layer::LT);
    CHECK(m.Update(0.45F, 0.50F) == Layer::LTRT);
    CHECK(m.Update(0.39F, 0.45F) == Layer::RT);
    CHECK(m.Update(0, 0.39F) == Layer::Base);
    m.Update(1, 1);
    m.Cancel();
    CHECK(m.CurrentLayer() == Layer::Base);
}

void TestCamera() {
    CamSink sink;
    CameraController camera(sink, CameraPath::MouseFallback, 0.15F, 2, 3, true);
    camera.Update(0.1F, 0);
    CHECK(sink.begins == 0);
    camera.Update(1, 0.5F);
    CHECK(sink.begins == 1 && sink.moves == 1 && sink.x > 0 && sink.y < 0);
    camera.Update(0, 0);
    CHECK(sink.ends == 1);
    camera.Cancel();
    CHECK(sink.ends == 1);
}

void TestBindings() {
    auto defaults = BuiltInBindings();
    CHECK(defaults.size() == 38);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::Base, Button::FaceSouth})).slot == 1);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::LT, Button::DPadLeft})).slot == 56);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::RT, Button::DPadLeft})).slot == 68);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::LTRT, Button::DPadLeft})).slot == 60);
    CHECK(!IsValid(ActionSlot{0}));
    CHECK(!IsValid(WowBinding{"MADEUP"}));
    CHECK(IsValid(KeyBinding{"5", {"CTRL"}}));
    CHECK(!IsValid(KeyBinding{"5", {"META"}}));
    CHECK(!IsValid(KeyBinding{"NOT_A_KEY", {}}));

    BindSink sink;
    BindingController controller(sink, defaults);
    controller.Update(Button::FaceSouth, true, Layer::Base);
    controller.Update(Button::FaceSouth, true, Layer::LT);
    controller.Update(Button::FaceSouth, false, Layer::LT);
    CHECK(sink.presses.size() == 1 && std::get<ActionSlot>(sink.presses[0]).slot == 1);
    CHECK(sink.releases.size() == 1 && std::get<ActionSlot>(sink.releases[0]).slot == 1);
    controller.Update(Button::FaceSouth, true, Layer::LT);
    CHECK(std::get<ActionSlot>(sink.presses[1]).slot == 49);
    controller.Cancel();
    const auto released = sink.releases.size();
    controller.Cancel();
    CHECK(sink.releases.size() == released);
}

void TestProfiles() {
    ProfileResolver profiles;
    const BindingKey key{Layer::Base, Button::FaceSouth};
    CHECK(profiles.Resolve(key)->source == BindingSource::BuiltIn);
    profiles.Global()[key] = ActionSlot{20};
    CHECK(profiles.Resolve(key)->source == BindingSource::Global);
    profiles.Character()[key] = ActionSlot{21};
    CHECK(profiles.Resolve(key)->source == BindingSource::Global);
    profiles.SetCharacterIdentity("Realm|Character");
    CHECK(profiles.Resolve(key)->source == BindingSource::Character);
}

void TestSelection() {
    ControllerSelector selector;
    const DeviceInfo a{1, 2, "A", "first", "xbox"};
    const DeviceInfo b{2, 0, "B", "second", "ps5"};
    CHECK(selector.SelectInitial({a, b})->stableId == "B");
    CHECK(!selector.Disconnect(1));
    CHECK(selector.Disconnect(2));
    CHECK(!selector.Reconnect({a}));
    CHECK(selector.Reconnect({DeviceInfo{9, 4, "B", "second", "ps5"}})->instanceId == 9);
}
} // namespace

int main() {
    TestDeadzone();
    TestMovement();
    TestModifiers();
    TestCamera();
    TestBindings();
    TestProfiles();
    TestSelection();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
