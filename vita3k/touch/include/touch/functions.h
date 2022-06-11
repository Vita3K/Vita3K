#pragma once

#include <host/state.h>
#include <touch/touch.h>

void touch_vsync_update(const HostState &host);
int handle_touch_event(SDL_TouchFingerEvent &finger);
int toggle_touchscreen();
int touch_get(const SceUID thread_id, HostState &host, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count, bool is_peek);
