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

#include <gui/functions.h>

#include "private.h"

namespace gui {

void set_shaders_compiled_display(GuiState &gui, HostState &host) {
    const uint64_t time = std::time(nullptr);
    if (host.renderer->shaders_count_compiled) {
        gui.shaders_compiled_display[Count] = host.renderer->shaders_count_compiled;
        gui.shaders_compiled_display[Time] = time;
        host.renderer->shaders_count_compiled = 0;
    } else if (!gui.shaders_compiled_display.empty()) {
        // Display shaders compliled count during 2 sec
        if ((gui.shaders_compiled_display[Time] + 2) <= time)
            gui.shaders_compiled_display.clear();
    }
}

void draw_shaders_count_compiled(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    ImGui::SetNextWindowPos(ImVec2(0.f, 510.f * SCALE.y));
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##shaders_compiled", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::Text("%lu shaders compiled", gui.shaders_compiled_display[Count]);
    ImGui::End();
}

} // namespace gui
