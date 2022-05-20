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

#pragma once

#include <string>

struct FeatureState;

namespace shader::usse::glsl {
enum CodeBodyType {
    BODY_TYPE_MAIN,
    BODY_TYPE_SECONDARY_PROGRAM,
    BODY_TYPE_PRIMARY_PROGRAM,
    BODY_TYPE_POSTWORK, // Workaround for param loader
    BODY_TYPE_MAX
};

class CodeWriter {
private:
    std::string body_indentation[BODY_TYPE_MAX];
    std::string preload_indentation;
    std::string postwork_indentation;
    std::string decl_indentation;

    std::string decl_content;
    std::string preload_content;
    std::string body_content[BODY_TYPE_MAX];
    std::string post_content;

    CodeBodyType active_body_type;

public:
    explicit CodeWriter();

    void add_declaration(const std::string &decl_line);
    void add_to_preload(const std::string &line);
    void add_to_current_body(const std::string &line);
    void indent_preload();
    void dedent_preload();
    void indent_current_body();
    void dedent_current_body();
    void indent_declaration();
    void dedent_declaration();
    void set_active_body_type(const CodeBodyType type);

    std::string assemble();
};
} // namespace shader::usse::glsl
