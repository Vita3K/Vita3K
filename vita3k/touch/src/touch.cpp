// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <display/functions.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <touch/functions.h>
#include <touch/state.h>
#include <touch/touch.h>

#include <SDL3/SDL_events.h>

#include <cstring>

static SceTouchData touch_buffers[MAX_TOUCH_BUFFER_SAVED][2];
static int touch_buffer_idx = 0;
static bool is_touchpad = false;
static SDL_TouchFingerEvent finger_buffer[8];
static SDL_GamepadTouchpadEvent touchpad_buffer[8];
static uint8_t finger_count = 0;
static SceTouchPortType touchscreen_port = SCE_TOUCH_PORT_FRONT;
static bool is_touched[2] = { false, false };
// Used for mouse support
static int curr_touch_id[2] = { 0, 0 };
// Used for the touch screen
static int next_touch_id = 1;
static uint64_t last_vcount[2] = { 0, 0 };
static bool forceTouchEnabled[2] = { false, false };

static SceTouchData recover_touch_events(const EmuEnvState &emuenv) {
    SceTouchData touch_data;
    touch_data.status = 0;

    for (uint8_t i = 0; i < finger_count; i++) {
        touch_data.report[i].id = static_cast<uint8_t>(finger_buffer[i].touchID);
        touch_data.report[i].force = forceTouchEnabled[touchscreen_port] ? 128 : 0;

        float x = (finger_buffer[i].x * emuenv.drawable_size.x - emuenv.drawable_viewport_pos.x) / emuenv.drawable_viewport_size.x;
        float y = (finger_buffer[i].y * emuenv.drawable_size.y - emuenv.drawable_viewport_pos.y) / emuenv.drawable_viewport_size.y;
        touch_data.report[i].x = static_cast<uint16_t>(x * 1920);

        if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
            touch_data.report[i].y = static_cast<uint16_t>(y * 1088);
        } else {
            touch_data.report[i].y = static_cast<uint16_t>(108 + y * 781);
        }
    }

    touch_data.reportNum = finger_count;

    return touch_data;
}

static SceTouchData recover_touchpad_events(const EmuEnvState &emuenv) {
    SceTouchData touch_data;
    touch_data.status = 0;

    for (uint8_t i = 0; i < finger_count; i++) {
        touch_data.report[i].id = static_cast<uint8_t>(touchpad_buffer[i].which);
        touch_data.report[i].force = forceTouchEnabled[touchscreen_port] ? 128 : 0;

        touch_data.report[i].x = static_cast<uint16_t>(touchpad_buffer[i].x * 1920);
        if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
            touch_data.report[i].y = static_cast<uint16_t>(touchpad_buffer[i].y * 1088);
        } else {
            touch_data.report[i].y = static_cast<uint16_t>(108 + touchpad_buffer[i].y * 781);
        }
    }

    touch_data.reportNum = finger_count;

    return touch_data;
}

