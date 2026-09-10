#include "ExtensionApi.hpp"

#include "config/Config.hpp"
#include "diagnostics/DebugPanel.hpp"
#include "engine/events/Event.hpp"
#include "runtime/FeatureController.hpp"
#include "wxl/PluginApi.h"

#include <Windows.h>

#include <filesystem>
#include <memory>

namespace wxl::controller {
namespace {
std::unique_ptr<FeatureController> g_controller;
const WXL_Api *g_api{};

void __cdecl OnUpdate(void *, const void *) {
    if (g_controller)
        g_controller->OnUpdate();
}
void __cdecl OnWorldEnter(void *, const void *) {
    if (g_controller)
        g_controller->OnWorldEnter();
}
void __cdecl OnWorldLeave(void *, const void *) {
    if (g_controller)
        g_controller->OnWorldLeave("world leave");
}
void __cdecl OnInput(void *, const void *raw) {
    if (!g_controller || !raw)
        return;
    const auto &input = *static_cast<const events::InputArgs *>(raw);
    if (input.message == WM_ACTIVATEAPP)
        g_controller->OnFocus(input.wparam != 0);
}
void __cdecl DrawPanel(void *) {
    if (g_controller && g_api)
        DrawDebugPanel(*g_api, *g_controller);
}

std::filesystem::path ModuleDirectory() {
    HMODULE module{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module);
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

bool ValidApi(const WXL_Api *api) noexcept {
    return api && api->apiVersion == WXL_API_VERSION && api->structSize == sizeof(WXL_Api) &&
           api->Log && api->Subscribe && api->Emit && api->HookAttach && api->HookAttachByName &&
           api->PublishInterface && api->GetInterface && api->UiAddPanel && api->UiIsOpen &&
           api->UiText && api->UiSeparator && api->UiButton && api->UiCheckbox &&
           api->UiSliderFloat && api->UiSliderInt && api->UiColorEdit && api->UiSameLine &&
           api->UiCombo && api->UiCollapsingHeader && api->UiInputText;
}
} // namespace

bool LoadExtension(const WXL_Api *api) noexcept {
    if (!ValidApi(api))
        return false;
    try {
        Config config = LoadConfig(ModuleDirectory() / "wxl-controller-input.cfg");
        auto controller = std::make_unique<FeatureController>(*api, config);
        if (!controller->Initialize())
            return false;
        g_controller = std::move(controller);
        g_api = api;
        api->Subscribe(static_cast<std::uint32_t>(events::Event::OnUpdate), &OnUpdate, nullptr);
        api->Subscribe(static_cast<std::uint32_t>(events::Event::OnWorldEnter), &OnWorldEnter,
                       nullptr);
        api->Subscribe(static_cast<std::uint32_t>(events::Event::OnWorldLeave), &OnWorldLeave,
                       nullptr);
        api->Subscribe(static_cast<std::uint32_t>(events::Event::OnInput), &OnInput, nullptr);
        api->UiAddPanel("Controller Input Diagnostics", &DrawPanel, nullptr);
        api->Log(WXL_LOG_INFO, "controller-input", "loaded v0.1.0 against API v1, client 12340");
        return true;
    } catch (...) {
        api->Log(WXL_LOG_ERROR, "controller-input", "initialization failed safely");
        return false;
    }
}

} // namespace wxl::controller
