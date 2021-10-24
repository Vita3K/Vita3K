#pragma once

#include <host/state.h>
#include <touch/touch.h>

int handle_touch_event(SDL_TouchFingerEvent &finger);
int toggle_touchscreen();
int peek_touch(const HostState &host, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count);
