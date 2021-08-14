// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

namespace gui {

void draw_event_flags_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Event Flags", &gui.debug_menu.eventflags_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s  %-7s   %-8s   %-16s", "ID", "EventFlag Name", "Flags", "Attributes", "Waiting Threads");

    const std::lock_guard<std::mutex> lock(host.kernel.mutex);

    for (const auto &event : host.kernel.eventflags) {
        std::shared_ptr<EventFlag> event_state = event.second;
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s  %02d        %01d         %02zu                 ",
            event.first,
            event_state->name,
            event_state->flags,
            event_state->attr,
            event_state->waiting_threads->size());
    }
    ImGui::End();
}

} // namespace gui
