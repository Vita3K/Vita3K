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

#include "ui_private.h"

#include <host/app.h>
#include <host/state.h>

using namespace std::string_literals;

static constexpr auto MENUBAR_HEIGHT = 19;

void DrawGameSelector(HostState &host, AppRunType *run_type) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::Begin("Game Selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    switch (host.gui.game_selector.state) {
    case SELECT_APP:
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 80);
        ImGui::SetWindowFontScale(1.1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        if (ImGui::Button("TitleID")) {
            std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                return lhs.title_id < rhs.title_id;
            });
            host.gui.game_selector.is_game_list_sorted = true;
        }
        ImGui::NextColumn();
        if (ImGui::Button("Title") || !host.gui.game_selector.is_game_list_sorted) {
            std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                return lhs.title < rhs.title;
            });
            host.gui.game_selector.is_game_list_sorted = true;
        }
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (auto game : host.gui.game_selector.games) {
            bool selected_1 = false;
            bool selected_2 = false;
            ImGui::Selectable(game.title_id.c_str(), &selected_1, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::NextColumn();
            ImGui::Selectable(game.title.c_str(), &selected_2, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::NextColumn();
            if (selected_1 || selected_2) {
                host.gui.game_selector.selected_title_id = game.title_id;
                *run_type = AppRunType::Extracted;
            }
        }
        ImGui::PopStyleColor(2);
        break;
    }
    ImGui::End();
}

void DrawReinstallDialog(HostState &host, GenericDialogState *status) {
    ImGui::SetNextWindowPosCenter();
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Reinstall this application?");
    ImGui::Text("This application is already installed.");
    ImGui::Text("Do you want to reinstall it and overwrite existing data?");
    if (ImGui::Button("Yes")) {
        *status = CONFIRM_STATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("No")) {
        *status = CANCEL_STATE;
    }
    ImGui::End();
}

void DrawUI(HostState &host) {
    DrawMainMenuBar(host);

    ImGui::PushFont(host.gui.monospaced_font);
    if (host.gui.threads_dialog) {
        DrawThreadsDialog(host);
    }
    if (host.gui.thread_details_dialog) {
        DrawThreadDetailsDialog(host);
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
    if (host.gui.condvars_dialog) {
        DrawCondvarsDialog(host);
    }
    if (host.gui.lwcondvars_dialog) {
        DrawLwCondvarsDialog(host);
    }
    if (host.gui.eventflags_dialog) {
        DrawEventFlagsDialog(host);
    }
    if (host.gui.controls_dialog) {
        DrawControlsDialog(host);
    }
    if (host.gui.allocations_dialog) {
        DrawAllocationsDialog(host);
    }
    if (host.gui.disassembly_dialog) {
        DrawDisassemblyDialog(host);
    }
    if (host.gui.about_dialog) {
        DrawAboutDialog(host);
    }
    ImGui::PopFont();
}
