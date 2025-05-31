#pragma once

#include <touch/touch.h>

#include <vector>

std::vector<SceFVector2> get_touchpad_fingers_pos(SceTouchPortType &port);
int handle_touchpad_event(SDL_GamepadTouchpadEvent &touchpad);
void touch_vsync_update(const EmuEnvState &emuenv);
int handle_touch_event(SDL_TouchFingerEvent &finger);
int toggle_touchscreen();
int touch_get(const SceUID thread_id, EmuEnvState &emuenv, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count, bool is_peek);
void touch_set_force_mode(int port, bool mode);
