// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <emuenv/state.h>

#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

namespace gui {

// Returns the name of a mutex's owner thread, or its UID once the thread is gone
static std::string owner_name(KernelState &kernel, SceUID owner_id) {
    if (!owner_id)
        return "not owned";
    if (const ThreadStatePtr owner = kernel.get_thread(owner_id))
        return owner->name;
    return fmt::format("0x{:08X}", owner_id);
}

void draw_mutexes_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Mutexes", &gui.debug_menu.mutexes_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-7s   %-8s   %-16s   %-16s", "ID", "Mutex Name", "Status", "Attributes", "Waiting Threads", "Owner");

    emuenv.kernel.objects.for_each<HeavyMutex>([&](HeavyMutex &mutex) {
        SceKernelMutexInfo info;
        if (mutex.info(info) != SCE_KERNEL_OK)
            return;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02u        %01d            %02u                 %s",
            mutex.uid,
            mutex.name.c_str(),
            info.currentCount,
            mutex.attr,
            info.numWaitThreads,
            owner_name(emuenv.kernel, info.currentOwnerId).c_str());
    });
    ImGui::End();
}

void draw_lw_mutexes_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Lightweight Mutexes", &gui.debug_menu.lwmutexes_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-7s   %-8s  %-16s   %-16s", "ID", "LwMutex Name", "Status", "Attributes", "Waiting Threads", "Owner");

    emuenv.kernel.objects.for_each<LwMutex>([&](LwMutex &mutex) {
        SceKernelLwMutexInfo info;
        if (mutex.info(info) != SCE_KERNEL_OK)
            return;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d        %01d           %02u                 %s",
            mutex.uid,
            mutex.name.c_str(),
            info.currentCount,
            mutex.attr,
            info.numWaitThreads,
            owner_name(emuenv.kernel, info.currentOwnerId).c_str());
    });
    ImGui::End();
}

} // namespace gui
