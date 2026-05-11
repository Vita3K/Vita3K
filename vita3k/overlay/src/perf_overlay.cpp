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

#include <overlay/perf_overlay.h>
#include <overlay/types.h>

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

namespace overlay {

perf_overlay::perf_overlay() {
    visible = true;

    m_body.fore_color = { 1.f, 1.f, 1.f, k_opacity };
    m_body.back_color = { 0.f, 0.f, 0.f, 0.5f * k_opacity };
    m_body.set_font(default_font_name, k_font_size);

    m_fps_graph.set_one_percent_sort_high(false);
    m_fps_graph.set_count(k_graph_datapoints);
    m_fps_graph.set_title("Framerate");
    m_fps_graph.set_font(default_font_name, static_cast<uint16_t>(k_font_size * 0.8f));
    m_fps_graph.set_color({ 0.2f, 1.f, 0.4f, k_opacity });
    m_fps_graph.set_guide_interval(10.f);

    reset_transforms();
}

void perf_overlay::set_detail_level(perf_detail_level level) {
    if (m_detail == level)
        return;

    m_detail = level;
    m_graph_enabled = (level == perf_detail_level::maximum);
    m_force_repaint = true;
}

void perf_overlay::set_position(screen_quadrant quadrant) {
    if (m_quadrant == quadrant)
        return;

    m_quadrant = quadrant;
    m_force_repaint = true;
}

void perf_overlay::set_fps_data(uint32_t fps, uint32_t avg_fps, uint32_t min_fps,
    uint32_t max_fps, uint32_t ms_per_frame,
    const float *fps_values, uint32_t fps_values_count,
    uint32_t fps_offset) {
    const bool changed = (m_fps != fps || m_avg_fps != avg_fps
        || m_min_fps != min_fps || m_max_fps != max_fps
        || m_ms_per_frame != ms_per_frame);

    m_fps = fps;
    m_avg_fps = avg_fps;
    m_min_fps = min_fps;
    m_max_fps = max_fps;
    m_ms_per_frame = ms_per_frame;

    if (changed || m_force_repaint) {
        if (m_graph_enabled && fps_values && fps_values_count > 0) {
            m_fps_graph.record_datapoint(static_cast<float>(fps), true);
            m_fps_graph.set_title(fmt::format("Framerate: {:04.1f}", static_cast<float>(fps)).c_str());
        }

        update_text();
        reset_transforms();
    }
}

void perf_overlay::update_text() {
    std::string text;

    switch (m_detail) {
    case perf_detail_level::minimum:
        text = fmt::format("FPS: {}", m_fps);
        break;
    case perf_detail_level::low:
        text = fmt::format("FPS: {} ({} ms)", m_fps, m_ms_per_frame);
        break;
    case perf_detail_level::medium:
    case perf_detail_level::maximum:
        text = fmt::format("FPS: {} ({} ms)\n"
                           "Avg: {}  Min: {}  Max: {}",
            m_fps, m_ms_per_frame,
            m_avg_fps, m_min_fps, m_max_fps);
        break;
    }

    m_body.set_text(text);
    m_body.auto_resize();
    m_body.refresh();
}

void perf_overlay::reset_body() {
    m_body.set_font(default_font_name, k_font_size);
    m_body.fore_color = { 1.f, 1.f, 1.f, k_opacity };
    m_body.back_color = { 0.f, 0.f, 0.f, 0.5f * k_opacity };
}

void perf_overlay::reset_transforms() {
    if (m_force_repaint) {
        reset_body();
    }

    m_body.set_padding(k_padding, k_padding, k_padding, k_padding);

    uint16_t graph_height = 0;
    if (m_graph_enabled) {
        m_fps_graph.set_size(m_body.w > 0 ? m_body.w : 150, k_graph_h);
        graph_height = m_fps_graph.get_height();
    }

    const uint16_t overlay_width = m_graph_enabled
        ? std::max(m_body.w, m_fps_graph.w)
        : m_body.w;
    const uint16_t overlay_height = static_cast<uint16_t>(
        m_body.h + (m_graph_enabled && m_body.h > 0 ? k_padding + graph_height : 0));

    int16_t pos_x = 0;
    int16_t pos_y = 0;

    switch (m_quadrant) {
    case screen_quadrant::top_left:
        pos_x = k_margin;
        pos_y = k_margin;
        break;
    case screen_quadrant::top_center:
        pos_x = static_cast<int16_t>((virtual_width - overlay_width) / 2);
        pos_y = k_margin;
        break;
    case screen_quadrant::top_right:
        pos_x = static_cast<int16_t>(virtual_width - overlay_width - k_margin);
        pos_y = k_margin;
        break;
    case screen_quadrant::bottom_left:
        pos_x = k_margin;
        pos_y = static_cast<int16_t>(virtual_height - overlay_height - k_margin);
        break;
    case screen_quadrant::bottom_center:
        pos_x = static_cast<int16_t>((virtual_width - overlay_width) / 2);
        pos_y = static_cast<int16_t>(virtual_height - overlay_height - k_margin);
        break;
    case screen_quadrant::bottom_right:
        pos_x = static_cast<int16_t>(virtual_width - overlay_width - k_margin);
        pos_y = static_cast<int16_t>(virtual_height - overlay_height - k_margin);
        break;
    }

    m_body.set_pos(pos_x, pos_y);

    if (m_graph_enabled) {
        int16_t graph_y = static_cast<int16_t>(pos_y + m_body.h + k_padding);
        m_fps_graph.set_pos(pos_x, graph_y);
        m_fps_graph.set_size(overlay_width, k_graph_h);
        m_fps_graph.update();
    }

    m_force_repaint = false;
}

compiled_resource perf_overlay::get_compiled() {
    compiled_resource result;

    if (!visible || m_fps == 0)
        return result;

    auto &body = m_body.get_compiled();
    result.add(body);

    if (m_graph_enabled) {
        auto &gr = m_fps_graph.get_compiled();
        result.add(gr);
    }

    return result;
}

} // namespace overlay
