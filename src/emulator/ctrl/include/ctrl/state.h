#pragma once

#include <SDL_haptic.h>
#include <SDL_joystick.h>

#include <map>
#include <memory>

struct _SDL_GameController;

typedef std::shared_ptr<_SDL_GameController> GameControllerPtr;
typedef std::shared_ptr<_SDL_Haptic> HapticPtr;

struct Controller {
    GameControllerPtr controller;
    HapticPtr haptic;
    int port;
};

typedef std::map<SDL_JoystickGUID, Controller> ControllerList;

struct CtrlState {
    ControllerList controllers;
    int controllers_num;
    bool free_ports[4] = { true, true, true, true };
};
