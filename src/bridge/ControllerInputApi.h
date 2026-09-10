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
#define WXL_CONTROLLER_NO_BUTTON (-1)

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
