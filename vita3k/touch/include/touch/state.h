#pragma once

#include <SDL3/SDL_events.h>

#include <touch/touch.h>

#include <algorithm>

struct TouchState {
    SceTouchSamplingState touch_mode[2] = { SCE_TOUCH_SAMPLING_STATE_STOP, SCE_TOUCH_SAMPLING_STATE_STOP };

    float mouse_x = 0;
    float mouse_y = 0;
    bool mouse_button_left = false;
    bool mouse_button_right = false;
    bool renderer_focused = false;
    SceTouchPortType touchscreen_port = SCE_TOUCH_PORT_FRONT;
    SceTouchData touch_buffers[MAX_TOUCH_BUFFER_SAVED][2] = {};
    int touch_buffer_idx = 0;
    bool is_touchpad = false;
    SDL_TouchFingerEvent finger_buffer[8] = {};
    SDL_GamepadTouchpadEvent touchpad_buffer[8] = {};
    uint8_t finger_count = 0;
    uint8_t touchpad_finger_count = 0;
    bool is_touched[2] = { false, false };
    int curr_touch_id[2] = { 0, 0 };
    int next_touch_id = 1;
    uint64_t last_vcount[2] = { 0, 0 };
    bool force_touch_enabled[2] = { false, false };
    bool pinch_modifier_enabled = false;
    float pinch_velocity = 0.f;
    bool old_pinch_behavior = false;
    static constexpr SceInt16 initial_pinch_dist = 300;
    SceInt16 pinch_dist = initial_pinch_dist;
    SceInt16 pinch_speed = 40;

    void reset_runtime() {
        touch_mode[0] = SCE_TOUCH_SAMPLING_STATE_STOP;
        touch_mode[1] = SCE_TOUCH_SAMPLING_STATE_STOP;
        mouse_x = 0.f;
        mouse_y = 0.f;
        mouse_button_left = false;
        mouse_button_right = false;
        renderer_focused = false;
        touchscreen_port = SCE_TOUCH_PORT_FRONT;
        std::fill_n(touch_buffers[0], MAX_TOUCH_BUFFER_SAVED * 2, SceTouchData{});
        touch_buffer_idx = 0;
        is_touchpad = false;
        std::fill_n(finger_buffer, 8, SDL_TouchFingerEvent{});
        std::fill_n(touchpad_buffer, 8, SDL_GamepadTouchpadEvent{});
        finger_count = 0;
        touchpad_finger_count = 0;
        is_touched[0] = false;
        is_touched[1] = false;
        curr_touch_id[0] = 0;
        curr_touch_id[1] = 0;
        next_touch_id = 1;
        last_vcount[0] = 0;
        last_vcount[1] = 0;
        force_touch_enabled[0] = false;
        force_touch_enabled[1] = false;
        pinch_modifier_enabled = false;
        pinch_velocity = 0.f;
        old_pinch_behavior = false;
        pinch_dist = initial_pinch_dist;
        pinch_speed = 40;
    }
};