void touch_vsync_update(const EmuEnvState &emuenv) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();

    if (finger_count > 0) {
        SceTouchData touch_data = is_touchpad ? recover_touchpad_events(emuenv) : recover_touch_events(emuenv);
        touch_data.timeStamp = timestamp;

        SceTouchData *buffers = touch_buffers[(touch_buffer_idx + 1) % MAX_TOUCH_BUFFER_SAVED];
        for (int port = 0; port < 2; port++) {
            buffers[port].status = 0;
            buffers[port].reportNum = 0;
            buffers[port].timeStamp = timestamp;
        }
        buffers[touchscreen_port] = touch_data;

    } else {
        SceFVector2 touch_pos_window = { 0, 0 };
        const uint32_t buttons = SDL_GetMouseState(&touch_pos_window.x, &touch_pos_window.y);

        for (int port = 0; port < 2; port++) {
            // do it for both the front and the back touchscreen
            SceTouchData *data = &touch_buffers[(touch_buffer_idx + 1) % MAX_TOUCH_BUFFER_SAVED][port];
            memset(data, 0, sizeof(SceTouchData));
            data->timeStamp = timestamp;

            const uint32_t mask = (port == SCE_TOUCH_PORT_BACK) ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
            if ((buttons & mask) && emuenv.renderer_focused) {
                if (!is_touched[port]) {
                    curr_touch_id[port]++;
                    // the touch id must be between 0 and 127
                    curr_touch_id[port] %= 128;
                    is_touched[port] = true;
                }

                SceIVector2 SDL_window_size = { 0, 0 };
                SDL_Window *const window = SDL_GetMouseFocus();
                SDL_GetWindowSize(window, &SDL_window_size.x, &SDL_window_size.y);

                SceFVector2 scale = { 1, 1 };
                if ((SDL_window_size.x > 0) && (SDL_window_size.y > 0)) {
                    scale.x = static_cast<float>(emuenv.drawable_size.x) / SDL_window_size.x;
                    scale.y = static_cast<float>(emuenv.drawable_size.y) / SDL_window_size.y;
                }

                const SceFVector2 touch_pos_drawable = {
                    touch_pos_window.x * scale.x,
                    touch_pos_window.y * scale.y
                };

                const SceFVector2 touch_pos_viewport = {
                    (touch_pos_drawable.x - emuenv.drawable_viewport_pos.x) / emuenv.drawable_viewport_size.x,
                    (touch_pos_drawable.y - emuenv.drawable_viewport_pos.y) / emuenv.drawable_viewport_size.y
                };

                if ((touch_pos_viewport.x >= 0) && (touch_pos_viewport.y >= 0) && (touch_pos_viewport.x < 1) && (touch_pos_viewport.y < 1)) {
                    data->report[data->reportNum].x = static_cast<uint16_t>(touch_pos_viewport.x * 1920);
                    if (port == SCE_TOUCH_PORT_FRONT) {
                        data->report[data->reportNum].y = static_cast<uint16_t>(touch_pos_viewport.y * 1088);
                    } else {
                        data->report[data->reportNum].y = static_cast<uint16_t>(108 + touch_pos_viewport.y * 781);
                    }
                    data->report[data->reportNum].id = curr_touch_id[port];
                    ++data->reportNum;
                }

                if (!emuenv.touch.touch_mode[port]) {
                    data->reportNum = 0;
                }
            } else {
                is_touched[port] = false;
            }
        }
    }

    touch_buffer_idx++;
    touch_buffer_idx %= MAX_TOUCH_BUFFER_SAVED;
}

int handle_touch_event(SDL_TouchFingerEvent &finger) {
    switch (finger.type) {
    case SDL_EVENT_FINGER_DOWN: {
        if (finger_count >= 8)
            // best we can do is clean everything
            finger_count = 0;

        finger_buffer[finger_count] = finger;
        finger_buffer[finger_count].touchID = next_touch_id;
        next_touch_id = (next_touch_id + 1) % 128;
        finger_count++;
        break;
    }
    case SDL_EVENT_FINGER_UP: {
        int finger_index = -1;
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerID == finger_buffer[i].fingerID) {
                finger_index = i;
            }
            if (finger_index != -1) {
                finger_buffer[i] = finger_buffer[i + 1];
            }
        }
        if (finger_count > 0)
            finger_count--;
        break;
    }

    case SDL_EVENT_FINGER_MOTION: {
        for (int i = 0; i < finger_count; i++) {
            if (finger.fingerID == finger_buffer[i].fingerID) {
                SDL_TouchID touch_id = finger_buffer[i].touchID;
                finger_buffer[i] = finger;
                finger_buffer[i].touchID = touch_id;
            }
        }
        break;
    }
    }

    return 0;
}

