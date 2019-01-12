#include <gui/functions.h>
#include <gui/gui_constants.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <host/state.h>

#include <spdlog/fmt/fmt.h>

void DrawAllocationsDialog(HostState &host) {
    ImGui::Begin("Memory Allocations", &host.gui.allocations_dialog);

    const char *blacklist[] = {
        "NULL",
        "export_sceGxmDisplayQueueAddEntry"
    };

    const std::lock_guard<std::mutex> lock(host.mem.generation_mutex);
    for (const auto &pair : host.mem.generation_names) {
        const auto generation_num = pair.first;
        const auto generation_name = pair.second;

        if (std::find(std::begin(blacklist), std::end(blacklist), generation_name) != std::end(blacklist))
            continue;

        if (ImGui::TreeNode(fmt::format("{}: {}", generation_num, generation_name).c_str())) {
            int32_t index = -1;
            uint32_t count = 1;

            for (const auto page : host.mem.allocated_pages) {
                if (index != -1) {
                    if (page != generation_num)
                        break;
                    count++;
                }

                if (index == -1 && page == generation_num) {
                    index = page;
                }
            }

            if (index == -1) {
                ImGui::Text("Generation no longer exists.");
            } else {
                ImGui::Text("Range %08lx - %08lx.", index * KB(4), (index + count) * KB(4));
                ImGui::Text("Size: %li KB (%li page[s])", count * 4l, count);
                if (index != 0) {
                    if (ImGui::Selectable("View/Edit")) {
                        host.gui.memory_editor_start = index * KB(4);
                        host.gui.memory_editor_count = count * KB(4);
                        if (!host.gui.memory_editor)
                            host.gui.memory_editor = std::make_unique<MemoryEditor>();
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    if (host.gui.memory_editor && host.gui.memory_editor->Open)
        host.gui.memory_editor->DrawWindow("Memory Editor", host.mem.memory.get() + host.gui.memory_editor_start,
            host.gui.memory_editor_count, host.gui.memory_editor_start);

    ImGui::End();
}
