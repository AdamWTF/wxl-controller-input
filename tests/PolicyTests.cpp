#include "config/Config.hpp"
#include "controller/ControllerSelector.hpp"
#include "input/DigitalOwnership.hpp"
#include "input/MouseSourceFilter.hpp"
#include "input/RelativeAccumulator.hpp"
#include "input/TouchInputGate.hpp"
#include "mapper/ConsolePortMapper.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

using namespace wxl::controller;
namespace {
int failures{};
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #x "\n";             \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

struct FakeSink final : IWoWInputSink {
    std::vector<std::pair<Key, bool>> keys;
    std::vector<std::pair<MouseButton, bool>> mouse;
    std::vector<std::pair<float, float>> motion;
    std::vector<bool> camera;
    int releaseAll{};
    bool allow{true};
    bool SetKey(Key key, bool down) noexcept override {
        keys.emplace_back(key, down);
        return allow;
    }
    bool SetMouseButton(MouseButton button, bool down) noexcept override {
        mouse.emplace_back(button, down);
        return allow;
    }
    bool SetCameraActive(bool active) noexcept override {
        camera.push_back(active);
        return allow;
    }
    bool MoveCameraRelative(float x, float y) noexcept override {
        motion.emplace_back(x, y);
        return allow;
    }
    bool Reconcile(bool) noexcept override { return allow; }
    void ReleaseAll() noexcept override { ++releaseAll; }
};

void TestFixedButtonMappings() {
    struct Case { Button button; Key key; };
    constexpr Case cases[]{
        {Button::DPadUp, Key::F1}, {Button::DPadRight, Key::F2},
        {Button::DPadDown, Key::F3}, {Button::DPadLeft, Key::F4},
        {Button::View, Key::F5}, {Button::Menu, Key::F6},
        {Button::LeftShoulder, Key::F7}, {Button::RightShoulder, Key::F8},
        {Button::FaceNorth, Key::Numpad4}, {Button::FaceEast, Key::F10},
        {Button::FaceSouth, Key::F11}, {Button::FaceWest, Key::F12},
        {Button::Guide, Key::NumpadMultiply}, {Button::Misc1, Key::NumpadAdd},
        {Button::RightPaddle1, Key::Numpad0}, {Button::RightPaddle2, Key::Numpad1},
        {Button::LeftPaddle1, Key::Numpad2}, {Button::LeftPaddle2, Key::Numpad3},
    };
    for (const auto &test : cases) {
        FakeSink sink;
        ConsolePortMapper mapper(sink, Config{});
        Snapshot state{};
        state.buttons[Index(test.button)] = true;
        CHECK(mapper.Update(state));
        CHECK((sink.keys == std::vector<std::pair<Key, bool>>{{test.key, true}}));
        CHECK(mapper.Update(state));
        CHECK(sink.keys.size() == 1);
        state.buttons[Index(test.button)] = false;
        CHECK(mapper.Update(state));
        CHECK(sink.keys.size() == 2 && sink.keys.back() == std::make_pair(test.key, false));
    }
}

void TestModifiersAndMouseButtons() {
    FakeSink sink;
    ConsolePortMapper mapper(sink, Config{});
    Snapshot state{};
    state.leftTrigger = 80.0F;
    state.rightTrigger = 80.0F;
    CHECK(mapper.Update(state));
    CHECK(sink.keys.empty());
    state.leftTrigger = 80.01F;
    state.rightTrigger = 250.0F;
    state.buttons[Index(Button::FaceSouth)] = true;
    state.buttons[Index(Button::LeftStick)] = true;
    state.buttons[Index(Button::RightStick)] = true;
    CHECK(mapper.Update(state));
    CHECK(mapper.State().keys[Index(Key::LeftShift)]);
    CHECK(mapper.State().keys[Index(Key::LeftControl)]);
    CHECK(mapper.State().keys[Index(Key::F11)]);
    CHECK((sink.mouse == std::vector<std::pair<MouseButton, bool>>{
                             {MouseButton::Left, true}, {MouseButton::Right, true}}));
    CHECK(mapper.Update(state));
    CHECK(sink.mouse.size() == 2);
    mapper.ReleaseAll();
    CHECK(!mapper.State().keys[Index(Key::LeftShift)]);
    CHECK(sink.mouse.size() == 4);
    CHECK(sink.releaseAll == 1);
    mapper.ReleaseAll();
    CHECK(sink.releaseAll == 2);
    CHECK(sink.mouse.size() == 4);
}

void TestMovementAndHelpers() {
    FakeSink sink;
    ConsolePortMapper mapper(sink, Config{});
    Snapshot state{};
    state.leftX = 100.0F;
    state.leftY = -50.0F;
    CHECK(mapper.Update(state));
    CHECK(mapper.State().keys[Index(Key::W)] && mapper.State().keys[Index(Key::D)]);
    CHECK(mapper.State().keys[Index(Key::H)] && !mapper.State().keys[Index(Key::V)]);
    state.leftX = -50.0F;
    state.leftY = 100.0F;
    CHECK(mapper.Update(state));
    CHECK(mapper.State().keys[Index(Key::A)] && mapper.State().keys[Index(Key::S)]);
    CHECK(!mapper.State().keys[Index(Key::H)] && mapper.State().keys[Index(Key::V)]);
    state = {};
    CHECK(mapper.Update(state));
    for (Key key : {Key::W, Key::A, Key::S, Key::D, Key::H, Key::V})
        CHECK(!mapper.State().keys[Index(key)]);

    struct Direction { float x; float y; Key key; };
    constexpr Direction cardinals[]{
        {0.0F, -100.0F, Key::W}, {-100.0F, 0.0F, Key::A},
        {0.0F, 100.0F, Key::S}, {100.0F, 0.0F, Key::D}};
    for (const auto &direction : cardinals) {
        state = {};
        state.leftX = direction.x;
        state.leftY = direction.y;
        CHECK(mapper.Update(state));
        CHECK(mapper.State().keys[Index(direction.key)]);
        state = {};
        CHECK(mapper.Update(state));
    }

    Config simple;
    simple.simpleRadial = true;
    FakeSink simpleSink;
    ConsolePortMapper simpleMapper(simpleSink, simple);
    state.leftX = 100.0F;
    state.leftY = -50.0F;
    CHECK(simpleMapper.Update(state));
    CHECK(!simpleMapper.State().keys[Index(Key::H)]);

    Config swapped;
    swapped.swapSticks = true;
    FakeSink swappedSink;
    ConsolePortMapper swappedMapper(swappedSink, swapped);
    state = {};
    state.rightY = -100.0F;
    state.leftX = 127.0F;
    CHECK(swappedMapper.Update(state));
    CHECK(swappedMapper.State().keys[Index(Key::W)]);
    CHECK(!swappedSink.motion.empty());
}

void TestPointerCurve() {
    FakeSink sink;
    ConsolePortMapper mapper(sink, Config{});
    Snapshot state{};
    state.rightX = 19.0F;
    CHECK(mapper.Update(state));
    CHECK(sink.motion.empty());
    state.rightX = 20.0F;
    CHECK(mapper.Update(state));
    CHECK(sink.motion.empty());
    state.rightX = 127.0F;
    CHECK(mapper.Update(state));
    CHECK(sink.motion.size() == 1);
    CHECK((sink.camera == std::vector<bool>{true}));
    CHECK(mapper.State().cameraActive);
    CHECK(sink.motion.back().first > 13.0F && sink.motion.back().second == 0.0F);
    state.rightX = -127.0F;
    state.rightY = 127.0F;
    CHECK(mapper.Update(state));
    CHECK(sink.motion.back().first < 0.0F && sink.motion.back().second > 0.0F);
    state = {};
    CHECK(mapper.Update(state));
    CHECK(mapper.State().pointerX == 0.0F && mapper.State().pointerY == 0.0F);
    CHECK((sink.camera == std::vector<bool>{true, false}));
    CHECK(!mapper.State().cameraActive);
}

void TestMouseSuppressionAndNeutralRearmContract() {
    FakeSink sink;
    ConsolePortMapper mapper(sink, Config{});
    Snapshot state{};
    state.rightX = 127.0F;
    state.buttons[Index(Button::LeftStick)] = true;
    state.buttons[Index(Button::RightStick)] = true;
    CHECK(mapper.Update(state));
    CHECK(mapper.State().cameraActive);
    CHECK(mapper.Update(state, false));
    CHECK(!mapper.State().cameraActive);
    CHECK(!mapper.State().mouseButtons[Index(MouseButton::Left)]);
    CHECK(!mapper.State().mouseButtons[Index(MouseButton::Right)]);
    CHECK(sink.camera.back() == false);
    CHECK(sink.mouse.size() == 4);

    const auto cameraTransitions = sink.camera.size();
    const auto mouseTransitions = sink.mouse.size();
    CHECK(mapper.Update(state, false));
    CHECK(sink.camera.size() == cameraTransitions);
    CHECK(sink.mouse.size() == mouseTransitions);

    state = {};
    CHECK(mapper.Update(state, true));
    CHECK(!mapper.State().cameraActive);
    state.rightX = 127.0F;
    CHECK(mapper.Update(state, true));
    CHECK(mapper.State().cameraActive);
}

void TestTouchInputGate() {
    TouchInputGate gate;
    CHECK(gate.Allow(0, true));
    CHECK(gate.Begin(7));
    CHECK(gate.Active() && gate.WaitingForNeutral());
    CHECK(!gate.Begin(8));
    CHECK(!gate.Allow(100, true));
    CHECK(!gate.End(8, 100));
    CHECK(gate.End(7, 100));
    CHECK(!gate.Active());
    CHECK(!gate.Allow(349, true));
    CHECK(!gate.Allow(350, false));
    CHECK(gate.Allow(350, true));
    CHECK(gate.Allow(351, false));

    CHECK(gate.Begin(9));
    gate.Cancel();
    CHECK(!gate.Active() && gate.WaitingForNeutral());
    CHECK(!gate.Allow(0, false));
    CHECK(gate.Allow(0, true));
}

void TestMouseSourceFilter() {
    CHECK(IsTouchOwnedMouseExtraInfo(kTouchSyntheticMouseTag));
    CHECK(IsTouchOwnedMouseExtraInfo(0xFF515780));
    CHECK(IsTouchOwnedMouseExtraInfo(0xFF515781));
    CHECK(!IsTouchOwnedMouseExtraInfo(0));
    CHECK(!IsTouchOwnedMouseExtraInfo(0xFF515700));
    CHECK(!IsTouchOwnedMouseExtraInfo(0x12345678));
}

void TestSinkFailurePropagation() {
    FakeSink sink;
    sink.allow = false;
    ConsolePortMapper mapper(sink, Config{});
    Snapshot state{};
    state.buttons[Index(Button::FaceSouth)] = true;
    CHECK(!mapper.Update(state));
    mapper.ReleaseAll();
    CHECK(!mapper.State().keys[Index(Key::F11)]);
    CHECK(sink.releaseAll == 1);
}

void TestOwnershipCoexistence() {
    DigitalOwnership ownership;
    ownership.SetDesired(true);
    CHECK(ownership.Reconcile(true, true) == OwnershipTransition::None);
    CHECK(ownership.Reconcile(false, true) == OwnershipTransition::SendDown);
    CHECK(ownership.Reconcile(true, true) == OwnershipTransition::None);
    CHECK(ownership.Reconcile(false, true) == OwnershipTransition::ResendDown);
    ownership.SetDesired(false);
    CHECK(ownership.Reconcile(true, true) == OwnershipTransition::None);
    CHECK(!ownership.Sent());

    ownership.SetDesired(true);
    CHECK(ownership.Reconcile(false, false) == OwnershipTransition::Blocked);
    CHECK(!ownership.Sent());
    CHECK(ownership.Reconcile(false, true) == OwnershipTransition::SendDown);
    ownership.Undo(OwnershipTransition::SendDown);
    CHECK(!ownership.Sent());
    CHECK(ownership.Reconcile(false, true) == OwnershipTransition::SendDown);
    ownership.SetDesired(false);
    CHECK(ownership.Reconcile(false, false) == OwnershipTransition::SendUp);
}

void TestRelativeAccumulator() {
    RelativeAccumulator accumulator;
    CHECK(accumulator.Add(0.4F, -0.4F).x == 0);
    CHECK(accumulator.Add(0.4F, -0.4F).y == 0);
    const auto third = accumulator.Add(0.4F, -0.4F);
    CHECK(third.x == 1 && third.y == -1);
    accumulator.Reset();
    const auto reset = accumulator.Add(0.4F, 0.4F);
    CHECK(reset.x == 0 && reset.y == 0);
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

    ControllerSelector hotplug;
    CHECK(!hotplug.SelectInitial({}));
    CHECK(!hotplug.HasIdentity());
    CHECK(hotplug.SelectInitial({a})->stableId == "A");
}

void TestConfig() {
    const auto path = std::filesystem::temp_directory_path() / "wxl-controller-input-config-test.cfg";
    {
        std::ofstream output(path);
        output << "Enabled=false\nDebugLogging=true\nMovementThreshold=500\n"
                  "LeftTriggerThreshold=70\nRightTriggerThreshold=90\nCursorDeadzone=21\n"
                  "CursorSpeed=17\nCursorCurve=5\nSimpleRadial=true\nSwapSticks=true\n"
                  "MovementDeadzone=0.9\n";
    }
    const Config config = LoadConfig(path);
    CHECK(!config.enabled && config.debugLogging);
    CHECK(config.movementThreshold == 127.0F);
    CHECK(config.leftTriggerThreshold == 70.0F && config.rightTriggerThreshold == 90.0F);
    CHECK(config.cursorDeadzone == 21.0F && config.cursorSpeed == 17.0F);
    CHECK(config.cursorCurve == 5.0F && config.simpleRadial && config.swapSticks);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
}

int main() {
    TestFixedButtonMappings();
    TestModifiersAndMouseButtons();
    TestMovementAndHelpers();
    TestPointerCurve();
    TestMouseSuppressionAndNeutralRearmContract();
    TestTouchInputGate();
    TestMouseSourceFilter();
    TestSinkFailurePropagation();
    TestOwnershipCoexistence();
    TestRelativeAccumulator();
    TestSelection();
    TestConfig();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
