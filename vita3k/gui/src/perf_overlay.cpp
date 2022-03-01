// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "private.h"

namespace gui {
static const ImVec2 PERF_OVERLAY_PAD = ImVec2(12.f, 12.f);
static const ImVec4 PERF_OVERLAY_BG_COLOR = ImVec4(0.282f, 0.239f, 0.545f, 0.8f);

static ImVec2 get_perf_pos(ImVec2 window_size, HostState &host) {
    const auto TOP = host.viewport_pos.y - PERF_OVERLAY_PAD.y;
    const auto LEFT = host.viewport_pos.x - PERF_OVERLAY_PAD.x;
    const auto CENTER = host.viewport_pos.x + (host.viewport_size.x / 2.f) - (window_size.x / 2.f);
    const auto RIGHT = host.viewport_pos.x + host.viewport_size.x - window_size.x + PERF_OVERLAY_PAD.x;
    const auto BOTTOM = host.viewport_pos.y + host.viewport_size.y - window_size.y + PERF_OVERLAY_PAD.y;

    switch (host.cfg.performance_overlay_position) {
    case TOP_CENTER: return ImVec2(CENTER, TOP);
    case TOP_RIGHT: return ImVec2(RIGHT, TOP);
    case BOTTOM_LEFT: return ImVec2(LEFT, BOTTOM);
    case BOTTOM_CENTER: return ImVec2(CENTER, BOTTOM);
    case BOTTOM_RIGHT: return ImVec2(RIGHT, BOTTOM);
    case TOP_LEFT:
    default: break;
    }

    return ImVec2(LEFT, TOP);
}

static float get_perf_height(HostState &host) {
    switch (host.cfg.performance_overlay_detail) {
    case MAXIMUM: return 138.f;
    case MEDIUM: return 80.f;
    case LOW:
    case MINIMUM:
    default: break;
    }

    return 57.f;
}

void draw_perf_overlay(GuiState &gui, HostState &host) {
    const auto MAIN_WINDOW_SIZE = ImVec2((host.cfg.performance_overlay_detail == MINIMUM ? 95.5f : 152.f) * host.dpi_scale, get_perf_height(host) * host.dpi_scale);
    const auto WINDOW_POS = get_perf_pos(MAIN_WINDOW_SIZE, host);
    const auto WINDOW_SIZE = ImVec2((host.cfg.performance_overlay_detail == MINIMUM ? 72.5f : 130.f) * host.dpi_scale, (host.cfg.performance_overlay_detail <= LOW ? 35.f : 58.f) * host.dpi_scale);

    ImGui::SetNextWindowSize(MAIN_WINDOW_SIZE);
    ImGui::SetNextWindowPos(WINDOW_POS);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##performance", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, PERF_OVERLAY_BG_COLOR);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f * host.dpi_scale);
    ImGui::BeginChild("#perf_stats", WINDOW_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (host.cfg.performance_overlay_detail == PerfomanceOverleyDetail::MINIMUM)
        ImGui::Text("FPS: %d", host.fps);
    else
        ImGui::Text("FPS: %d Avg: %d", host.fps, host.avg_fps);
    if (host.cfg.performance_overlay_detail >= PerfomanceOverleyDetail::MEDIUM) {
        ImGui::Separator();
        ImGui::Text("Min: %d Max: %d", host.min_fps, host.max_fps);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (host.cfg.performance_overlay_detail == PerfomanceOverleyDetail::MAXIMUM) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (5.f * host.dpi_scale));
        ImGui::PlotLines("##fps_graphic", host.fps_values, IM_ARRAYSIZE(host.fps_values), host.current_fps_offset, nullptr, 0.f, float(host.max_fps), WINDOW_SIZE);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
