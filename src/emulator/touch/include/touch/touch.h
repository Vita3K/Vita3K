// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <SDL.h>

#include <ctrl/state.h>

struct SDL_TouchFingerEvent;
typedef SDL_TouchFingerEvent SDL_TouchFingerEvent;

int handle_touch_event(SDL_TouchFingerEvent &finger);
int toggle_touchscreen();
int peek_touch(SceFVector2 viewport_pos, SceFVector2 viewport_size, SceIVector2 drawable_size, bool renderer_focused, const CtrlState &ctrl, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count);
