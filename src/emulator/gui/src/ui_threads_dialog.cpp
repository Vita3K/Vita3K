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

#include <gui/functions.h>

#include "ui_private.h"

#include <cpu/functions.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>
#include <host/state.h>
#include <util/resource.h>

#include <spdlog/fmt/fmt.h>

void DrawThreadDetailsDialog(HostState &host) {
    ThreadStatePtr &thread = host.kernel.threads[host.gui.thread_watch_index];
    CPUState &cpu = *thread->cpu;

    ImGui::Begin("Thread Viewer", &host.gui.thread_details_dialog);

    uint32_t pc = read_pc(cpu);
    uint32_t sp = read_sp(cpu);
    uint32_t lr = read_lr(cpu);
    uint32_t registers[16];
    for (size_t a = 0; a < 16; a++)
        registers[a] = read_reg(cpu, a);

    // TODO: Add THUMB/ARM mode viewer. What arch is the cpu currently using?

    ImGui::Text("Name: %s (%x)", thread->name.c_str(), host.gui.thread_watch_index);

    ImGui::Text("PC: %08x", pc);
    ImGui::Text("SP: %08x", sp);
    ImGui::Text("LR: %08x", lr);
    ImGui::Text("Executing: %s", disassemble(cpu, pc).c_str());
    ImGui::Separator();
    for (int a = 0; a < 8; a++) {
        ImGui::Text("r%02i: %08x   r%02i: %08x", a, registers[a], a + 8, registers[a + 8]);
    }

    ImGui::End();
}

void DrawThreadsDialog(HostState &host) {
    ImGui::Begin("Threads", &host.gui.threads_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE,
        "%-16s %-32s   %-16s   %-16s", "ID", "Thread Name", "Status", "Stack Pointer");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (const auto &thread : host.kernel.threads) {
        std::shared_ptr<ThreadState> th_state = thread.second;
        std::string run_state;
        switch (th_state->to_do) {
        case ThreadToDo::run:
            run_state = "Running";
            break;
        case ThreadToDo::wait:
            run_state = "Waiting";
            break;
        case ThreadToDo::exit:
            run_state = "Exiting";
            break;
        }
        if (ImGui::Selectable(fmt::format("{:0>8X}         {:<32}   {:<16}   {:0<8X}",
                thread.first, th_state->name, run_state, th_state->stack.get()->get())
                                  .c_str())) {
            host.gui.thread_watch_index = thread.first;
            host.gui.thread_details_dialog = true;
        }
    }
    ImGui::End();
}
