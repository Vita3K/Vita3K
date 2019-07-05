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
#include <crypto/hash.h>
#include <gxm/types.h>
#include <host/state.h>
#include <renderer/state.h>
#include <shader/spirv_recompiler.h>

#include <imgui_memory_editor.h>
#include <spdlog/fmt/fmt.h>

#include <array>
#include <string>
#include <vector>

#define EXTERNAL_DISASM_ENABLE 0
#define EXTERNAL_DISASM_PATH ""
#define EXTERNAL_DISASM_ARGS ""

namespace gui {

void draw_shader_editor_dialog(GuiState &gui, HostState &host) {
    static int selected_vertex_shader = 0;
    static int selected_frag_shader = 0;
    static emu::SceGxmProgramType selected_shader_type = emu::SceGxmProgramType::Vertex;

    ImGui::Begin("Shader Editor", &gui.debug_menu.shader_editor_dialog);

    std::vector<std::string> vertex_shader_names;
    std::vector<std::pair<Sha256Hash, std::string>> vertex_shader_data;
    for (auto s : host.renderer.vertex_glsl_cache) {
        vertex_shader_names.push_back(hex(s.first).data());
        vertex_shader_data.emplace_back(s.first, s.second);
    }

    std::vector<std::string> frag_shader_names;
    std::vector<std::pair<Sha256Hash, std::string>> frag_shader_data;
    for (auto s : host.renderer.fragment_glsl_cache) {
        frag_shader_names.push_back(hex(s.first).data());
        frag_shader_data.emplace_back(s.first, s.second);
    }

    ImGui::PushItemWidth(150);
    ImGui::Combo("##vtx_shad_sel", &selected_vertex_shader, ImGui::vector_getter, (void *)&vertex_shader_names, host.renderer.vertex_glsl_cache.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load Vertex Shader"))
        selected_shader_type = emu::SceGxmProgramType::Vertex;

    ImGui::PushItemWidth(150);
    ImGui::Combo("##frg_shad_sel", &selected_frag_shader, ImGui::vector_getter, (void *)&frag_shader_names, host.renderer.fragment_glsl_cache.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load Fragment Shader"))
        selected_shader_type = emu::SceGxmProgramType::Fragment;

    std::pair<Sha256Hash, std::string> selected_shader;

    if (selected_shader_type == emu::SceGxmProgramType::Vertex) {
        if (vertex_shader_data.size() <= selected_vertex_shader) {
            ImGui::End();
            return;
        }
        selected_shader = vertex_shader_data[selected_vertex_shader];
    } else if (selected_shader_type == emu::SceGxmProgramType::Fragment) {
        if (frag_shader_data.size() <= selected_frag_shader) {
            ImGui::End();
            return;
        }
        selected_shader = frag_shader_data[selected_frag_shader];
    }

    const auto hash = selected_shader.first;
    const auto src = selected_shader.second;

    const auto gxp_map_entry = host.renderer.gxp_ptr_map.find(hash);

    if (gxp_map_entry != std::end(host.renderer.gxp_ptr_map)) {
        std::string spirv_dump;
        std::string disasm_dump;

        const auto gxp_program = gxp_map_entry->second;
        shader::convert_gxp_to_glsl(*gxp_program, "", false, &spirv_dump, &disasm_dump);

        gui.gxp_shader_editor.DrawWindow("GXP Shader Editor", (void *)gxp_program, gxp_program->size);

        ImGui::Begin("Disassembly");
        ImGui::InputTextMultiline("Disassembly", const_cast<char *>(disasm_dump.data()), disasm_dump.size(), ImGui::GetWindowSize());
        ImGui::End();

        ImGui::Begin("SPIR-V");
        ImGui::InputTextMultiline("SPIR-V Output", const_cast<char *>(spirv_dump.data()), spirv_dump.size(), ImGui::GetWindowSize());
        ImGui::End();

        ImGui::Begin("GLSL");
        ImGui::InputTextMultiline("", const_cast<char *>(src.data()), src.size(), ImGui::GetWindowSize());
        ImGui::End();

#if EXTERNAL_DISASM_ENABLE
        ImGui::Begin("External Disassembler");
        const auto temp_gxp_name = ".temp.gxp";
        std::ofstream gxp_temp(temp_gxp_name, std::ios::out | std::ios::binary | std::ios::trunc);
        gxp_temp.write((char *)gxp_program, gxp_program->size);
        gxp_temp.close();
        const auto exec_cmd = fmt::format("{} {} {}", EXTERNAL_DISASM_PATH, temp_gxp_name, EXTERNAL_DISASM_ARGS);
        const auto exec_out = util::exec<KB(8)>(exec_cmd);
        ImGui::InputTextMultiline("", const_cast<char *>(exec_out.data()), exec_out.size(), ImGui::GetWindowSize());
        std::remove(temp_gxp_name);
#endif

        ImGui::End();
    }
    ImGui::SameLine();

    ImGui::End();
}

} // namespace gui
