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

#include <config/state.h>
#include <display/state.h>

namespace gui {
static ImVec2 get_perf_pos(ImVec2 window_size, EmuEnvState &emuenv) {
    static const ImVec2 PERF_OVERLAY_PAD = ImVec2(12.f, 12.f);

    const auto TOP = emuenv.viewport_pos.y - PERF_OVERLAY_PAD.y + emuenv.display.imgui_render * 27.f;
    const auto LEFT = emuenv.viewport_pos.x - PERF_OVERLAY_PAD.x;
    const auto CENTER = emuenv.viewport_pos.x + (emuenv.viewport_size.x / 2.f) - (window_size.x / 2.f);
    const auto RIGHT = emuenv.viewport_pos.x + emuenv.viewport_size.x - window_size.x + PERF_OVERLAY_PAD.x;
    const auto BOTTOM = emuenv.viewport_pos.y + emuenv.viewport_size.y - window_size.y + PERF_OVERLAY_PAD.y;

    switch (emuenv.cfg.performance_overlay_position) {
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

static float get_perf_height(EmuEnvState &emuenv) {
    switch (emuenv.cfg.performance_overlay_detail) {
    case MAXIMUM: return 128.f;
    case MEDIUM: return 70.f;
    case LOW:
    case MINIMUM:
    default: break;
    }

    return 47.f;
}

void draw_perf_overlay(GuiState &gui, EmuEnvState &emuenv) {
    static const ImVec4 PERF_OVERLAY_BG_COLOR = ImVec4(0.282f, 0.239f, 0.545f, 0.8f);
    const auto MAIN_WINDOW_SIZE = ImVec2((emuenv.cfg.performance_overlay_detail == MINIMUM ? 151.5f : 286.5f) * emuenv.dpi_scale, get_perf_height(emuenv) * emuenv.dpi_scale);
    const auto WINDOW_SIZE = ImVec2((emuenv.cfg.performance_overlay_detail == MINIMUM ? 135.f : 270.f) * emuenv.dpi_scale, (emuenv.cfg.performance_overlay_detail <= LOW ? 35.f : 58.f) * emuenv.dpi_scale);
    const auto WINDOW_POS = get_perf_pos(MAIN_WINDOW_SIZE, emuenv);
    const auto SCALE_MAX = emuenv.max_fps < 300 ? emuenv.max_fps : 300;

    ImGui::SetNextWindowSize(MAIN_WINDOW_SIZE);
    ImGui::SetNextWindowPos(WINDOW_POS);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##performance", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, PERF_OVERLAY_BG_COLOR);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f * emuenv.dpi_scale);
    ImGui::BeginChild("#perf_stats", WINDOW_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (emuenv.cfg.performance_overlay_detail == PerfomanceOverleyDetail::MINIMUM)
        ImGui::Text("Avg: %u (%u ms)", emuenv.avg_fps, (uint16_t)(1000 / (emuenv.avg_fps ? emuenv.avg_fps : 1)));
    else
        ImGui::Text("FPS: %u (%u ms) Avg: %u (%u ms)", emuenv.fps, (uint16_t)(1000 / (emuenv.fps ? emuenv.fps : 1)), emuenv.avg_fps, (uint16_t)(1000 / (emuenv.avg_fps ? emuenv.avg_fps : 1)));
    if (emuenv.cfg.performance_overlay_detail >= PerfomanceOverleyDetail::MEDIUM) {
        ImGui::Separator();
        ImGui::Text("Min: %u (%u ms) Max: %u (%u ms)", emuenv.min_fps, (uint16_t)(1000 / (emuenv.min_fps ? emuenv.min_fps : 1)), emuenv.max_fps, (uint16_t)(1000 / (emuenv.max_fps ? emuenv.max_fps : 1)));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (emuenv.cfg.performance_overlay_detail == PerfomanceOverleyDetail::MAXIMUM) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (5.f * emuenv.dpi_scale));
        ImGui::PlotLines("##fps_graphic", emuenv.fps_values, IM_ARRAYSIZE(emuenv.fps_values), emuenv.current_fps_offset, nullptr, 0.f, SCALE_MAX, WINDOW_SIZE);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
