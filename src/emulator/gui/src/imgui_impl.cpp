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

#include <gui/functions.h>

#include <SDL.h>

void imgui::init(WindowPtr window) {
    ImGui::CreateContext();
    ImGui_ImplSdlGL3_Init(window.get());
    ImGui::StyleColorsDark();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
}

void imgui::draw_begin(WindowPtr window) {
    ImGui_ImplSdlGL3_NewFrame(window.get());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void imgui::draw_main(HostState& host, GLuint texture_id) {
    const auto window_size = host.display.window_size;
    const auto image_size = host.display.image_size;

    // workaround for imgui-related crash
    if (host.display.image_size.width == 0)
        return;

    glBindTexture(GL_TEXTURE_2D, texture_id);
    void *const pixels = host.display.base.cast<void>().get(host.mem);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, host.display.pitch);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_size.width, image_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_size.width, window_size.height), ImGuiSetCond_Always);
    ImGui::Begin("", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    host.gui.renderer_focused = ImGui::IsWindowFocused();
    ImGui::Image(reinterpret_cast<void *>(texture_id), ImVec2(image_size.width, image_size.height));
    ImGui::End();
}

void imgui::draw_end(WindowPtr window) {
    glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    ImGui::Render();
    ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window.get());
}
