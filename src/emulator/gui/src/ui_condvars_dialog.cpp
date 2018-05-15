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
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>
#include <util/resource.h>

void DrawCondvarsDialog(HostState &host) {
    ImGui::Begin("Condition Variables", &host.gui.condvars_dialog);
    ImGui::TextColored(ImVec4(255, 255, 0, 255), "%-16s %-32s   %-16s %-16s", "ID", "Name", "Attributes", "Waiting Threads");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (auto condvar : host.kernel.condvars) {
        std::shared_ptr<Condvar> sema_state = condvar.second;
        ImGui::Text("0x%08X       %-32s   %02d             %02u",
            condvar.first,
            sema_state->name,
            sema_state->attr,
            sema_state->waiting_threads.size());
    }
    ImGui::End();
}

void DrawLwCondvarsDialog(HostState &host) {
    ImGui::Begin("Lightweight Condition Variables", &host.gui.lwcondvars_dialog);
    ImGui::TextColored(ImVec4(255, 255, 0, 255), "%-16s %-32s   %-16s %-16s", "ID", "Name", "Attributes", "Waiting Threads");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (auto condvar : host.kernel.lwcondvars) {
        std::shared_ptr<Condvar> sema_state = condvar.second;
        ImGui::Text("0x%08X       %-32s   %02d             %02u",
            condvar.first,
            sema_state->name,
            sema_state->attr,
            sema_state->waiting_threads.size());
    }
    ImGui::End();
}