int handle_touchpad_event(SDL_GamepadTouchpadEvent &touchpad) {
    switch (touchpad.type) {
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        if (finger_count >= 8) // best we can do is clean everything
            finger_count = 0;

        touchpad_buffer[finger_count] = touchpad;
        touchpad_buffer[finger_count].which = next_touch_id;
        next_touch_id = (next_touch_id + 1) % 128;
        finger_count++;
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        for (uint32_t i = 0; i < finger_count; i++) {
            if (touchpad.finger == touchpad_buffer[i].finger) {
                if (i < (finger_count - 1))
                    touchpad_buffer[i] = touchpad_buffer[i + 1];
            }
        }
        if (finger_count > 0)
            finger_count--;
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        for (int i = 0; i < finger_count; i++) {
            if (touchpad.finger == touchpad_buffer[i].finger) {
                const auto touch_id = touchpad_buffer[i].which;
                touchpad_buffer[i] = touchpad;
                touchpad_buffer[i].which = touch_id;
            }
        }
        break;
    }

    is_touchpad = finger_count > 0;

    return 0;
}

std::vector<SceFVector2> get_touchpad_fingers_pos(SceTouchPortType &port) {
    if (!is_touchpad)
        return {};

    std::vector<SceFVector2> touchpad_fingers_pos;
    touchpad_fingers_pos.reserve(finger_count);
    for (int i = 0; i < finger_count; i++) {
        touchpad_fingers_pos.push_back({ touchpad_buffer[i].x, touchpad_buffer[i].y });
    }

    port = touchscreen_port;

    return touchpad_fingers_pos;
}

int toggle_touchscreen() {
    if (touchscreen_port == SCE_TOUCH_PORT_FRONT) {
        touchscreen_port = SCE_TOUCH_PORT_BACK;
    } else {
        touchscreen_port = SCE_TOUCH_PORT_FRONT;
    }
    return 0;
}

int touch_get(const SceUID thread_id, EmuEnvState &emuenv, const SceUInt32 &port, SceTouchData *pData, SceUInt32 count, bool is_peek) {
    memset(pData, 0, sizeof(SceTouchData) * count);
    const int port_idx = static_cast<int>(port);

    int nb_returned_data = 1;
    if (is_peek) {
        if (emuenv.touch.touch_mode[port])
            nb_returned_data = count;
        else
            nb_returned_data = 0;
    } else {
        if (emuenv.display.vblank_count <= last_vcount[port_idx]) {
            // sceTouchRead is blocking, wait for the next vsync for the buffer to be updated
            auto thread = emuenv.kernel.get_thread(thread_id);

            wait_vblank(emuenv.display, emuenv.kernel, thread, last_vcount[port_idx] + 1, false);
        }
        uint64_t vblank_count = emuenv.display.vblank_count.load();
        nb_returned_data = std::min<int>(count, vblank_count - last_vcount[port_idx]);
        last_vcount[port_idx] = vblank_count;
    }

    int corr_buffer_idx;
    if (is_peek) {
        corr_buffer_idx = touch_buffer_idx;
    } else {
        // give the oldest buffer first
        corr_buffer_idx = (touch_buffer_idx - nb_returned_data + 1 + MAX_TOUCH_BUFFER_SAVED) % MAX_TOUCH_BUFFER_SAVED;
    }
    for (int32_t i = 0; i < nb_returned_data; i++) {
        memcpy(&pData[i], &touch_buffers[corr_buffer_idx][port_idx], sizeof(SceTouchData));
        if (forceTouchEnabled[port_idx]) {
            for (int32_t j = 0; j < pData[i].reportNum; j++) {
                pData[i].report[j].force = 128;
            }
        }
        // if peek, repeat the last buffer
        if (!is_peek) {
            corr_buffer_idx++;
            corr_buffer_idx %= MAX_TOUCH_BUFFER_SAVED;
        }
    }

    return nb_returned_data;
}

void touch_set_force_mode(int port, bool mode) {
    forceTouchEnabled[port] = mode;
}
