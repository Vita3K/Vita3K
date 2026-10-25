// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <SDL3/SDL_events.h>
#include <input/physical_key.h>

#include <cstdint>

namespace renderer {
enum class Backend : std::uint32_t;
}

namespace sdl {

enum class EventOutcome {
    Continue,
    QuitRequested
};

struct EventResult {
    EventOutcome outcome = EventOutcome::Continue;
    input::PhysicalKeyCode physical_key = input::PhysicalKeyCode::Unbound;
    uint32_t vita_button = 0;
    bool firmware_updated = false;
};

constexpr int VITA_VIEWPORT_WIDTH = 960;
constexpr int VITA_VIEWPORT_HEIGHT = 544;

struct Viewport {
    float window_width{};
    float window_height{};
    float drawable_width{};
    float drawable_height{};
    float logical_x{};
    float logical_y{};
    float logical_width{};
    float logical_height{};
    float drawable_x{};
    float drawable_y{};
    float drawable_view_width{};
    float drawable_view_height{};
    float system_dpi_scale{ 1.f };
    float manual_dpi_scale{ 1.f };
    float scale_x{};
    float scale_y{};
};

} // namespace sdl