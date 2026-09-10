#include "bridge/LuaBridge.hpp"

#include "bindings/Bindings.hpp"
#include "bridge/ControllerInputApi.h"
#include "input/Deadzone.hpp"
#include "runtime/FeatureController.hpp"
#include "wxl/PluginApi.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace wxl::controller {
namespace {
constexpr std::uintptr_t kGetContext = 0x00817DB0;
constexpr std::uintptr_t kRegisterFunction = 0x00817F90;
constexpr std::uintptr_t kExecute = 0x00819210;
constexpr std::uintptr_t kLuaGetTop = 0x0084DBD0;
constexpr std::uintptr_t kLuaIsNumber = 0x0084DF20;
constexpr std::uintptr_t kLuaIsString = 0x0084DF60;
constexpr std::uintptr_t kLuaToNumber = 0x0084E030;
constexpr std::uintptr_t kLuaToString = 0x0084E0E0;
constexpr std::uintptr_t kLuaPushNumber = 0x0084E2A0;
constexpr std::uintptr_t kLuaPushString = 0x0084E350;
constexpr std::uintptr_t kLuaPushNil = 0x0084E280;
constexpr std::uintptr_t kLuaPushBoolean = 0x0084E4D0;

constexpr std::uint32_t kCapabilitiesBase = WXL_CONTROLLER_CAP_DIAGNOSTICS |
                                             WXL_CONTROLLER_CAP_BINDING_PERSISTENCE |
                                             WXL_CONTROLLER_CAP_BINDING_CAPTURE |
                                             WXL_CONTROLLER_CAP_GAME_OUTPUT |
                                             WXL_CONTROLLER_CAP_CHARACTER_PROFILES;
constexpr std::uint32_t kCapabilityEffectiveActionSlots =
    WXL_CONTROLLER_CAP_EFFECTIVE_ACTION_SLOTS;

using LuaFunction = int(__cdecl *)(void *);
using ValidateFunction = void(__cdecl *)(std::uintptr_t);
using GetContextFunction = void *(__cdecl *)();
using RegisterFunction = void(__cdecl *)(const char *, LuaFunction);
using ExecuteFunction = void(__cdecl *)(const char *, void *);

template <typename T> T Native(std::uintptr_t address) noexcept {
    return reinterpret_cast<T>(address);
}

FeatureController *g_controller{};
const WXL_Api *g_api{};
ValidateFunction g_validateOriginal{};
void *g_context{};
bool g_validatorAttached{};
bool g_ready{};
const char *g_reason{"Lua validator hook unavailable"};
std::chrono::steady_clock::time_point g_lastObservation{};

int ArgCount(void *state) noexcept {
    return Native<int(__cdecl *)(void *)>(kLuaGetTop)(state);
}
bool IsNumber(void *state, int index) noexcept {
    return Native<int(__cdecl *)(void *, int)>(kLuaIsNumber)(state, index) != 0;
}
bool IsString(void *state, int index) noexcept {
    return Native<int(__cdecl *)(void *, int)>(kLuaIsString)(state, index) != 0;
}
double ToNumber(void *state, int index) noexcept {
    return Native<double(__cdecl *)(void *, int)>(kLuaToNumber)(state, index);
}
const char *ToString(void *state, int index) noexcept {
    return Native<const char *(__cdecl *)(void *, int, std::size_t *)>(kLuaToString)(state, index,
                                                                                    nullptr);
}
void PushNumber(void *state, double value) noexcept {
    Native<void(__cdecl *)(void *, double)>(kLuaPushNumber)(state, value);
}
void PushString(void *state, const char *value) noexcept {
    Native<void(__cdecl *)(void *, const char *)>(kLuaPushString)(state, value ? value : "");
}
void PushBoolean(void *state, bool value) noexcept {
    Native<void(__cdecl *)(void *, int)>(kLuaPushBoolean)(state, value ? 1 : 0);
}
void PushNil(void *state) noexcept {
    Native<void(__cdecl *)(void *)>(kLuaPushNil)(state);
}

const char *ErrorName(RuntimeResult result) noexcept {
    switch (result) {
    case RuntimeResult::Ok:
        return nullptr;
    case RuntimeResult::InvalidArgument:
        return "INVALID_ARGUMENT";
    case RuntimeResult::Unsupported:
        return "UNSUPPORTED";
    case RuntimeResult::NoCharacter:
        return "NO_CHARACTER";
    case RuntimeResult::CaptureActive:
        return "CAPTURE_ACTIVE";
    case RuntimeResult::SaveFailed:
        return "SAVE_FAILED";
    }
    return "UNAVAILABLE";
}

int PushResult(void *state, RuntimeResult result) noexcept {
    if (result == RuntimeResult::Ok) {
        PushBoolean(state, true);
        return 1;
    }
    PushNil(state);
    PushString(state, ErrorName(result));
    return 2;
}

std::optional<Layer> ParseLayer(double value) noexcept {
    if (!std::isfinite(value) || value < 0 || value > 3 || std::floor(value) != value)
        return std::nullopt;
    return static_cast<Layer>(static_cast<unsigned>(value));
}
std::optional<Button> ParseButton(double value) noexcept {
    if (!std::isfinite(value) || value < 0 || value >= static_cast<double>(Button::Count) ||
        std::floor(value) != value)
        return std::nullopt;
    return static_cast<Button>(static_cast<unsigned>(value));
}
std::optional<bool> ParseScope(const char *scope) noexcept {
    if (!scope)
        return std::nullopt;
    if (std::strcmp(scope, "global") == 0)
        return false;
    if (std::strcmp(scope, "character") == 0)
        return true;
    return std::nullopt;
}

std::vector<std::string> ParseModifiers(const char *text) {
    std::vector<std::string> result;
    if (!text || !*text)
        return result;
    std::string_view remaining{text};
    while (!remaining.empty()) {
        const std::size_t comma = remaining.find(',');
        const std::string_view part = remaining.substr(0, comma);
        if (part.empty())
            return {"INVALID"};
        result.emplace_back(part);
        if (comma == std::string_view::npos)
            break;
        remaining.remove_prefix(comma + 1);
    }
    return result;
}

std::optional<Binding> ParseBinding(void *state) {
    if (ArgCount(state) < 6 || !IsString(state, 4) || !IsString(state, 5) ||
        !IsString(state, 6))
        return std::nullopt;
    const std::string type = ToString(state, 4);
    const std::string value = ToString(state, 5);
    const char *modifiers = ToString(state, 6);
    if (type == "Unassigned")
        return Unassigned{};
    if (type == "ActionSlot") {
        if (value.empty())
            return std::nullopt;
        char *end{};
        const unsigned long slot = std::strtoul(value.c_str(), &end, 10);
        if (!end || *end != '\0' || slot > 120)
            return std::nullopt;
        return ActionSlot{static_cast<unsigned>(slot)};
    }
    if (type == "WoWBinding")
        return WowBinding{value};
    if (type == "KeyBinding")
        return KeyBinding{value, ParseModifiers(modifiers)};
    return std::nullopt;
}

const char *BindingSourceName(BindingSource source) noexcept {
    switch (source) {
    case BindingSource::BuiltIn:
        return "builtin";
    case BindingSource::Global:
        return "global";
    case BindingSource::Character:
        return "character";
    }
    return "builtin";
}

int __cdecl LuaReady(void *state) {
    g_ready = true;
    g_reason = "";
    PushBoolean(state, true);
    return 1;
}

int __cdecl LuaGetCapabilities(void *state) {
    std::uint32_t capabilities = kCapabilitiesBase;
    if (g_controller && g_controller->EffectiveActionSlotsValid())
        capabilities |= kCapabilityEffectiveActionSlots;
    PushNumber(state, capabilities);
    return 1;
}

int __cdecl LuaGetState(void *state) {
    if (!g_controller) {
        PushNil(state);
        PushString(state, "UNAVAILABLE");
        return 2;
    }
    const Snapshot &raw = g_controller->CurrentSnapshot();
    const Config &config = g_controller->Settings();
    const Vec2 left = ApplyRadialDeadzone(raw.leftX, raw.leftY, config.movementDeadzone);
    const Vec2 right = ApplyRadialDeadzone(raw.rightX, raw.rightY, config.cameraDeadzone);
    std::uint32_t pressed{};
    for (std::size_t i = 0; i < raw.buttons.size(); ++i)
        if (raw.buttons[i])
            pressed |= 1u << i;
    const DeviceInfo *device = g_controller->CurrentDevice();
    const bool actionValid = g_controller->EffectiveActionSlotsValid();
    const bool degraded = !g_ready || !g_controller->TextEntryKnown() || !actionValid;
    const char *degradedReason = !g_ready                         ? g_reason
                                 : !g_controller->TextEntryKnown() ? "TEXT_CONTEXT_UNAVAILABLE"
                                 : !actionValid                    ? "ACTION_CONTEXT_UNAVAILABLE"
                                                                   : "";
    PushBoolean(state, config.enabled);
    PushBoolean(state, g_controller->RuntimeReady());
    PushBoolean(state, g_controller->Connected());
    PushBoolean(state, g_controller->InWorld());
    PushBoolean(state, g_controller->Focused());
    PushBoolean(state, g_controller->OverlayOpen());
    PushBoolean(state, g_controller->TextEntryKnown());
    PushBoolean(state, g_controller->TextEntryActive());
    PushBoolean(state, g_controller->CaptureActive());
    const auto captured = g_controller->CapturedButton();
    PushNumber(state, captured ? static_cast<unsigned>(*captured) : -1.0);
    PushString(state, device ? device->name.c_str() : "");
    PushString(state, device ? device->family.c_str() : "");
    PushString(state, device ? device->stableId.c_str() : "");
    PushNumber(state, raw.leftX);
    PushNumber(state, raw.leftY);
    PushNumber(state, raw.rightX);
    PushNumber(state, raw.rightY);
    PushNumber(state, raw.leftTrigger);
    PushNumber(state, raw.rightTrigger);
    PushNumber(state, left.x);
    PushNumber(state, left.y);
    PushNumber(state, right.x);
    PushNumber(state, right.y);
    PushBoolean(state, g_controller->LeftModifierActive());
    PushBoolean(state, g_controller->RightModifierActive());
    PushNumber(state, static_cast<unsigned>(g_controller->CurrentLayer()));
    PushNumber(state, pressed);
    PushString(state, g_controller->CancellationReason());
    PushString(state, g_controller->Realm().c_str());
    PushString(state, g_controller->Character().c_str());
    PushBoolean(state, g_controller->HasCharacter());
    PushBoolean(state, actionValid);
    PushNumber(state, g_controller->ActionPage());
    for (unsigned slot : g_controller->EffectiveActionSlots())
        PushNumber(state, slot);
    PushBoolean(state, degraded);
    PushString(state, degradedReason);
    PushNumber(state, config.movementDeadzone);
    PushNumber(state, config.cameraDeadzone);
    PushNumber(state, config.cameraHorizontalSensitivity);
    PushNumber(state, config.cameraVerticalSensitivity);
    PushBoolean(state, config.invertCameraY);
    PushNumber(state, config.triggerActivateThreshold);
    PushNumber(state, config.triggerReleaseThreshold);
    return 54;
}

int __cdecl LuaGetBinding(void *state) {
    if (!g_controller || ArgCount(state) < 3 || !IsString(state, 1) || !IsNumber(state, 2) ||
        !IsNumber(state, 3)) {
        PushNil(state);
        PushString(state, "INVALID_ARGUMENT");
        return 2;
    }
    const auto scope = ParseScope(ToString(state, 1));
    const auto layer = ParseLayer(ToNumber(state, 2));
    const auto button = ParseButton(ToNumber(state, 3));
    if (!scope || !layer || !button || (*button == Button::Menu && *layer != Layer::Base)) {
        PushNil(state);
        PushString(state, "INVALID_ARGUMENT");
        return 2;
    }
    if (*scope && !g_controller->HasCharacter()) {
        PushNil(state);
        PushString(state, "NO_CHARACTER");
        return 2;
    }
    const auto resolved = g_controller->GetBinding(*scope, *layer, *button);
    if (!resolved) {
        PushNil(state);
        PushString(state, "INVALID_ARGUMENT");
        return 2;
    }
    std::string value;
    std::string modifiers;
    const char *type = "Unassigned";
    std::visit(
        [&](const auto &binding) {
            using T = std::decay_t<decltype(binding)>;
            if constexpr (std::is_same_v<T, ActionSlot>) {
                type = "ActionSlot";
                value = std::to_string(binding.slot);
            } else if constexpr (std::is_same_v<T, WowBinding>) {
                type = "WoWBinding";
                value = binding.command;
            } else if constexpr (std::is_same_v<T, KeyBinding>) {
                type = "KeyBinding";
                value = binding.key;
                for (const std::string &modifier : binding.modifiers) {
                    if (!modifiers.empty())
                        modifiers.push_back(',');
                    modifiers += modifier;
                }
            }
        },
        resolved->binding);
    PushString(state, type);
    PushString(state, value.c_str());
    PushString(state, modifiers.c_str());
    PushString(state, BindingSourceName(resolved->source));
    return 4;
}

int __cdecl LuaSetBinding(void *state) {
    if (!g_controller || ArgCount(state) < 6 || !IsString(state, 1) || !IsNumber(state, 2) ||
        !IsNumber(state, 3))
        return PushResult(state, RuntimeResult::InvalidArgument);
    const auto scope = ParseScope(ToString(state, 1));
    const auto layer = ParseLayer(ToNumber(state, 2));
    const auto button = ParseButton(ToNumber(state, 3));
    const auto binding = ParseBinding(state);
    if (!scope || !layer || !button || !binding)
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->SetBinding(*scope, *layer, *button, *binding));
}

