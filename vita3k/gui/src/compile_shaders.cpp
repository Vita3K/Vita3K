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

#include <gui/functions.h>
#include <io/state.h>
#include <renderer/state.h>

#include "private.h"

namespace gui {

static std::vector<std::string> points = { ".", "..", "..." };
static int pos = 2;
static auto time = std::time(nullptr);
void draw_pre_compiling_shaders_progress(GuiState &gui, EmuEnvState &emuenv, const uint32_t &total) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto WINDOW_SIZE = ImVec2(616.f * SCALE.x, 236.f * SCALE.y);
    const auto ICON_SIZE_SCALE = ImVec2(96.f * SCALE.x, 96.f * SCALE.y);

    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::Begin("##shaders_pre_compile", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(1.1f * RES_SCALE.x);

    // Check if icon exist
    if (gui.app_selector.vita_apps_icon.contains(emuenv.io.app_path)) {
        ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, 32.f * SCALE.y));
        ImGui::Image(get_app_icon(gui, emuenv.io.app_path)->second, ICON_SIZE_SCALE);
    }

    const auto current_time = std::time(nullptr);
    if (time < current_time) {
        pos = pos < (points.size() - 1) ? pos + 1 : 0;
        time = current_time;
    }
    ImGui::SetCursorPos(ImVec2((176.f * SCALE.x), (52.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.current_app_title.c_str());
    ImGui::SetCursorPos(ImVec2((176.f * SCALE.x), ImGui::GetCursorPosY() + (30.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s%s", gui.lang.compile_shaders["compiling_shaders"].c_str(), points[pos].c_str());
    const float PROGRESS_BAR_WIDTH = 508.f * SCALE.x;
    ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f), ImGui::GetCursorPosY() + 30.f * emuenv.manual_dpi_scale));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    const auto progress_programs = (emuenv.renderer->programs_count_pre_compiled * 100) / total;
    ImGui::ProgressBar(progress_programs / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * emuenv.manual_dpi_scale), "");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * emuenv.manual_dpi_scale));
    TextColoredCentered(GUI_COLOR_TEXT, fmt::format("{}/{}", emuenv.renderer->programs_count_pre_compiled, total).c_str());
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

void set_shaders_compiled_display(GuiState &gui, EmuEnvState &emuenv) {
    const uint64_t time = std::time(nullptr);
    if (emuenv.renderer->shaders_count_compiled > 0) {
        gui.shaders_compiled_display_count += emuenv.renderer->shaders_count_compiled;
        gui.shaders_compiled_display_time = time;
        emuenv.renderer->shaders_count_compiled = 0;
    } else if (gui.shaders_compiled_display_count > 0) {
        // Display shaders compiled count during 3 sec
        if ((gui.shaders_compiled_display_time + 3) <= time)
            gui.shaders_compiled_display_count = 0;
    }
}

void draw_shaders_count_compiled(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::SetNextWindowPos(ImVec2(emuenv.drawable_viewport_pos.x + (2.f * emuenv.manual_dpi_scale), emuenv.drawable_viewport_pos.y + emuenv.drawable_viewport_size.y - (42.f * emuenv.manual_dpi_scale)));
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##shaders_compiled", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const char *gpu_objects_compiled_msg;
    if (emuenv.renderer->current_backend == renderer::Backend::Vulkan) {
        gpu_objects_compiled_msg = gui.lang.compile_shaders["pipelines_compiled"].c_str();
    } else {
        gpu_objects_compiled_msg = gui.lang.compile_shaders["shaders_compiled"].c_str();
    }
    const auto shaders_compiled_str = fmt::format(fmt::runtime(gpu_objects_compiled_msg), gui.shaders_compiled_display_count);
    ImGui::Text("%s", shaders_compiled_str.c_str());
    ImGui::End();
}

} // namespace gui
