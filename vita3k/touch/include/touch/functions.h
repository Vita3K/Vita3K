#pragma once

#include <touch/touch.h>

#include <vector>

struct EmuEnvState;
struct TouchState;

std::vector<SceFVector2> get_touchpad_fingers_pos(const TouchState &state, SceTouchPortType &port);
int handle_touchpad_event(TouchState &state, SDL_GamepadTouchpadEvent &touchpad);
void touch_vsync_update(EmuEnvState &emuenv);
void pinch_modifier(TouchState &state, bool isHold);
void pinch_move(TouchState &state, float velocity);
void pinch_automove(TouchState &state, float velocity);
int handle_touch_event(TouchState &state, SDL_TouchFingerEvent &finger);
int toggle_touchscreen(TouchState &state);
int touch_get(const SceUID thread_id, EmuEnvState &emuenv, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count, bool is_peek);
void touch_set_force_mode(TouchState &state, int port, bool mode);
#ifdef __ANDROID__
void set_rear_touchscreen(TouchState &state, bool is_back);
#endif
