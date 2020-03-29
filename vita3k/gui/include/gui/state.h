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

#pragma once

#include <dialog/state.h>
#include <np/state.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <gui/imgui_impl_sdl_state.h>

#include <glutil/object.h>

#include <queue>
#include <vector>

namespace gui {

enum SelectorState {
    SELECT_APP
};

enum SortState {
    NOT_SORTED,
    ASCENDANT,
    DESCENDANT
};

struct Game {
    std::string app_ver;
    std::string title;
    std::string title_id;
};

struct GamesSelector {
    std::vector<Game> games;
    std::map<std::string, ImGui_Texture> icons;
    std::string selected_title_id;
    bool is_game_list_sorted{ false };
    SortState title_id_sort_state = NOT_SORTED;
    SortState app_ver_sort_state = NOT_SORTED;
    SortState title_sort_state = NOT_SORTED;
    SelectorState state = SELECT_APP;
};

struct LiveAreaState {
    bool live_area_dialog = false;
    bool manual_dialog = false;
};

struct FileMenuState {
    bool firmware_install_dialog = false;
    bool game_install_dialog = false;
};

struct DebugMenuState {
    bool threads_dialog = false;
    bool thread_details_dialog = false;
    bool semaphores_dialog = false;
    bool condvars_dialog = false;
    bool lwcondvars_dialog = false;
    bool mutexes_dialog = false;
    bool lwmutexes_dialog = false;
    bool eventflags_dialog = false;
    bool allocations_dialog = false;
    bool memory_editor_dialog = false;
    bool disassembly_dialog = false;
};

struct ConfigurationMenuState {
    bool profiles_manager_dialog = false;
    bool settings_dialog = false;
};

struct ControlMenuState {
    bool controls_dialog = false;
};

struct HelpMenuState {
    bool about_dialog = false;
};

} // namespace gui

enum class TrophyAnimationStage {
    SLIDE_IN = 0,
    STATIC = 1,
    SLIDE_OUT = 2,
    END = 3
};

struct GuiState {
    std::unique_ptr<ImGui_State> imgui_state;

    bool renderer_focused = true;
    gui::FileMenuState file_menu;
    gui::DebugMenuState debug_menu;
    gui::ConfigurationMenuState configuration_menu;   
	gui::ControlMenuState controls_menu; 
    gui::HelpMenuState help_menu;
    gui::LiveAreaState live_area;
    gui::GamesSelector game_selector;
 

    std::string app_ver;
    bool game_reinstall_confirm = false;

    MemoryEditor memory_editor;
    MemoryEditor gxp_shader_editor;
    size_t memory_editor_start = 0;
    size_t memory_editor_count = 0;

    bool isButtonClicked = false;
    bool isCapturingKeys = false;
    int oldCapturedKey = 0;
    int capturedKey = 0;

    std::string disassembly_arch = "THUMB";
    char disassembly_address[9] = "00000000";
    char disassembly_count[5] = "100";
    std::vector<std::string> disassembly;

    std::string online_id;

    std::vector<std::pair<std::string, bool>> modules;
    ImGuiTextFilter module_search_bar;

    ImTextureID current_background{};
    bool delete_game_background = false;
    bool delete_game_icon = false;
    GLuint display = 0;
    std::map<std::string, ImGui_Texture> game_backgrounds;
    ImGuiTextFilter game_search_bar;
    std::map<std::string, ImGui_Texture> user_backgrounds;

    std::map<std::string, std::map<int, ImGui_Texture>> live_area_contents;
    std::map<std::string, std::map<int, std::map<int, std::vector<ImGui_Texture>>>> live_items;

    std::map<std::string, std::vector<ImGui_Texture>> manuals;

    SceUID thread_watch_index = -1;

    std::uint32_t trophy_window_frame_count{ 0 };
    TrophyAnimationStage trophy_window_frame_stage{ TrophyAnimationStage::SLIDE_IN };
    ImTextureID trophy_window_icon{};

    std::queue<NpTrophyUnlockCallbackData> trophy_unlock_display_requests;
    std::mutex trophy_unlock_display_requests_access_mutex;

    ImVec2 trophy_window_pos;

    // imgui
    ImFont *normal_font{};
    ImFont *monospaced_font{};
    std::vector<char> font_data;
};
