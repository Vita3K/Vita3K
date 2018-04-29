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
#include <imgui.h>

#include <host/state.h>
#include <kernel/thread_functions.h>
#include <kernel/thread_state.h>
#include <util/resource.h>

void DrawThreadsDialog(HostState &host) {
    ImGui::Begin("Threads", &host.gui.threads_dialog);
    ImGui::TextColored(ImVec4(255, 255, 0, 255), "%-16s %-32s   %-16s   %-16s", "ID", "Thread Name", "Status", "Stack Pointer");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (auto thread : host.kernel.threads) {
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
        ImGui::Text("0x%08X       %-32s   %-16s   0x%08X       ",
            thread.first,
            th_state->name,
            run_state.c_str(),
            th_state->stack.get()->get());
    }
    ImGui::End();
}