int __cdecl LuaResetBinding(void *state) {
    if (!g_controller || ArgCount(state) < 3 || !IsString(state, 1) || !IsNumber(state, 2) ||
        !IsNumber(state, 3))
        return PushResult(state, RuntimeResult::InvalidArgument);
    const auto scope = ParseScope(ToString(state, 1));
    const auto layer = ParseLayer(ToNumber(state, 2));
    const auto button = ParseButton(ToNumber(state, 3));
    if (!scope || !layer || !button)
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->ResetBinding(*scope, *layer, *button));
}

int __cdecl LuaResetLayer(void *state) {
    if (!g_controller || ArgCount(state) < 2 || !IsString(state, 1) || !IsNumber(state, 2))
        return PushResult(state, RuntimeResult::InvalidArgument);
    const auto scope = ParseScope(ToString(state, 1));
    const auto layer = ParseLayer(ToNumber(state, 2));
    if (!scope || !layer)
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->ResetLayer(*scope, *layer));
}

int __cdecl LuaResetProfile(void *state) {
    if (!g_controller || ArgCount(state) < 1 || !IsString(state, 1))
        return PushResult(state, RuntimeResult::InvalidArgument);
    const auto scope = ParseScope(ToString(state, 1));
    if (!scope)
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->ResetProfile(*scope));
}

