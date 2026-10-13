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

#include <kernel/state.h>

namespace gui {

void draw_event_flags_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Event Flags", &gui.debug_menu.eventflags_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s  %-7s   %-8s   %-16s", "ID", "EventFlag Name", "Flags", "Attributes", "Waiting Threads");

    emuenv.kernel.objects.for_each<EventFlag>([](EventFlag &event_flag) {
        SceKernelEventFlagInfo info;
        if (event_flag.info(info) != SCE_KERNEL_OK)
            return;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s  %02u        %01d         %02u                 ",
            event_flag.uid,
            event_flag.name.c_str(),
            info.currentPattern,
            event_flag.attr,
            info.numWaitThreads);
    });
    ImGui::End();
}

} // namespace gui
