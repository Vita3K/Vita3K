#include <gui/functions.h>
#include <gui/gui_constants.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <host/state.h>

#include <spdlog/fmt/fmt.h>

void DrawAllocationsDialog(HostState &host) {
    ImGui::Begin("Memory Allocations", &host.gui.allocations_dialog);

    const std::lock_guard<std::mutex> lock(host.mem.generation_mutex);
    for (const auto &pair : host.mem.generation_names) {
        if (ImGui::TreeNode(fmt::format("{}: {}", pair.first, pair.second).c_str())) {
            long index = -1, count = 1;
            
            for (long a = 0; a < host.mem.allocated_pages.size(); a++) {
                if (index != -1) {
                    if (host.mem.allocated_pages[a] != pair.first) break;
                    count++;
                }

                if (index == -1 && host.mem.allocated_pages[a] == pair.first) {
                    index = a;
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
                        if (!host.gui.memory_editor) host.gui.memory_editor = new MemoryEditor;
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    if (host.gui.memory_editor) {
        if (host.gui.memory_editor->Open) {
            host.gui.memory_editor->DrawWindow("Editor", host.mem.memory.get() + host.gui.memory_editor_start,
                    host.gui.memory_editor_count, host.gui.memory_editor_start);
        } else {
            delete host.gui.memory_editor;
            host.gui.memory_editor = nullptr;
        }
    }

    ImGui::End();
}
