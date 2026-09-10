#ifndef WXL_CONTROLLER_INPUT_API_H
#define WXL_CONTROLLER_INPUT_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WXL_CONTROLLER_INPUT_INTERFACE_NAME "wxl.controller-input"
#define WXL_CONTROLLER_INPUT_API_VERSION 1

#define WXL_CONTROLLER_CAP_DIAGNOSTICS 0x00000001u
#define WXL_CONTROLLER_CAP_BINDING_PERSISTENCE 0x00000002u
#define WXL_CONTROLLER_CAP_BINDING_CAPTURE 0x00000004u
#define WXL_CONTROLLER_CAP_GAME_OUTPUT 0x00000008u
#define WXL_CONTROLLER_CAP_CHARACTER_PROFILES 0x00000010u
#define WXL_CONTROLLER_CAP_EFFECTIVE_ACTION_SLOTS 0x00000020u
#define WXL_CONTROLLER_NO_BUTTON (-1)

#define WXL_CONTROLLER_LAYER_BASE 0u
#define WXL_CONTROLLER_LAYER_LT 1u
#define WXL_CONTROLLER_LAYER_RT 2u
#define WXL_CONTROLLER_LAYER_LT_RT 3u

#define WXL_CONTROLLER_BUTTON_FACE_SOUTH 0
#define WXL_CONTROLLER_BUTTON_FACE_EAST 1
#define WXL_CONTROLLER_BUTTON_FACE_WEST 2
#define WXL_CONTROLLER_BUTTON_FACE_NORTH 3
#define WXL_CONTROLLER_BUTTON_DPAD_UP 4
#define WXL_CONTROLLER_BUTTON_DPAD_RIGHT 5
#define WXL_CONTROLLER_BUTTON_DPAD_DOWN 6
#define WXL_CONTROLLER_BUTTON_DPAD_LEFT 7
#define WXL_CONTROLLER_BUTTON_LEFT_SHOULDER 8
#define WXL_CONTROLLER_BUTTON_RIGHT_SHOULDER 9
#define WXL_CONTROLLER_BUTTON_LEFT_STICK 10
#define WXL_CONTROLLER_BUTTON_RIGHT_STICK 11
#define WXL_CONTROLLER_BUTTON_VIEW 12
#define WXL_CONTROLLER_BUTTON_MENU 13

typedef struct WXL_ControllerInputStateV1 {
    uint32_t structSize;
    uint32_t capabilities;
    int enabled;
    int runtimeReady;
    int connected;
    int inWorld;
    int focused;
    int captureActive;
    int capturedButton;
    float leftX;
    float leftY;
    float rightX;
    float rightY;
    float leftTrigger;
    float rightTrigger;
    int logicalLT;
    int logicalRT;
    uint32_t activeLayer;
    uint32_t pressedButtons;
} WXL_ControllerInputStateV1;

typedef struct WXL_ControllerInputApiV1 {
    uint32_t structSize;
    uint32_t apiVersion;
    int(__cdecl *GetState)(WXL_ControllerInputStateV1 *state);
    int(__cdecl *BeginBindingCapture)(void);
    void(__cdecl *CancelBindingCapture)(void);
} WXL_ControllerInputApiV1;

#ifdef __cplusplus
}
#endif

#endif
