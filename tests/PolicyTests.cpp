#include "bindings/Bindings.hpp"
#include "camera/CameraController.hpp"
#include "config/Config.hpp"
#include "controller/ControllerSelector.hpp"
#include "input/Deadzone.hpp"
#include "input/ModifierController.hpp"
#include "movement/MovementController.hpp"
#include "persistence/BindingStore.hpp"
#include "persistence/Json.hpp"
#include "profiles/ProfileResolver.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

    struct DirectionCase {
        float x;
        float y;
        std::array<bool, 4> expected;
    };
    constexpr DirectionCase directions[]{
        {0, -1, {true, false, false, false}}, {0, 1, {false, true, false, false}},
        {-1, 0, {false, false, true, false}}, {1, 0, {false, false, false, true}},
        {-1, -1, {true, false, true, false}}, {1, -1, {true, false, false, true}},
        {-1, 1, {false, true, true, false}},  {1, 1, {false, true, false, true}},
        {0, 0, {false, false, false, false}},
    };
    for (const auto &direction : directions) {
        movement.Update(direction.x, direction.y);
        CHECK(movement.State() == direction.expected);
    }
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

    CamSink nativeSink;
    CameraController native(nativeSink, CameraPath::Native);
    native.Update(1, 0);
    CHECK(nativeSink.begins == 1 && nativeSink.moves == 1);
    nativeSink.allow = false;
    native.Update(1, 0);
    CHECK(nativeSink.ends == 1 && !native.Active());
}

void TestBindings() {
    auto defaults = BuiltInBindings();
    CHECK(defaults.size() == 38);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::Base, Button::FaceSouth})).slot == 1);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::LT, Button::DPadLeft})).slot == 56);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::RT, Button::DPadLeft})).slot == 68);
    CHECK(std::get<ActionSlot>(defaults.at({Layer::LTRT, Button::DPadLeft})).slot == 60);
    constexpr std::array<Button, 8> primary{Button::FaceSouth, Button::FaceEast, Button::FaceWest,
                                            Button::FaceNorth, Button::DPadUp,   Button::DPadRight,
                                            Button::DPadDown,  Button::DPadLeft};
    constexpr std::array<std::pair<Layer, std::array<unsigned, 8>>, 4> expected{{
        {Layer::Base, {1, 2, 3, 4, 5, 6, 7, 8}},
        {Layer::LT, {49, 50, 51, 52, 53, 54, 55, 56}},
        {Layer::RT, {61, 62, 63, 64, 65, 66, 67, 68}},
        {Layer::LTRT, {9, 10, 11, 12, 57, 58, 59, 60}},
    }};
    for (const auto &[layer, slots] : expected)
        for (std::size_t i = 0; i < primary.size(); ++i)
            CHECK(std::get<ActionSlot>(defaults.at({layer, primary[i]})).slot == slots[i]);
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

    ControllerSelector fallback;
    CHECK(fallback.SelectInitial({a, DeviceInfo{3, -1, "C", "third", "switch"}})->stableId == "A");
    CHECK(fallback.SelectInitial({b})->stableId == "A");
    CHECK(fallback.Disconnect(1));
    CHECK(!fallback.Reconnect({b}));
}

void TestJson() {
    const auto value = json::Parse(R"({"name":"Realm\u0020Name","items":[true,false,null,1.5]})");
    CHECK(value.has_value());
    CHECK(value->AsObject() != nullptr);
    CHECK(*value->AsObject()->at("name").AsString() == "Realm Name");
    CHECK(json::Parse(json::Serialize(*value)).has_value());
    const auto emoji = json::Parse(R"("\uD83C\uDFAE")");
    CHECK(emoji && *emoji->AsString() == "\xF0\x9F\x8E\xAE");
    CHECK(!json::Parse(R"("\uD83C")"));
    CHECK(!json::Parse("{broken"));
    CHECK(!json::Parse(R"({"duplicate":1,"duplicate":2})"));
}

void TestPersistence() {
    const auto root = std::filesystem::temp_directory_path() / "wxl-controller-input-policy-tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const auto configPath = root / "settings.cfg";
    {
        std::ofstream output(configPath);
        output << "MovementDeadzone=2.0\nCameraDeadzone=bad\nUnknown=ignored\n"
                  "InvertCameraY=true\n";
    }
    const Config config = LoadConfig(configPath);
    CHECK(config.movementDeadzone == 0.95F);
    CHECK(config.cameraDeadzone == 0.15F);
    CHECK(config.invertCameraY);
    Config precise = config;
    precise.cameraHorizontalSensitivity = 1.234567F;
    CHECK(SaveConfigAtomic(configPath, precise));
    const Config roundTrip = LoadConfig(configPath);
    CHECK(roundTrip.movementDeadzone == 0.95F);
    CHECK(roundTrip.cameraHorizontalSensitivity == precise.cameraHorizontalSensitivity);

    const auto bindingsPath = root / "bindings.json";
    BindingStore store;
    CHECK(store.Load(bindingsPath) == BindingLoadResult::Missing);
    const BindingKey south{Layer::Base, Button::FaceSouth};
    store.SetGlobal(south, ActionSlot{20});
    store.SetCharacter("Realm|Character", south, ActionSlot{21});
    store.SetGlobal({Layer::Base, Button::Menu}, Unassigned{});
    CHECK(store.SaveAtomic(bindingsPath));

    BindingStore loaded;
    CHECK(loaded.Load(bindingsPath) == BindingLoadResult::Loaded);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at(south)).slot == 20);
    CHECK(std::get<ActionSlot>(loaded.Effective("Realm|Character").at(south)).slot == 21);
    CHECK(
        std::get<KeyBinding>(loaded.Effective(std::nullopt).at({Layer::Base, Button::Menu})).key ==
        "ESCAPE");
    loaded.ResetCharacter("Realm|Character", south);
    CHECK(std::get<ActionSlot>(loaded.Effective("Realm|Character").at(south)).slot == 20);
    loaded.ResetGlobal(south);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at(south)).slot == 1);

    {
        std::ofstream output(bindingsPath, std::ios::trunc);
        output
            << R"({"schemaVersion":1,"future":{"ignored":true},"global":{"bindings":{"Base.FaceSouth":{"type":"ActionSlot","slot":22},"Base.FaceEast":{"type":"ActionSlot","slot":999}}},"characters":{}})";
    }
    CHECK(loaded.Load(bindingsPath) == BindingLoadResult::Loaded);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at(south)).slot == 22);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at({Layer::Base, Button::FaceEast}))
              .slot == 2);

    {
        std::ofstream output(bindingsPath, std::ios::trunc);
        output << R"({"schemaVersion":2,"future":true})";
    }
    CHECK(loaded.Load(bindingsPath) == BindingLoadResult::UnsupportedVersion);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at(south)).slot == 22);
    {
        std::ofstream output(bindingsPath, std::ios::trunc);
        output << "not json";
    }
    CHECK(loaded.Load(bindingsPath) == BindingLoadResult::Invalid);
    CHECK(std::get<ActionSlot>(loaded.Effective(std::nullopt).at(south)).slot == 22);
    std::filesystem::remove_all(root, ignored);
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
    TestJson();
    TestPersistence();
    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