int __cdecl LuaSetCharacter(void *state) {
    if (!g_controller || ArgCount(state) < 2 || !IsString(state, 1) || !IsString(state, 2))
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->SetCharacter(ToString(state, 1), ToString(state, 2)));
}
int __cdecl LuaClearCharacter(void *state) {
    return PushResult(state, g_controller ? g_controller->ClearCharacter()
                                          : RuntimeResult::InvalidArgument);
}
int __cdecl LuaSetEnabled(void *state) {
    if (!g_controller || ArgCount(state) < 1 || !IsNumber(state, 1))
        return PushResult(state, RuntimeResult::InvalidArgument);
    return PushResult(state, g_controller->SetEnabled(ToNumber(state, 1) != 0));
}

int __cdecl LuaGetOption(void *state) {
    if (!g_controller || ArgCount(state) < 1 || !IsString(state, 1)) {
        PushNil(state);
        PushString(state, "INVALID_ARGUMENT");
        return 2;
    }
    const std::string name = ToString(state, 1);
    if (const auto value = g_controller->GetBooleanOption(name)) {
        PushBoolean(state, *value);
        return 1;
    }
    if (const auto value = g_controller->GetNumericOption(name)) {
        PushNumber(state, *value);
        return 1;
    }
    PushNil(state);
    PushString(state, "UNSUPPORTED");
    return 2;
}

