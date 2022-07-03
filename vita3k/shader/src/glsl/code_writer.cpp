// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <shader/glsl/code_writer.h>

namespace shader::usse::glsl {
CodeWriter::CodeWriter()
    : active_body_type(BODY_TYPE_MAIN) {
    preload_indentation = "\t";
    postwork_indentation = "\t";
    for (std::size_t i = 0; i < BODY_TYPE_MAX; i++) {
        body_indentation[i] = "\t";
    }
}

void CodeWriter::add_declaration(const std::string &decl_line) {
    decl_content += decl_indentation + decl_line + "\n";
}

void CodeWriter::add_to_preload(const std::string &line) {
    preload_content += preload_indentation + line + "\n";
}

void CodeWriter::add_to_current_body(const std::string &line) {
    body_content[active_body_type] += body_indentation[active_body_type] + line + "\n";
}

void CodeWriter::indent_preload() {
    preload_indentation.push_back('\t');
}

void CodeWriter::dedent_preload() {
    if (preload_indentation.length() > 1) {
        preload_indentation.pop_back();
    }
}

void CodeWriter::indent_declaration() {
    decl_indentation.push_back('\t');
}

void CodeWriter::dedent_declaration() {
    decl_indentation.pop_back();
}

void CodeWriter::indent_current_body() {
    body_indentation[active_body_type].push_back('\t');
}

void CodeWriter::dedent_current_body() {
    if (body_indentation[active_body_type].length() > 1) {
        body_indentation[active_body_type].pop_back();
    }
}

void CodeWriter::set_active_body_type(const CodeBodyType type) {
    active_body_type = type;
}

std::string CodeWriter::assemble() {
    std::string adjusted_main = std::string("void main() {\n") + preload_content;
    adjusted_main += body_content[BODY_TYPE_MAIN];
    if (!body_content[BODY_TYPE_SECONDARY_PROGRAM].empty()) {
        adjusted_main += "\tsecondary_program();\n";
    }
    adjusted_main += "\tprimary_program();\n";
    adjusted_main += body_content[BODY_TYPE_POSTWORK];
    adjusted_main += "}\n";

    std::string adjusted_secondary;

    if (!body_content[BODY_TYPE_SECONDARY_PROGRAM].empty()) {
        adjusted_secondary = std::string("void secondary_program() {\n") + body_content[BODY_TYPE_SECONDARY_PROGRAM] + "}\n";
    }

    std::string adjusted_primary = std::string("void primary_program() {\n") + body_content[BODY_TYPE_PRIMARY_PROGRAM] + "}\n";
    return decl_content + "\n" + adjusted_secondary + "\n" + adjusted_primary + "\n" + adjusted_main;
}
} // namespace shader::usse::glsl