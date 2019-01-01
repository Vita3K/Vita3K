#pragma once

#include <SDL_haptic.h>
#include <SDL_joystick.h>

#include <map>
#include <memory>

struct _SDL_GameController;

typedef std::shared_ptr<_SDL_GameController> GameControllerPtr;
typedef std::map<SDL_JoystickGUID, GameControllerPtr> GameControllerList;

typedef std::shared_ptr<_SDL_Haptic> HapticPtr;
typedef std::map<SDL_JoystickGUID, HapticPtr> HapticList;

struct CtrlState {
    GameControllerList controllers;
    HapticList haptics;
};