int __cdecl LuaSetOption(void *state) {
    if (!g_controller || ArgCount(state) < 3 || !IsString(state, 1) || !IsNumber(state, 2) ||
        !IsNumber(state, 3))
        return PushResult(state, RuntimeResult::InvalidArgument);
    const bool isBoolean = ToNumber(state, 3) != 0;
    return PushResult(state, g_controller->SetOption(ToString(state, 1), ToNumber(state, 2),
                                                     ToNumber(state, 2) != 0, isBoolean));
}

int __cdecl LuaBeginCapture(void *state) {
    if (!g_controller || !g_controller->BeginBindingCapture()) {
        PushNil(state);
        PushString(state, !g_controller                  ? "UNAVAILABLE"
                          : !g_controller->InWorld()    ? "NOT_IN_WORLD"
                          : !g_controller->Focused()    ? "NOT_FOCUSED"
                                                       : "UNAVAILABLE");
        return 2;
    }
    PushBoolean(state, true);
    return 1;
}
int __cdecl LuaCancelCapture(void *state) {
    if (g_controller)
        g_controller->CancelBindingCapture();
    PushBoolean(state, true);
    return 1;
}

int __cdecl LuaObserve(void *state) {
    if (!g_controller || ArgCount(state) < 16)
        return 0;
    for (int i = 1; i <= 16; ++i)
        if (!IsNumber(state, i))
            return 0;
    g_controller->SetTextEntryState(ToNumber(state, 1) != 0, ToNumber(state, 2) != 0);
    std::array<unsigned, 12> slots{};
    bool valid = ToNumber(state, 3) != 0;
    const double pageValue = ToNumber(state, 4);
    if (!std::isfinite(pageValue) || pageValue < 0 || pageValue > 10)
        valid = false;
    for (int i = 0; i < 12; ++i) {
        const double value = ToNumber(state, i + 5);
        if (!std::isfinite(value) || value < 1 || value > 120 || std::floor(value) != value)
            valid = false;
        slots[static_cast<std::size_t>(i)] = static_cast<unsigned>(value);
    }
    g_controller->SetEffectiveActionSlots(slots, valid, static_cast<unsigned>(pageValue));
    g_lastObservation = std::chrono::steady_clock::now();
    return 0;
}

