// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <util/vector_utils.h>

#include <imgui_memory_editor.h>

namespace gui {

static constexpr std::array<const char *, 2> blacklist = {
    "NULL",
    "export_sceGxmDisplayQueueAddEntry"
};

void draw_allocations_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Memory Allocations", &gui.debug_menu.allocations_dialog);

    const std::lock_guard<std::mutex> lock(emuenv.mem.generation_mutex);
    for (const auto &[generation_num, generation_name] : emuenv.mem.page_name_map) {
        if (vector_utils::contains(blacklist, generation_name))
            continue;

        const auto &page = emuenv.mem.alloc_table[generation_num];
        if (ImGui::TreeNode(fmt::format("{}: {}", generation_num, generation_name).c_str())) {
            ImGui::Text("Range 0x%08zx - 0x%08zx.", generation_num * KiB(4), (generation_num + page.size) * KiB(4));
            ImGui::Text("Size: %i KiB (%i page[s])", page.size * 4, page.size);
            if (ImGui::Selectable("View/Edit")) {
                gui.memory_editor_start = generation_num * KiB(4);
                gui.memory_editor_count = page.size * KiB(4);
                gui.debug_menu.memory_editor_dialog = true;
            }
            if (ImGui::Selectable("View Disassembly")) {
                snprintf(gui.disassembly_address, sizeof(gui.disassembly_address), "%08zx", page.size * KiB(4));
                reevaluate_code(gui, emuenv);
                gui.debug_menu.disassembly_dialog = true;
            }
            ImGui::TreePop();
        }
    }

    if (gui.debug_menu.memory_editor_dialog) {
        if (gui.memory_editor.Open) {
            gui.memory_editor.DrawWindow("Memory Editor",
                emuenv.mem.memory.get() + gui.memory_editor_start,
                gui.memory_editor_count, gui.memory_editor_start);
        } else {
            gui.debug_menu.memory_editor_dialog = false;
        }
    }
    ImGui::End();
}

} // namespace gui
