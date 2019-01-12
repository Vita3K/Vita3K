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

#pragma once

#include <dialog/state.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <memory>

struct ImFont;
struct MemoryEditor;

enum SelectorState {
    SELECT_APP
};

struct Game {
    std::string title;
    std::string title_id;
};

struct GamesSelector {
    std::vector<Game> games;
    std::string selected_title_id;
    bool is_game_list_sorted{ false };
    SelectorState state = SELECT_APP;
};

struct GuiState {
    // Debug menu
    bool renderer_focused = true;
    bool threads_dialog = false;
    bool semaphores_dialog = false;
    bool condvars_dialog = false;
    bool lwcondvars_dialog = false;
    bool mutexes_dialog = false;
    bool lwmutexes_dialog = false;
    bool eventflags_dialog = false;
    bool allocations_dialog = false;

    // Optimisation menu
    bool texture_cache = true;

    // Help menu
    bool controls_dialog = false;

    DialogState common_dialog;
    GamesSelector game_selector;
    std::unique_ptr<MemoryEditor> memory_editor;
    size_t memory_editor_start, memory_editor_count;

    // imgui
    ImFont *normal_font{};
    ImFont *monospaced_font{};
    char *font_data{};
};