template <LuaFunction Function> int __cdecl Guarded(void *state) noexcept {
    try {
        return Function(state);
    } catch (...) {
        PushNil(state);
        PushString(state, "UNAVAILABLE");
        return 2;
    }
}

constexpr std::array<LuaFunction, 14> kOwnFunctions{
    &Guarded<&LuaReady>,          &Guarded<&LuaGetCapabilities>,
    &Guarded<&LuaGetState>,       &Guarded<&LuaGetBinding>,
    &Guarded<&LuaSetBinding>,     &Guarded<&LuaResetBinding>,
    &Guarded<&LuaResetLayer>,     &Guarded<&LuaResetProfile>,
    &Guarded<&LuaSetCharacter>,   &Guarded<&LuaClearCharacter>,
    &Guarded<&LuaSetEnabled>,     &Guarded<&LuaGetOption>,
    &Guarded<&LuaSetOption>,      &Guarded<&LuaBeginCapture>};

bool IsOwnFunction(std::uintptr_t function) noexcept {
    if (function == reinterpret_cast<std::uintptr_t>(&Guarded<&LuaCancelCapture>) ||
        function == reinterpret_cast<std::uintptr_t>(&Guarded<&LuaObserve>))
        return true;
    for (LuaFunction own : kOwnFunctions)
        if (function == reinterpret_cast<std::uintptr_t>(own))
            return true;
    return false;
}

