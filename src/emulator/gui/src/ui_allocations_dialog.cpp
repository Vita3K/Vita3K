#include <gui/functions.h>
#include <gui/gui_constants.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <host/state.h>

#include <string.h>

void DrawAllocationsDialog(HostState &host) {
    ImGui::Begin("Allocations", &host.gui.allocations_dialog);

    const std::lock_guard<std::mutex> lock(host.mem.generation_mutex);
    for (const auto &pair : host.mem.generation_names) {
        int length = snprintf(nullptr, 0, "%i: %s", (int)pair.first, pair.second.c_str());
        std::vector<char> buffer((unsigned long)length);
        sprintf(&buffer[0], "%i: %s", (int)pair.first, pair.second.c_str());

        if (ImGui::TreeNode(&buffer[0])) {
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
                long start = index * KB(4);
                ImGui::Text("Range %08lx - %08lx.", start, (index + count) * KB(4));
                ImGui::Text("Size: %li KB (%li page[s])", count * 4l, count);
                if (index != 0) {
                    if (ImGui::Selectable("View/Edit")) {
                        host.gui.memory_editor_start = start;
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
                    (size_t) host.gui.memory_editor_count, (size_t) host.gui.memory_editor_start);
        } else {
            delete host.gui.memory_editor;
            host.gui.memory_editor = nullptr;
        }
    }

    ImGui::End();
}