// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

void draw_condvars_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Condition Variables", &gui.debug_menu.condvars_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-16s %-16s", "ID", "Name", "Attributes", "Waiting Threads");

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, sema_state] : emuenv.kernel.condvars) {
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d             %02zu",
            id,
            sema_state->name,
            sema_state->attr,
            sema_state->waiting_threads->size());
    }
    ImGui::End();
}

void draw_lw_condvars_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::Begin("Lightweight Condition Variables", &gui.debug_menu.lwcondvars_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s %-32s   %-16s %-16s", "ID", "Name", "Attributes", "Waiting Threads");

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, sema_state] : emuenv.kernel.lwcondvars) {
        ImGui::TextColored(GUI_COLOR_TEXT, "0x%08X       %-32s   %02d             %02zu",
            id,
            sema_state->name,
            sema_state->attr,
            sema_state->waiting_threads->size());
    }
    ImGui::End();
}

} // namespace gui