void __cdecl ValidateDetour(std::uintptr_t function) {
    if (!IsOwnFunction(function) && g_validateOriginal)
        g_validateOriginal(function);
}

struct Registration {
    const char *name;
    LuaFunction function;
};
constexpr Registration kRegistrations[]{
    {"_WXLControllerReady", &Guarded<&LuaReady>},
    {"_WXLControllerGetCapabilities", &Guarded<&LuaGetCapabilities>},
    {"_WXLControllerGetState", &Guarded<&LuaGetState>},
    {"_WXLControllerGetBinding", &Guarded<&LuaGetBinding>},
    {"_WXLControllerSetBinding", &Guarded<&LuaSetBinding>},
    {"_WXLControllerResetBinding", &Guarded<&LuaResetBinding>},
    {"_WXLControllerResetLayer", &Guarded<&LuaResetLayer>},
    {"_WXLControllerResetProfile", &Guarded<&LuaResetProfile>},
    {"_WXLControllerSetCharacter", &Guarded<&LuaSetCharacter>},
    {"_WXLControllerClearCharacter", &Guarded<&LuaClearCharacter>},
    {"_WXLControllerSetEnabled", &Guarded<&LuaSetEnabled>},
    {"_WXLControllerGetOption", &Guarded<&LuaGetOption>},
    {"_WXLControllerSetOption", &Guarded<&LuaSetOption>},
    {"_WXLControllerBeginCapture", &Guarded<&LuaBeginCapture>},
    {"_WXLControllerCancelCapture", &Guarded<&LuaCancelCapture>},
    {"_WXLControllerObserve", &Guarded<&LuaObserve>},
};

