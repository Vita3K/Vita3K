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

#include <host/state.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>

namespace gui {

void draw_mutexes_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Mutexes", &gui.debug_menu.mutexes_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-7s   %-8s   %-16s   %-16s", "ID", "Mutex Name", "Status", "Attributes", "Waiting Threads", "Owner");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (const auto &mutex : host.kernel.mutexes) {
        std::shared_ptr<Mutex> mutex_state = mutex.second;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d        %01d            %02zu                 %s",
            mutex.first,
            mutex_state->name,
            mutex_state->lock_count,
            mutex_state->attr,
            mutex_state->waiting_threads->size(),
            mutex_state->owner == nullptr ? "not owned" : mutex_state->owner->name.c_str());
    }
    ImGui::End();
}

void draw_lw_mutexes_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Lightweight Mutexes", &gui.debug_menu.lwmutexes_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-7s   %-8s  %-16s   %-16s", "ID", "LwMutex Name", "Status", "Attributes", "Waiting Threads", "Owner");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (const auto &mutex : host.kernel.lwmutexes) {
        std::shared_ptr<Mutex> mutex_state = mutex.second;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d        %01d           %02zu                 %s",
            mutex.first,
            mutex_state->name,
            mutex_state->lock_count,
            mutex_state->attr,
            mutex_state->waiting_threads->size(),
            mutex_state->owner == nullptr ? "not owned" : mutex_state->owner->name.c_str());
    }
    ImGui::End();
}

} // namespace gui
