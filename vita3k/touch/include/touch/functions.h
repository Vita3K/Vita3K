#pragma once

#include <emuenv/state.h>
#include <touch/touch.h>

void touch_vsync_update(const EmuEnvState &emuenv);
int handle_touch_event(SDL_TouchFingerEvent &finger);
int toggle_touchscreen();
int touch_get(const SceUID thread_id, EmuEnvState &emuenv, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count, bool is_peek);
void touch_set_force_mode(int port, bool mode);
