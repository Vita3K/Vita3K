// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <config/state.h>
#include <gui/functions.h>
#include <host/state.h>
#include <io/state.h>

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
        ImGui::MenuItem("Install .pkg", nullptr, &state.pkg_install_dialog);
        ImGui::MenuItem("Install .zip, .vpk", nullptr, &state.archive_install_dialog);
        ImGui::EndMenu();
    }
}

static void draw_emulation_menu(GuiState &gui, HostState &host) {
    if (ImGui::BeginMenu("Emulation")) {
        if (ImGui::MenuItem("Load last App", host.cfg->last_app.c_str(), false, !host.cfg->last_app.empty() && host.io->title_id.empty())) {
            host.app_title_id = host.cfg->last_app;
            pre_load_app(gui, host, host.cfg->show_live_area_screen);
        }
        ImGui::EndMenu();
    }
}

static void draw_debug_menu(DebugMenuState &state) {
    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("Threads", nullptr, &state.threads_dialog);
        ImGui::MenuItem("Semaphores", nullptr, &state.semaphores_dialog);
        ImGui::MenuItem("Mutexes", nullptr, &state.mutexes_dialog);
        ImGui::MenuItem("Lightweight Mutexes", nullptr, &state.lwmutexes_dialog);
        ImGui::MenuItem("Condition Variables", nullptr, &state.condvars_dialog);
        ImGui::MenuItem("Lightweight Condition Variables", nullptr, &state.lwcondvars_dialog);
        ImGui::MenuItem("Event Flags", nullptr, &state.eventflags_dialog);
        ImGui::MenuItem("Memory Allocations", nullptr, &state.allocations_dialog);
        ImGui::MenuItem("Disassembly", nullptr, &state.disassembly_dialog);
        ImGui::EndMenu();
    }
}

static void draw_config_menu(GuiState &gui, HostState &host) {
    if (ImGui::BeginMenu("Configuration")) {
        ImGui::MenuItem("Profiles Manager", nullptr, &gui.configuration_menu.profiles_manager_dialog);
        ImGui::MenuItem("Settings", nullptr, &gui.configuration_menu.settings_dialog);
        ImGui::EndMenu();
    }
}

static void draw_controls_menu(ControlMenuState &state) {
    if (ImGui::BeginMenu("Controls")) {
        ImGui::MenuItem("Keyboard Controls", nullptr, &state.controls_dialog);
        ImGui::EndMenu();
    }
}

static void draw_help_menu(HelpMenuState &state) {
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("About", nullptr, &state.about_dialog);
        ImGui::EndMenu();
    }
}

void draw_main_menu_bar(GuiState &gui, HostState &host) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);

        draw_file_menu(gui.file_menu, host);
        draw_emulation_menu(gui, host);
        draw_debug_menu(gui.debug_menu);
        draw_config_menu(gui, host);
        draw_controls_menu(gui.controls_menu);
        draw_help_menu(gui.help_menu);

        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}

} // namespace gui
