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

#include <host/state.h>

#include <kernel/thread/thread_state.h>

namespace gui {
static const ImVec2 PERF_OVERLAY_POS = ImVec2(10.0f, 10.0f);
static const ImVec4 PERF_OVERLAY_BG_COLOR = ImVec4(0.282f, 0.239f, 0.545f, 1.0f);

void draw_perf_overlay(GuiState &gui, HostState &host) {
    ImGui::SetNextWindowPos(ImVec2(PERF_OVERLAY_POS.x + host.viewport_pos.x, PERF_OVERLAY_POS.y));
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, PERF_OVERLAY_BG_COLOR);
    ImGui::Begin("##performance", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %d", host.fps);
    if (host.cfg.performance_overlay_detail >= PerfomanceOverleyDetail::MEDIUM) {
        ImGui::Separator();
        ImGui::Text("Avg: %d\nMin: %d Max: %d", host.avg_fps, host.min_fps, host.max_fps);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    if (host.cfg.performance_overlay_detail == PerfomanceOverleyDetail::MAXIMUM) {
        ImGui::SetNextWindowPos(ImVec2(PERF_OVERLAY_POS.x + host.viewport_pos.x - (11.f * host.dpi_scale), PERF_OVERLAY_POS.y + (60.f * host.dpi_scale)));
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::Begin("##fps_graphic", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PlotLines("##fps_graphic", host.fps_values, IM_ARRAYSIZE(host.fps_values), host.current_fps_offset, nullptr, 0.f, float(host.max_fps), ImVec2(160.f * host.dpi_scale, 60.f * host.dpi_scale));
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

} // namespace gui
