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

#include "private.h"

#include <cpu/functions.h>

#include <imgui_memory_editor.h>
#include <spdlog/fmt/fmt.h>

namespace gui {

const char *blacklist[] = {
    "NULL",
    "export_sceGxmDisplayQueueAddEntry"
};

void draw_allocations_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Memory Allocations", &gui.debug_menu.allocations_dialog);

    const std::lock_guard<std::mutex> lock(host.mem.generation_mutex);
    for (const auto &pair : host.mem.page_name_map) {
        const auto generation_num = pair.first;
        const auto generation_name = pair.second;
        const auto page = host.mem.page_table[generation_num];

        if (std::find(std::begin(blacklist), std::end(blacklist), generation_name) != std::end(blacklist))
            continue;

        if (ImGui::TreeNode(fmt::format("{}: {}", generation_num, generation_name).c_str())) {
            ImGui::Text("Range %08lx - %08lx.", generation_num * KB(4), (generation_num + page.size) * KB(4));
            ImGui::Text("Size: %i KB (%i page[s])", page.size * 4, page.size);
            if (ImGui::Selectable("View/Edit")) {
                gui.memory_editor_start = generation_num * KB(4);
                gui.memory_editor_count = page.size * KB(4);
                gui.debug_menu.memory_editor_dialog = true;
            }
            if (ImGui::Selectable("View Disassembly")) {
                sprintf(gui.disassembly_address, "%08zx", page.size * KB(4));
                reevaluate_code(gui, host);
                gui.debug_menu.disassembly_dialog = true;
            }
            ImGui::TreePop();
        }
    }

    if (gui.debug_menu.memory_editor_dialog) {
        if (gui.memory_editor.Open) {
            gui.memory_editor.DrawWindow("Memory Editor",
                host.mem.memory.get() + gui.memory_editor_start,
                gui.memory_editor_count, gui.memory_editor_start);
        } else {
            gui.debug_menu.memory_editor_dialog = false;
        }
    }
    ImGui::End();
}

} // namespace gui
