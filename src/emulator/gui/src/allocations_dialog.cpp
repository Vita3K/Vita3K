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

void draw_allocations_dialog(HostState &host, GuiState &gui) {
    ImGui::Begin("Memory Allocations", &gui.debug_menu.allocations_dialog);

    const std::lock_guard<std::mutex> lock(host.mem.generation_mutex);
    for (const auto &pair : host.mem.generation_names) {
        const auto generation_num = pair.first;
        const auto generation_name = pair.second;

        if (std::find(std::begin(blacklist), std::end(blacklist), generation_name) != std::end(blacklist))
            continue;

        if (ImGui::TreeNode(fmt::format("{}: {}", generation_num, generation_name).c_str())) {
            int32_t index = -1;
            int32_t count = 1;

            for (int32_t a = 0; a < host.mem.allocated_pages.size(); a++) {
                auto generation = host.mem.allocated_pages[a];
                if (index != -1) {
                    if (generation != generation_num)
                        break;
                    count++;
                }

                if (index == -1 && generation == generation_num) {
                    index = a;
                }
            }

            if (index == -1) {
                ImGui::Text("Generation no longer exists.");
            } else {
                ImGui::Text("Range %08lx - %08lx.", index * KB(4), (index + count) * KB(4));
                ImGui::Text("Size: %i KB (%i page[s])", count * 4, count);
                if (ImGui::Selectable("View/Edit")) {
                    gui.memory_editor_start = index * KB(4);
                    gui.memory_editor_count = count * KB(4);
                    gui.debug_menu.memory_editor_dialog = true;
                }
                if (ImGui::Selectable("View Disassembly")) {
                    sprintf(gui.disassembly_address, "%08zx", index * KB(4));
                    reevaluate_code(host, gui);
                    gui.debug_menu.disassembly_dialog = true;
                }
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
