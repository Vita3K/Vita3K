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

#include <imgui.h>
#include <gui/functions.h>

#include <host/state.h>
#include <kernel/thread_functions.h>
#include <kernel/thread_state.h>
#include <util/resource.h>

void DrawThreadsDialog(HostState& host){
    ImGui::Begin("Threads", &host.gui.threads_dialog);
    ImGui::TextColored(ImVec4(255,255,0,255), "%-32s   %-16s   %-16s", "Thread Name", "Status", "Stack Pointer");
    for (auto thread : host.kernel.threads) {
        std::shared_ptr<ThreadState> th_state = thread.second;
        switch (th_state->to_do){
        case ThreadToDo::run:
            ImGui::Text("%-32s   %-16s   0x%08X    ",
                th_state->name,
                "Running",
                th_state->stack.get()->get());
            break;
        case ThreadToDo::wait:
            ImGui::Text("%-32s   %-16s   0x%08X    ",
                th_state->name,
                "Waiting",
                th_state->stack.get()->get());
            break;
        case ThreadToDo::exit:
            ImGui::Text("%-32s   %-16s   0x%08X    ",
                th_state->name,
                "Exiting",
                th_state->stack.get()->get());
            break;
        }
    }
    ImGui::End();
}
