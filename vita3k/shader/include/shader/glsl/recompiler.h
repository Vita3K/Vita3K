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

#pragma once

#include <map>
#include <vector>

struct SceGxmProgram;

namespace shader::usse::glsl {
using SamplerMap = std::map<int, std::pair<std::string, bool>>;

struct VarToReg {
    std::string var_name;
    int offset;
    int location;
    int comp_count;
    int data_type;
    bool reg_format;
    bool builtin;
};

struct NonDependentTextureQueryInfo {
    int sampler_index;
    int coord_index;
    int proj_pos;
    std::string sampler_name;
    bool sampler_cube;

    int data_type;
    int offset_in_pa;
};

using NonDependentTextureQueries = std::vector<NonDependentTextureQueryInfo>;

struct ProgramState {
    SamplerMap samplers_on_offset;
    NonDependentTextureQueries non_dependent_queries;
    const SceGxmProgram &actual_program;
    bool should_generate_vld_func;

    explicit ProgramState(const SceGxmProgram &actual_program)
        : actual_program(actual_program)
        , should_generate_vld_func(false) {
    }
};
} // namespace shader::usse::glsl