constexpr const char *kWrapper = R"lua(
do
 local N={ready=_WXLControllerReady,caps=_WXLControllerGetCapabilities,state=_WXLControllerGetState,get=_WXLControllerGetBinding,set=_WXLControllerSetBinding,reset=_WXLControllerResetBinding,resetLayer=_WXLControllerResetLayer,resetProfile=_WXLControllerResetProfile,setCharacter=_WXLControllerSetCharacter,clearCharacter=_WXLControllerClearCharacter,setEnabled=_WXLControllerSetEnabled,getOption=_WXLControllerGetOption,setOption=_WXLControllerSetOption,beginCapture=_WXLControllerBeginCapture,cancelCapture=_WXLControllerCancelCapture,observe=_WXLControllerObserve}
 local layers={Base=0,LT=1,RT=2,["LT+RT"]=3}
 local layerNames={[0]="Base",[1]="LT",[2]="RT",[3]="LT+RT"}
 local buttons={FaceSouth=0,FaceEast=1,FaceWest=2,FaceNorth=3,DPadUp=4,DPadRight=5,DPadDown=6,DPadLeft=7,LeftShoulder=8,RightShoulder=9,LeftStick=10,RightStick=11,View=12,Menu=13}
 local buttonNames={[0]="FaceSouth",[1]="FaceEast",[2]="FaceWest",[3]="FaceNorth",[4]="DPadUp",[5]="DPadRight",[6]="DPadDown",[7]="DPadLeft",[8]="LeftShoulder",[9]="RightShoulder",[10]="LeftStick",[11]="RightStick",[12]="View",[13]="Menu"}
 local function lid(v) return type(v)=="string" and layers[v] or v end
 local function bid(v) return type(v)=="string" and buttons[v] or v end
 local function split(v) local r={} if v and v~="" then for x in string.gmatch(v,"[^,]+") do table.insert(r,x) end end return r end
 local function labels(f) f=string.lower(f or "") if string.find(f,"ps") or string.find(f,"playstation") then return {FaceSouth="Cross",FaceEast="Circle",FaceWest="Square",FaceNorth="Triangle",LeftShoulder="L1",RightShoulder="R1",View="Create/Share",Menu="Options"} elseif string.find(f,"xbox") then return {FaceSouth="A",FaceEast="B",FaceWest="X",FaceNorth="Y",LeftShoulder="LB",RightShoulder="RB",View="View",Menu="Menu"} else return {} end end
 local A={contractVersion=1}
 function A.GetCapabilities() local m=N.caps() return {diagnostics=math.floor(m/1)%2==1,bindingPersistence=math.floor(m/2)%2==1,bindingCapture=math.floor(m/4)%2==1,gameOutput=math.floor(m/8)%2==1,characterProfiles=math.floor(m/16)%2==1,effectiveActionSlots=math.floor(m/32)%2==1} end
 function A.GetState()
  local v={N.state()} if not v[1] then return nil,v[2] end
  local pressed={} for i=0,13 do pressed[buttonNames[i]]=math.floor(v[27]/(2^i))%2==1 end
  local slots={} for i=1,12 do slots[i]=v[33+i] end
  return {extensionVersion="0.1.0",enabled=v[1],runtimeReady=v[2],connected=v[3],inWorld=v[4],focused=v[5],overlayOpen=v[6],textEntryKnown=v[7],textEntryActive=v[8],captureActive=v[9],capturedButton=buttonNames[v[10]],device={controllerIndex=1,name=v[11],family=v[12],stableId=v[13],labels=labels(v[12])},axes={raw={leftX=v[14],leftY=v[15],rightX=v[16],rightY=v[17]},processed={leftX=v[20],leftY=v[21],rightX=v[22],rightY=v[23]},leftTrigger=v[18],rightTrigger=v[19]},logicalLT=v[24],logicalRT=v[25],activeLayer=layerNames[v[26]],pressed=pressed,cancellationReason=v[28],profile={realm=v[29],character=v[30],active=v[31] and "character" or "global"},actionPage=v[33],effectiveActionSlots=v[32] and slots or nil,degraded=v[46],degradedReason=v[47]~="" and v[47] or nil,settings={MovementDeadzone=v[48],CameraDeadzone=v[49],CameraHorizontalSensitivity=v[50],CameraVerticalSensitivity=v[51],InvertCameraY=v[52],TriggerActivateThreshold=v[53],TriggerReleaseThreshold=v[54]}}
 end
 function A.GetBinding(scope,layer,button) local t,v,m,s=N.get(scope,lid(layer),bid(button)) if not t then return nil,v end local b={type=t} if t=="ActionSlot" then b.slot=tonumber(v) elseif t=="WoWBinding" then b.command=v elseif t=="KeyBinding" then b.key=v;b.modifiers=split(m) end return b,s end
 function A.SetBinding(scope,layer,button,b) if type(b)~="table" or type(b.type)~="string" then return nil,"INVALID_ARGUMENT" end local v="" local m="" if b.type=="ActionSlot" then v=tostring(b.slot or "") elseif b.type=="WoWBinding" then v=b.command or "" elseif b.type=="KeyBinding" then v=b.key or "" if type(b.modifiers)=="table" then m=table.concat(b.modifiers,",") end elseif b.type~="Unassigned" then return nil,"INVALID_ARGUMENT" end return N.set(scope,lid(layer),bid(button),b.type,v,m) end
 function A.ResetBinding(scope,layer,button) return N.reset(scope,lid(layer),bid(button)) end
 function A.ResetLayer(scope,layer) return N.resetLayer(scope,lid(layer)) end
 function A.ResetProfile(scope) return N.resetProfile(scope) end
 function A.SetCharacter(realm,character) return N.setCharacter(realm,character) end
 function A.ClearCharacter() return N.clearCharacter() end
 function A.SetEnabled(enabled) if type(enabled)~="boolean" then return nil,"INVALID_ARGUMENT" end return N.setEnabled(enabled and 1 or 0) end
 function A.GetOption(name) return N.getOption(name) end
 function A.SetOption(name,value) if type(value)=="boolean" then return N.setOption(name,value and 1 or 0,1) elseif type(value)=="number" then return N.setOption(name,value,0) end return nil,"INVALID_ARGUMENT" end
 function A.BeginBindingCapture() return N.beginCapture() end
 function A.CancelBindingCapture() return N.cancelCapture() end
 _G.WXLControllerInput=A
 local function observe()
  local known=type(GetCurrentKeyBoardFocus)=="function" local focused=known and GetCurrentKeyBoardFocus()~=nil
  local valid=type(ActionButton_GetPagedID)=="function" local page=type(GetActionBarPage)=="function" and (GetActionBarPage() or 0) or 0
  if (VehicleMenuBar and VehicleMenuBar:IsShown()) or (PossessBarFrame and PossessBarFrame:IsShown()) then valid=false end
  local s={} for i=1,12 do local b=_G["ActionButton"..i] local id=valid and b and ActionButton_GetPagedID(b) or 0 s[i]=tonumber(id) or 0 if s[i]<1 or s[i]>120 then valid=false end end
  N.observe(known and 1 or 0,focused and 1 or 0,valid and 1 or 0,page,s[1],s[2],s[3],s[4],s[5],s[6],s[7],s[8],s[9],s[10],s[11],s[12])
 end
 local f=CreateFrame("Frame") f:SetScript("OnUpdate",observe) observe() N.ready()
