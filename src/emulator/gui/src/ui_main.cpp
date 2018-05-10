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

#include <gui/functions.h>
#include <host/state.h>
#include <imgui.h>

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;
static constexpr auto WINDOW_BORDER_WIDTH = 16;
static constexpr auto WINDOW_BORDER_HEIGHT = 34;

void DrawGameSelector(HostState &host, bool *is_vpk) {
    ImGui::SetNextWindowPos(ImVec2(0, 19), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(DEFAULT_RES_WIDTH + WINDOW_BORDER_WIDTH, DEFAULT_RES_HEIGHT + WINDOW_BORDER_HEIGHT), ImGuiSetCond_Always);
    ImGui::Begin("", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    switch (host.gui.game_selector.state) {
    case MAIN_MENU:
        ImGui::TextColored(ImVec4(255, 255, 0, 255), "Welcome to Vita3K");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(255, 255, 0, 255), "Select what to do");
        if (ImGui::Button("Load an installed game/application")){
			host.gui.game_selector.state = SELECT_APP;
        }
        break;
    case SELECT_APP:
        ImGui::TextColored(ImVec4(255, 255, 0, 255), "Select the game/application to start");
        ImGui::Separator();
        for (auto titleid : host.gui.game_selector.title_ids) {
            if (ImGui::Button(titleid.c_str())){
                host.gui.game_selector.title_id = titleid;
                *is_vpk = false;
            }
        }
        break;
    }
    ImGui::End();
}

void DrawUI(HostState &host) {
    DrawMainMenuBar(host);

    if (host.gui.threads_dialog) {
        DrawThreadsDialog(host);
    }
    if (host.gui.semaphores_dialog) {
        DrawSemaphoresDialog(host);
    }
    if (host.gui.mutexes_dialog) {
        DrawMutexesDialog(host);
    }
    if (host.gui.lwmutexes_dialog) {
        DrawLwMutexesDialog(host);
    }
}
