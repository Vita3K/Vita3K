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

#include <cpu/functions.h>
#include <gui/functions.h>
#include <host/state.h>
#include <kernel/state.h>

#include <spdlog/fmt/fmt.h>

namespace gui {

static void evaluate_code(GuiState &gui, HostState &host, uint32_t from, uint32_t count, bool thumb) {
    gui.disassembly.clear();

    if (host.kernel->threads.empty()) {
        gui.disassembly.emplace_back("Nothing to disassemble.");
        return;
    }

    uint16_t size = 1;
    uint32_t addr = from;

    for (std::uint32_t a = 0; a < count && size != 0; a++) {
        // Apparently some THUMB instructions are 4 bytes long, so check all 4 just to be safe.
        size_t addr_page = addr / KB(4);
        size_t end_page = (addr + 4) / KB(4);

        if (addr_page == 0 || host.mem.allocated_pages[addr_page] == 0 || host.mem.allocated_pages[end_page] == 0) {
            gui.disassembly.emplace_back(fmt::format("Disassembled {} instructions.", a));
            break;
        }

        // Use DisasmState for first thread.
        std::string disasm = fmt::format("{:0>8X}: {}",
            addr, disassemble(*host.kernel->threads.begin()->second->cpu.get(), addr, thumb, &size));
        gui.disassembly.emplace_back(disasm);
        addr += size;
    }
}

void reevaluate_code(GuiState &gui, HostState &host) {
    std::string address_string = std::string(gui.disassembly_address);
    std::string count_string = std::string(gui.disassembly_count);

    uint32_t address = 0, count = 0;
    if (!address_string.empty())
        address = static_cast<uint32_t>(std::stol(address_string, nullptr, 16));
    if (!count_string.empty())
        count = static_cast<uint32_t>(std::stol(count_string));
    bool thumb = gui.disassembly_arch == "THUMB";

    evaluate_code(gui, host, address, count, thumb);
}

std::string archs[] = {
    "ARM",
    "THUMB",
};

void draw_disassembly_dialog(GuiState &gui, HostState &host) {
    ImGui::Begin("Disassembly", &gui.debug_menu.disassembly_dialog);
    ImGui::BeginChild("disasm", ImVec2(0, -(ImGui::GetTextLineHeightWithSpacing() + 10)));
    for (const std::string &assembly : gui.disassembly) {
        ImGui::Text("%s", assembly.c_str());
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("disasm_info", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + 10));

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::Text("Address");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
    ImGui::PushItemWidth(10 * 8);
    if (ImGui::InputText("##disasm_addr", gui.disassembly_address, 9,
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        reevaluate_code(gui, host);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::Text("Count");
    ImGui::SameLine();
    ImGui::PushItemWidth(10 * 4);
    if (ImGui::InputText("##disasm_count", gui.disassembly_count, 5,
            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        reevaluate_code(gui, host);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::Text("Arch");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##disasm_arch", gui.disassembly_arch.c_str())) {
        for (const std::string &arch : archs) {
            bool is_selected = gui.disassembly_arch == arch;
            if (ImGui::Selectable(arch.c_str(), is_selected)) {
                gui.disassembly_arch = arch;
                reevaluate_code(gui, host);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace gui
