// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <gui/imgui_impl.h>

#include <SDL.h>

void imgui::init(WindowPtr window) {
    ImGui::CreateContext();
    ImGui_ImplSdlGL3_Init(window.get());
    ImGui::StyleColorsDark();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
}

void imgui::draw_begin(WindowPtr window) {
    ImGui_ImplSdlGL3_NewFrame(window.get());
}

void imgui::draw_end(WindowPtr window) {
    glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    ImGui::Render();
    ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window.get());
}
