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

#include <touch/touch.h>

#include <ctrl/state.h>

#include <psp2/touch.h>

#include <SDL_events.h>

#include <cstring>

static SDL_TouchFingerEvent finger_buffer[8];
static uint8_t finger_count = 0;
static long int finger_id_ref;
static auto touchscreen_port = SCE_TOUCH_PORT_FRONT;

static SceTouchData recover_touch_events() {
    SceTouchData touch_data;
    touch_data.timeStamp = timestamp;

    for (uint8_t i = 0; i < finger_count; i++) {
        touch_data.report[i].id = static_cast<uint8_t>(finger_buffer[i].fingerId - finger_id_ref);
        touch_data.report[i].force = static_cast<uint8_t>(1 + finger_buffer[i].pressure * 127);
        touch_data.report[i].x = static_cast<uint16_t>(finger_buffer[i].x * 1920);

        if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
            touch_data.report[i].y = static_cast<uint16_t>(finger_buffer[i].y * 1088);
        } else {
            touch_data.report[i].y = static_cast<uint16_t>(108 + finger_buffer[i].y * 781);
        }
    }

    touch_data.reportNum = finger_count;

    return touch_data;
}

int handle_touch_event(SDL_TouchFingerEvent &finger) {
    switch (finger.type) {
    case SDL_FINGERDOWN: {
        if (finger_count == 0) {
            finger_id_ref = finger.fingerId;
        }

        finger_buffer[finger_count] = finger;
        finger_count++;

        break;
    }
    case SDL_FINGERUP: {
        int finger_index = -1;
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerId == finger_buffer[i].fingerId) {
                finger_index = i;
            }
            if (finger_index > -1) {
                finger_buffer[i] = finger_buffer[i + 1];
            }
        }
        finger_count--;
        break;
    }

    case SDL_FINGERMOTION: {
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerId == finger_buffer[i].fingerId) {
                finger_buffer[i] = finger;
            }
        }
        break;
    }
    }

    return 0;
}

static bool registered_touch() {
    return finger_count > 0;
}

int toggle_touchscreen() {
    if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
        touchscreen_port = SCE_TOUCH_PORT_BACK;
    } else {
        touchscreen_port = SCE_TOUCH_PORT_FRONT;
    }
    return 0;
}

int peek_touch(const SceFVector2 viewport_pos, const SceFVector2 viewport_size, const SceIVector2 drawable_size, const bool renderer_focused, const CtrlState &ctrl, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count) {
    memset(pData, 0, sizeof(*pData));
    pData->timeStamp = timestamp++; // TODO Use the real time and units.

    SceIVector2 touch_pos_window = { 0, 0 };
    const uint32_t buttons = SDL_GetMouseState(&touch_pos_window.x, &touch_pos_window.y);
    const uint32_t mask = (port == SCE_TOUCH_PORT_BACK) ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
    if ((buttons & mask) && renderer_focused) {
        SceIVector2 SDL_window_size = { 0, 0 };
        SDL_Window *const window = SDL_GetMouseFocus();
        SDL_GetWindowSize(window, &SDL_window_size.x, &SDL_window_size.y);

        SceFVector2 scale = { 1, 1 };
        if ((SDL_window_size.x > 0) && (SDL_window_size.y > 0)) {
            scale.x = static_cast<float>(drawable_size.x) / SDL_window_size.x;
            scale.y = static_cast<float>(drawable_size.y) / SDL_window_size.y;
        }

        const SceFVector2 touch_pos_drawable = {
            touch_pos_window.x * scale.x,
            touch_pos_window.y * scale.y
        };

        const SceFVector2 touch_pos_viewport = {
            (touch_pos_drawable.x - viewport_pos.x) / viewport_size.x,
            (touch_pos_drawable.y - viewport_pos.y) / viewport_size.y
        };

        if ((touch_pos_viewport.x >= 0) && (touch_pos_viewport.y >= 0) && (touch_pos_viewport.x < 1) && (touch_pos_viewport.y < 1)) {
            pData->report[pData->reportNum].x = static_cast<uint16_t>(touch_pos_viewport.x * 1920);
            if (port == SCE_TOUCH_PORT_FRONT) {
                pData->report[pData->reportNum].y = static_cast<uint16_t>(touch_pos_viewport.y * 1088);
            } else {
                pData->report[pData->reportNum].y = static_cast<uint16_t>(108 + touch_pos_viewport.y * 781);
            }
            ++pData->reportNum;
        }

        if (!ctrl.touch_mode[port]) {
            pData->reportNum = 0;
        }
    } else if (registered_touch() && port == touchscreen_port) {
        pData[0] = recover_touch_events();
    }

    for (uint8_t i = 1; i < count; i++) {
        memcpy(&pData[i], &pData[0], sizeof(SceTouchData));
    }

    return count;
}
