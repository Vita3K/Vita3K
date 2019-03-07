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

#include "private.h"

#include <host/state.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>
#include <util/resource.h>

namespace gui {

void draw_semaphores_dialog(HostState &host) {
    ImGui::Begin("Semaphores", &host.gui.debug_menu.semaphores_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-16s   %-16s", "ID", "Semaphore Name", "Status", "Locked Threads");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (auto semaphore : host.kernel.semaphores) {
        std::shared_ptr<Semaphore> sema_state = semaphore.second;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d/%02d              %02zu",
            semaphore.first,
            sema_state->name,
            sema_state->val,
            sema_state->max,
            sema_state->waiting_threads.size());
    }
    ImGui::End();
}

} // namespace gui
