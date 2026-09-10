#include "bridge/ControllerBridge.hpp"

#include "bridge/ControllerInputApi.h"
#include "runtime/FeatureController.hpp"

namespace wxl::controller {
namespace {
FeatureController *g_controller{};

int __cdecl GetState(WXL_ControllerInputStateV1 *state) {
    if (!state || state->structSize != sizeof(WXL_ControllerInputStateV1) || !g_controller)
        return 0;
    const Snapshot &snapshot = g_controller->CurrentSnapshot();
    state->capabilities = WXL_CONTROLLER_CAP_DIAGNOSTICS | WXL_CONTROLLER_CAP_BINDING_PERSISTENCE |
                          WXL_CONTROLLER_CAP_BINDING_CAPTURE | WXL_CONTROLLER_CAP_GAME_OUTPUT;
    state->enabled = g_controller->Settings().enabled ? 1 : 0;
    state->runtimeReady = 1;
    state->connected = g_controller->Connected() ? 1 : 0;
    state->inWorld = g_controller->InWorld() ? 1 : 0;
    state->focused = g_controller->Focused() ? 1 : 0;
    state->captureActive = g_controller->CaptureActive() ? 1 : 0;
    const auto captured = g_controller->CapturedButton();
    state->capturedButton = captured ? static_cast<int>(*captured) : WXL_CONTROLLER_NO_BUTTON;
    state->leftX = snapshot.leftX;
    state->leftY = snapshot.leftY;
    state->rightX = snapshot.rightX;
    state->rightY = snapshot.rightY;
    state->leftTrigger = snapshot.leftTrigger;
    state->rightTrigger = snapshot.rightTrigger;
    state->logicalLT = g_controller->LeftModifierActive() ? 1 : 0;
    state->logicalRT = g_controller->RightModifierActive() ? 1 : 0;
    state->activeLayer = static_cast<uint32_t>(g_controller->CurrentLayer());
    state->pressedButtons = 0;
    for (std::size_t i = 0; i < snapshot.buttons.size(); ++i)
        if (snapshot.buttons[i])
            state->pressedButtons |= (1u << i);
    return 1;
}

int __cdecl BeginBindingCapture() {
    return g_controller && g_controller->BeginBindingCapture() ? 1 : 0;
}

void __cdecl CancelBindingCapture() {
    if (g_controller)
        g_controller->CancelBindingCapture();
}

WXL_ControllerInputApiV1 g_interface{sizeof(WXL_ControllerInputApiV1),
                                     WXL_CONTROLLER_INPUT_API_VERSION, &GetState,
                                     &BeginBindingCapture, &CancelBindingCapture};
} // namespace

void ControllerBridge::Bind(FeatureController *controller) noexcept {
    g_controller = controller;
}
WXL_ControllerInputApiV1 *ControllerBridge::Interface() noexcept {
    return &g_interface;
}

} // namespace wxl::controller
