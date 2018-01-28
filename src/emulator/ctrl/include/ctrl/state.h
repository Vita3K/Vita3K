#pragma once

#include <SDL_joystick.h>

#include <map>
#include <memory>

struct _SDL_GameController;

typedef std::shared_ptr<_SDL_GameController> GameControllerPtr;
typedef std::map<SDL_JoystickGUID, GameControllerPtr> GameControllerList;

struct CtrlState {
    GameControllerList controllers;
};
