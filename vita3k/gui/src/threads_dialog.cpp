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

#include <kernel/thread/thread_state.h>

#include <spdlog/fmt/fmt.h>

namespace gui {

void draw_thread_details_dialog(GuiState &gui, HostState &host) {
    ThreadStatePtr &thread = host.kernel.threads[gui.thread_watch_index];
    CPUState &cpu = *thread->cpu;

    ImGui::Begin("Thread Viewer", &gui.debug_menu.thread_details_dialog);

    uint32_t pc = read_pc(cpu);
    uint32_t sp = read_sp(cpu);
    uint32_t lr = read_lr(cpu);
    uint32_t registers[13];
    for (size_t a = 0; a < 13; a++)
        registers[a] = read_reg(cpu, a);

    // TODO: Add THUMB/ARM mode viewer. What arch is the cpu currently using?

    ImGui::Text("Name: %s (%x)", thread->name.c_str(), gui.thread_watch_index);

    ImGui::Text("PC: %08x", pc);
    ImGui::Text("SP: %08x", sp);
    ImGui::Text("LR: %08x", lr);
    ImGui::Text("Executing: %s", disassemble(cpu, pc).c_str());
    ImGui::Separator();
    for (int a = 0; a < 6; a++) {
        ImGui::Text("r%02i: %08x   r%02i: %08x", a, registers[a], a + 6, registers[a + 6]);
    }
    ImGui::Text("r12: %08x", registers[12]);

    ImGui::End();
}

void draw_threads_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Threads", &gui.debug_menu.threads_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE,
        "%-16s %-32s   %-16s   %-16s", "ID", "Thread Name", "Status", "Stack Pointer");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (const auto &thread : host.kernel.threads) {
        std::shared_ptr<ThreadState> th_state = thread.second;
        std::string run_state;
        switch (th_state->status) {
        case ThreadStatus::run:
            run_state = "Running";
            break;
        case ThreadStatus::wait:
            run_state = "Waiting";
            break;
        case ThreadStatus::dormant:
            run_state = "Dormant";
            break;
        }
        if (ImGui::Selectable(fmt::format("{:0>8X}         {:<32}   {:<16}   {:0>8X}",
                thread.first, th_state->name, run_state, th_state->stack.get())
                                  .c_str())) {
            gui.thread_watch_index = thread.first;
            gui.debug_menu.thread_details_dialog = true;
        }
    }
    ImGui::End();
}

} // namespace gui
