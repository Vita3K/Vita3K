// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <ctrl/state.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>

#include <SDL_events.h>

namespace gui {

bool rebinds_is_open = false;
bool is_capturing_buttons = false;

void add_bind_to_table(GuiState &gui, EmuEnvState &emuenv, const char *button_name, int pos) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", button_name);
    ImGui::TableSetColumnIndex(1);
    std::string button_button_name = "Button " + std::to_string(emuenv.cfg.controller_binds[pos]) + "##" + std::to_string(pos);     //thanks imgui :smile:
    if (ImGui::Button(button_button_name.c_str())) {
        bool something_happened = false;
        is_capturing_buttons = true;
        while (is_capturing_buttons) {
            // query SDL for button events
            SDL_Event get_controller_events;
            while(SDL_PollEvent(&get_controller_events)){
                switch(get_controller_events.type){
                case SDL_CONTROLLERBUTTONDOWN:
                    emuenv.cfg.controller_binds[pos] = (short)get_controller_events.cbutton.button;
                    something_happened = true;
                case SDL_KEYDOWN:
                    if(get_controller_events.key.type == SDLK_ESCAPE)   //allow esc or screen tap to stop the SDL loop from spinning indefinitely.
                        is_capturing_buttons = false;
                case SDL_FINGERDOWN:
                    is_capturing_buttons = false;
                }
            }
            if (something_happened){
                config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                something_happened = false;
                is_capturing_buttons = false;
                break;
            }
        }
    }
}

void draw_controllers_dialog(GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.controllers;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(lang["title"].c_str(), &gui.controls_menu.controllers_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    auto &ctrl = emuenv.ctrl;
    if (ctrl.controllers_num) {
        const auto connected_str = fmt::format(fmt::runtime(lang["connected"].c_str()), ctrl.controllers_num);
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", connected_str.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::BeginTable("main", 2)) {
            ImGui::TableSetupColumn("num");
            ImGui::TableSetupColumn("name");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["num"].c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["name"].c_str());
            ImGui::Spacing();
            for (auto i = 0; i < ctrl.controllers_num; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%-8d", i);
                ImGui::TableSetColumnIndex(1);
                if(ImGui::Button(ctrl.controllers_name[i]))
                    rebinds_is_open = true;
                if(rebinds_is_open){
                    float height = emuenv.viewport_size.y / emuenv.dpi_scale;
                    if (ImGui::BeginMainMenuBar()) {
                        height = height - ImGui::GetWindowHeight() * 2;
                        ImGui::EndMainMenuBar();
                    }
                    ImGui::SetNextWindowSize(ImVec2(0, height-5.0f));
                    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::Begin("Rebind Controls", &rebinds_is_open);
                    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", ctrl.controllers_name[i]);
                    ImGui::Separator();
                    ImGui::Spacing();
                    if(ImGui::BeginTable("rebindTable", 2)){
                        ImGui::TableSetupColumn("vitaControl");
                        ImGui::TableSetupColumn("controllerControl");
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", "Control");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", "Mapped Button");

                        //these are added to the table based on the SDL_GameControllerButton enumeration
                        add_bind_to_table(gui, emuenv, "Cross", 0);
                        add_bind_to_table(gui, emuenv, "Circle", 1);
                        add_bind_to_table(gui, emuenv, "Square", 2);
                        add_bind_to_table(gui, emuenv, "Triangle", 3);
                        add_bind_to_table(gui, emuenv, "Select", 4);
                        add_bind_to_table(gui, emuenv, "PS Button", 5);
                        add_bind_to_table(gui, emuenv, "Start", 6);
                        add_bind_to_table(gui, emuenv, "L3", 7);
                        add_bind_to_table(gui, emuenv, "R3", 8);
                        add_bind_to_table(gui, emuenv, "L1", 9);
                        add_bind_to_table(gui, emuenv, "R1", 10);
                        add_bind_to_table(gui, emuenv, "D-Pad Up", 11);
                        add_bind_to_table(gui, emuenv, "D-Pad Down", 12);
                        add_bind_to_table(gui, emuenv, "D-Pad Left", 13);
                        add_bind_to_table(gui, emuenv, "D-Pad Right", 14);

                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::End();
                }
            }
            ImGui::EndTable();
        }
    } else
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", lang["not_connected"].c_str());
    ImGui::End();
}

} // namespace gui
