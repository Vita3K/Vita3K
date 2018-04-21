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

void DrawMutexesDialog(HostState &host) {
    ImGui::Begin("Mutexes", &host.gui.mutexes_dialog);
    ImGui::TextColored(ImVec4(255, 255, 0, 255), "%-16s %-32s   %-16s   %-16s   %-16s", "ID", "Mutex Name", "Status", "Locked Threads", "Owner");
    for (auto mutex : host.kernel.mutexes) {
        std::shared_ptr<Mutex> mutex_state = mutex.second;
        ImGui::Text("0x%08X       %-32s   %02d                 %02u                 %s",
            mutex.first,
            mutex_state->name.c_str(),
            mutex_state->lock_count,
            mutex_state->locked.size(),
            mutex_state->owner == nullptr ? "not owned" : mutex_state->owner->name);
    }
    ImGui::End();
}
