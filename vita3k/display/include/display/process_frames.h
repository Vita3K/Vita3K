// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <display/state.h>

#include <utility>

namespace display {

inline bool submit_process_frame(DisplayState &display, SceUID pid, DisplayFrameInfo frame) {
    const std::lock_guard lock(display.display_info_mutex);
    const auto it = display.process_frames.insert_or_assign(pid, std::move(frame)).first;
    if (display.active_display_pid != pid)
        return false;
    display.background_frame = it->second;
    return true;
}

inline bool select_process_frame(DisplayState &display, SceUID pid) {
    const std::lock_guard lock(display.display_info_mutex);
    const auto previous = display.background_frame.software_pixels;
    display.active_display_pid = pid;
    const auto it = display.process_frames.find(pid);
    display.background_frame = it == display.process_frames.end() ? DisplayFrameInfo{} : it->second;
    return previous != display.background_frame.software_pixels;
}

inline bool clear_process_frame(DisplayState &display, SceUID pid) {
    const std::lock_guard lock(display.display_info_mutex);
    display.process_frames.erase(pid);
    if (display.active_display_pid != pid || !display.background_frame.software_pixels)
        return false;
    display.background_frame = {};
    return true;
}

} // namespace display