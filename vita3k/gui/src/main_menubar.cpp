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

#include "private.h"

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

namespace gui {

static void draw_file_menu(FileMenuState &state, HostState &host) {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Pref Path")) {
            system((OS_PREFIX + host.pref_path).c_str());
        }
        ImGui::Separator();
        ImGui::MenuItem("Install Firmware", nullptr, &state.firmware_install_dialog);
        ImGui::MenuItem("Install Game", nullptr, &state.game_install_dialog);
        ImGui::EndMenu();
    }
}

static void draw_debug_menu(DebugMenuState &state) {
    if (ImGui::BeginMenu("Debug")) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
        ImGui::MenuItem("Threads", nullptr, &state.threads_dialog);
        ImGui::MenuItem("Semaphores", nullptr, &state.semaphores_dialog);
        ImGui::MenuItem("Mutexes", nullptr, &state.mutexes_dialog);
        ImGui::MenuItem("Lightweight Mutexes", nullptr, &state.lwmutexes_dialog);
        ImGui::MenuItem("Condition Variables", nullptr, &state.condvars_dialog);
        ImGui::MenuItem("Lightweight Condition Variables", nullptr, &state.lwcondvars_dialog);
        ImGui::MenuItem("Event Flags", nullptr, &state.eventflags_dialog);
        ImGui::MenuItem("Memory Allocations", nullptr, &state.allocations_dialog);
        ImGui::MenuItem("Disassembly", nullptr, &state.disassembly_dialog);
        ImGui::PopStyleColor();
        ImGui::EndMenu();
    }
}

static void draw_config_menu(ConfigurationMenuState &state) {
    if (ImGui::BeginMenu("Configuration")) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
        ImGui::MenuItem("Profiles Manager", nullptr, &state.profiles_manager_dialog);
        ImGui::MenuItem("Settings", nullptr, &state.settings_dialog);
        ImGui::PopStyleColor();
        ImGui::EndMenu();
    }
}

static void draw_controls_menu(ControlMenuState &state) {
    if (ImGui::BeginMenu("Controls")) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
        ImGui::MenuItem("Keyboard Controls", nullptr, &state.controls_dialog);
        ImGui::PopStyleColor();
        ImGui::EndMenu();
    }
}

static void draw_help_menu(HelpMenuState &state) {
    if (ImGui::BeginMenu("Help")) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
        ImGui::MenuItem("About", nullptr, &state.about_dialog);
        ImGui::PopStyleColor();
        ImGui::EndMenu();
    }
}

void draw_main_menu_bar(GuiState &gui, HostState &host) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);

        draw_file_menu(gui.file_menu, host);
        draw_debug_menu(gui.debug_menu);
        draw_config_menu(gui.configuration_menu);
        draw_controls_menu(gui.controls_menu);
        draw_help_menu(gui.help_menu);

        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}

} // namespace gui
