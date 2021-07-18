#pragma once

#include <SDL_haptic.h>
#include <SDL_joystick.h>

#include <map>
#include <memory>

struct _SDL_GameController;

typedef std::shared_ptr<_SDL_GameController> GameControllerPtr;
typedef std::shared_ptr<_SDL_Haptic> HapticPtr;

enum SceCtrlPadInputMode {
    SCE_CTRL_MODE_DIGITAL = 0,
    SCE_CTRL_MODE_ANALOG = 1,
    SCE_CTRL_MODE_ANALOG_WIDE = 2
};

enum SceTouchSamplingState {
    SCE_TOUCH_SAMPLING_STATE_STOP,
    SCE_TOUCH_SAMPLING_STATE_START
};

struct Controller {
    GameControllerPtr controller;
    int port;
};

typedef std::map<SDL_JoystickGUID, Controller> ControllerList;

struct CtrlState {
    ControllerList controllers;
    int controllers_num = 0;
    bool free_ports[4] = { true, true, true, true };
    SceCtrlPadInputMode input_mode = SCE_CTRL_MODE_DIGITAL;
    SceCtrlPadInputMode input_mode_ext = SCE_CTRL_MODE_DIGITAL;
    SceTouchSamplingState touch_mode[2] = { SCE_TOUCH_SAMPLING_STATE_STOP, SCE_TOUCH_SAMPLING_STATE_STOP };
};
