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
#include <host/state.h>
#include <imgui.h>

void DrawMainMenuBar(HostState &host) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Threads", nullptr, host.gui.threads_dialog)) {
                host.gui.threads_dialog = !host.gui.threads_dialog;
            }
            if (ImGui::MenuItem("Semaphores", nullptr, host.gui.semaphores_dialog)) {
                host.gui.semaphores_dialog = !host.gui.semaphores_dialog;
            }
            if (ImGui::MenuItem("Mutexes", nullptr, host.gui.mutexes_dialog)) {
                host.gui.mutexes_dialog = !host.gui.mutexes_dialog;
            }
            if (ImGui::MenuItem("Lightweight Mutexes", nullptr, host.gui.lwmutexes_dialog)) {
                host.gui.lwmutexes_dialog = !host.gui.lwmutexes_dialog;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
