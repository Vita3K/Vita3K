#include <gui/functions.h>
#include <gui/gui_constants.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <cpu/functions.h>
#include <host/state.h>

#include <spdlog/fmt/fmt.h>

void EvaluateCode(HostState &host, uint32_t from, uint32_t count, bool thumb) {
    host.gui.disassembly.clear();

    if (host.kernel.threads.empty()) {
        host.gui.disassembly.emplace_back("Nothing to disassemble.");
        return;
    }

    uint16_t size = 1;
    uint32_t addr = from;

    for (int a = 0; a < count && size != 0; a++) {
        // Apparently some THUMB instructions are 4 bytes long, so check all 4 just to be safe.
        size_t addr_page = addr / KB(4);
        size_t end_page = (addr + 4) / KB(4);

        if (addr_page == 0 || host.mem.allocated_pages[addr_page] == 0 || host.mem.allocated_pages[end_page] == 0) {
            host.gui.disassembly.emplace_back(fmt::format("Disassembled {} instructions.", a));
            break;
        }

        // Use DisasmState for first thread.
        std::string disasm = fmt::format("{:0>8X}: {}",
            addr, disassemble(*host.kernel.threads.begin()->second->cpu.get(), addr, thumb, &size));
        host.gui.disassembly.emplace_back(disasm);
        addr += size;
    }
}

void ReevaluateCode(HostState &host) {
    std::string address_string = std::string(host.gui.disassembly_address);
    std::string count_string = std::string(host.gui.disassembly_count);

    uint32_t address = 0, count = 0;
    if (!address_string.empty())
        address = static_cast<uint32_t>(std::stol(address_string, nullptr, 16));
    if (!count_string.empty())
        count = static_cast<uint32_t>(std::stol(count_string));
    bool thumb = host.gui.disassembly_arch == "THUMB";

    EvaluateCode(host, address, count, thumb);
}

std::string archs[] = {
    "ARM",
    "THUMB",
};

void DrawDisassemblyDialog(HostState &host) {
    ImGui::Begin("Disassembly", &host.gui.disassembly_dialog);
    ImGui::BeginChild("disasm", ImVec2(0, -(ImGui::GetTextLineHeightWithSpacing() + 10)));
    for (const std::string &assembly : host.gui.disassembly) {
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
    if (ImGui::InputText("##disasm_addr", host.gui.disassembly_address, 9,
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        ReevaluateCode(host);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::Text("Count");
    ImGui::SameLine();
    ImGui::PushItemWidth(10 * 4);
    if (ImGui::InputText("##disasm_count", host.gui.disassembly_count, 5,
            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        ReevaluateCode(host);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::Text("Arch");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##disasm_arch", host.gui.disassembly_arch.c_str())) {
        for (const std::string &arch : archs) {
            bool is_selected = host.gui.disassembly_arch == arch;
            if (ImGui::Selectable(arch.c_str(), is_selected)) {
                host.gui.disassembly_arch = arch;
                ReevaluateCode(host);
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
