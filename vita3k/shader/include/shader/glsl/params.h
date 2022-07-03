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
#include <queue>
#include <shader/types.h>
#include <stack>

namespace shader::usse::glsl {
class CodeWriter;

struct ShaderVariableVec4Info {
    // Each int represents the previous type that has not been flushed. If the new dest type is different, flush
    // and reassign!
    int dirty[4];
    bool inited = false;

    enum DirtyType {
        DIRTY_TYPE_U8 = 0,
        DIRTY_TYPE_F16 = 1,
        DIRTY_TYPE_F32 = 2,
        DIRTY_TYPE_S8 = 3,
        DIRTY_TYPE_U16 = 4,
        DIRTY_TYPE_S16 = 5,
        DIRTY_TYPE_MAX = 6
    };

    enum DeclaredType {
        DECLARED_FLOAT4 = 1 << 0,
        DECLARED_HALF4_N1 = 1 << 1,
        DECLARED_HALF4_N2 = 1 << 2,
        DECLARED_U8_N1 = 1 << 3,
        DECLARED_U8_N2 = 1 << 4,
        DECLARED_U8_N3 = 1 << 5,
        DECLARED_U8_N4 = 1 << 6,
        DECLARED_S8_N1 = 1 << 7,
        DECLARED_S8_N2 = 1 << 8,
        DECLARED_S8_N3 = 1 << 9,
        DECLARED_S8_N4 = 1 << 10,
        DECLARED_U8_SHIFT_START = 3,
        DECLARED_S8_SHIFT_START = 7,
        DECLARED_U16_N1 = 1 << 11,
        DECLARED_U16_N2 = 1 << 12,
        DECLARED_S16_N1 = 1 << 13,
        DECLARED_S16_N2 = 1 << 14
    };

    std::uint32_t declared = 0;
    explicit ShaderVariableVec4Info();
};

struct RestorationInfo {
    ShaderVariableVec4Info &info;
    int component;
    int original_type;
    int base_current;
    std::uint32_t decl;
};

struct ShaderVariableBank {
    std::map<int, ShaderVariableVec4Info> variables;
    std::string prefix;

    explicit ShaderVariableBank(const std::string &prefix)
        : prefix(prefix) {
    }
};

struct ShaderVariables {
private:
    ShaderVariableBank pa_bank;
    ShaderVariableBank sa_bank;
    ShaderVariableBank internal_bank;
    ShaderVariableBank temp_bank;
    ShaderVariableBank output_bank;

    bool index_variables[2];
    bool should_gen_indexing_lookup[4]; // TEMP, OUTPUT, PA, SA
    bool should_gen_vecint_bitcast[4];
    bool should_gen_vecfloat_bitcast[6];
    bool should_gen_clamp16;

    CodeWriter &writer;
    const ProgramInput &program_input;
    bool is_vertex;

    ShaderVariableBank &get_load_bank(const RegisterBank bank);

    void do_restore(std::queue<RestorationInfo> &infos, const std::string &prefix);
    void apply_modifiers(const std::uint32_t flags, std::string &result);
    void prepare_variables(ShaderVariableBank &bank, const DataType dt, const RegisterBank bank_type, const int index, const int shift, const Swizzle4 &swizz,
        const std::uint8_t dest_mask, const bool for_write, std::string &output_prefix, int &real_load_index, bool &load_crossing_to_another_vector);
    void flush_bank(ShaderVariableBank &bank);

public:
    enum GenPackUnpackKind {
        GEN_PACK_U8,
        GEN_PACK_S8,
        GEN_UNPACK_U8,
        GEN_UNPACK_S8,
        GEN_PACK_F16,
        GEN_UNPACK_F16,
        GEN_PACK_U16,
        GEN_PACK_S16,
        GEN_UNPACK_S16,
        GEN_UNPACK_U16,
        GEN_MAX
    };

    bool should_gen_pack_unpack[GEN_MAX];
    explicit ShaderVariables(CodeWriter &writer, const ProgramInput &input, const bool is_vertex);

    std::string load(const Operand &op, const std::uint8_t dest_mask, const int shift_offset);
    void store(const Operand &dest, const std::string &rhs, const std::uint8_t dest_mask, const int shift_offset, const bool prepare_store = false);
    void generate_helper_functions();
    void flush();
    void mark_f32_raw_dirty(const RegisterBank bank, const int offset);
};
} // namespace shader::usse::glsl