end
)lua";
} // namespace

bool LuaBridge::AttachValidator(const WXL_Api &api) noexcept {
    g_api = &api;
    g_validatorAttached = api.HookAttachByName(
                              "Lua.ValidateFunctionPointer", reinterpret_cast<void *>(&ValidateDetour),
                              reinterpret_cast<void **>(&g_validateOriginal),
                              WXL_HOOK_DEFAULT_PRIORITY) != 0;
    g_reason = g_validatorAttached ? "FrameScript context unavailable"
                                   : "Lua validator hook unavailable";
    return g_validatorAttached;
}

void LuaBridge::Bind(FeatureController *controller) noexcept {
    g_controller = controller;
}

void LuaBridge::Tick() noexcept {
    if (!g_validatorAttached || !g_controller)
        return;
    void *const context = Native<GetContextFunction>(kGetContext)();
    if (!context) {
        if (g_context) {
            g_context = nullptr;
            g_ready = false;
            g_controller->SetTextEntryState(false, false);
            g_controller->SetEffectiveActionSlots({}, false, 0);
        }
        g_reason = "FrameScript context unavailable";
        return;
    }
    if (context == g_context)
    {
        if (g_ready && g_lastObservation != std::chrono::steady_clock::time_point{} &&
            std::chrono::steady_clock::now() - g_lastObservation > std::chrono::milliseconds(500)) {
            g_controller->SetTextEntryState(false, false);
            g_controller->SetEffectiveActionSlots({}, false, 0);
            g_reason = "Lua context observer stale";
        }
        return;
    }
    g_context = context;
    g_ready = false;
    g_lastObservation = {};
    g_reason = "Lua wrapper handshake failed";
    g_controller->SetTextEntryState(false, false);
    g_controller->SetEffectiveActionSlots({}, false, 0);
    for (const Registration &registration : kRegistrations)
        Native<RegisterFunction>(kRegisterFunction)(registration.name, registration.function);
    Native<ExecuteFunction>(kExecute)(kWrapper, context);
    if (g_ready && g_api)
        g_api->Log(WXL_LOG_INFO, "controller-input", "registered controller Lua contract v1");
    else if (g_api)
        g_api->Log(WXL_LOG_ERROR, "controller-input", "controller Lua wrapper handshake failed");
}

bool LuaBridge::Ready() noexcept {
    return g_ready;
}

const char *LuaBridge::DegradedReason() noexcept {
    return g_reason;
}

} // namespace wxl::controller
