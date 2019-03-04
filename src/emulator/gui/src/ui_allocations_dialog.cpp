#include <gui/functions.h>

#include "ui_private.h"

#include <cpu/functions.h>
#include <host/state.h>

#include <imgui_memory_editor.h>
#include <spdlog/fmt/fmt.h>

namespace gui {

const char *blacklist[] = {
    "NULL",
    "export_sceGxmDisplayQueueAddEntry"
};

void DrawAllocationsDialog(HostState &host) {
    ImGui::Begin("Memory Allocations", &host.gui.allocations_dialog);

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
                    host.gui.memory_editor_start = index * KB(4);
                    host.gui.memory_editor_count = count * KB(4);
                    host.gui.memory_editor_dialog = true;
                }
                if (ImGui::Selectable("View Disassembly")) {
                    sprintf(host.gui.disassembly_address, "%08zx", index * KB(4));
                    ReevaluateCode(host);
                    host.gui.disassembly_dialog = true;
                }
            }
            ImGui::TreePop();
        }
    }

    if (host.gui.memory_editor_dialog) {
        if (host.gui.memory_editor.Open) {
            host.gui.memory_editor.DrawWindow("Memory Editor",
                host.mem.memory.get() + host.gui.memory_editor_start,
                host.gui.memory_editor_count, host.gui.memory_editor_start);
        } else {
            host.gui.memory_editor_dialog = false;
        }
    }

    ImGui::End();
}

} // namespace gui
