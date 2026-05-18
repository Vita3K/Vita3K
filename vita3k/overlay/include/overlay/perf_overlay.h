// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <overlay/controls.h>
#include <overlay/overlay.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace overlay {

enum class screen_quadrant : uint8_t {
    top_left = 0,
    top_center,
    top_right,
    bottom_left,
    bottom_center,
    bottom_right
};

// Detail level for the performance overlay.
// Maps to PerformanceOverlayDetail from config/config.h.
enum class perf_detail_level : uint8_t {
    minimum = 0, // FPS only
    low, // FPS + ms/frame
    medium, // FPS + ms/frame + min/max/avg
    maximum // FPS + ms/frame + min/max/avg + graph
};

struct perf_overlay : public overlay {
    perf_overlay();

    // Configuration
    void set_detail_level(perf_detail_level level);
    void set_position(screen_quadrant quadrant);
    void set_fps_data(uint32_t fps, uint32_t avg_fps, uint32_t min_fps,
        uint32_t max_fps, uint32_t ms_per_frame,
        const float *fps_values, uint32_t fps_values_count,
        uint32_t fps_offset);

    compiled_resource get_compiled() override;

private:
    void reset_transforms();
    void reset_body();
    void update_text();

    perf_detail_level m_detail = perf_detail_level::minimum;
    screen_quadrant m_quadrant = screen_quadrant::top_left;

    label m_body;
    graph m_fps_graph;
    bool m_graph_enabled = false;

    // Cached stats
    uint32_t m_fps = 0;
    uint32_t m_avg_fps = 0;
    uint32_t m_min_fps = 0;
    uint32_t m_max_fps = 0;
    uint32_t m_ms_per_frame = 0;

    bool m_force_repaint = true;

    static constexpr uint16_t k_font_size = 13;
    static constexpr uint16_t k_padding = 6;
    static constexpr uint16_t k_margin = 8;
    static constexpr float k_opacity = 0.75f;
    static constexpr uint16_t k_graph_h = 40;
    static constexpr uint32_t k_graph_datapoints = 50;
};

} // namespace overlay
