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

#include <shader/disasm.h>
#include <shader/glsl/code_writer.h>
#include <shader/glsl/params.h>
#include <shader/glsl/translator.h>
#include <util/log.h>

using namespace shader;
using namespace usse;
using namespace glsl;

std::string USSETranslatorVisitorGLSL::do_fetch_texture(const std::string tex, std::string coord_name, const DataType dest_type, const int lod_mode,
    const std::string extra1, const std::string extra2) {
    std::string result;

    switch (lod_mode) {
    case 4:
        result = fmt::format("textureProj({}, {})", tex, coord_name);
        break;

    case 5:
        result = fmt::format("textureProjCube({}, {})", tex, coord_name);
        break;

    case 0:
    case 1:
        result = fmt::format("texture({}, {})", tex, coord_name);
        break;

    case 2:
        result = fmt::format("textureLod({}, {}, {})", tex, coord_name, extra1);
        break;

    case 3:
        result = fmt::format("textureGrad({}, {}, {}, {})", tex, coord_name, extra1, extra2);
        break;

    default:
        return "";
    }

    if (dest_type == DataType::UINT8) {
        result = fmt::format("uvec4({} * 255.0)", result);
    }

    return result;
}

void USSETranslatorVisitorGLSL::do_texture_queries(const NonDependentTextureQueries &texture_queries) {
    Operand store_op;
    store_op.bank = RegisterBank::PRIMATTR;
    store_op.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    store_op.type = DataType::F32;

    for (auto &texture_query : texture_queries) {
        bool proj = (texture_query.proj_pos >= 0);
        std::string coord_access = fmt::format("v_TexCoord{}", texture_query.coord_index);
        std::string proj_access = coord_access + ".";
        if (texture_query.sampler_cube) {
            coord_access += ".xyz";
        } else {
            coord_access += ".xy";
        }

        if (proj) {
            coord_access += static_cast<char>('w' + (texture_query.proj_pos + 1) % 4);
        }

        std::string fetch_result = do_fetch_texture(texture_query.sampler_name, coord_access, static_cast<DataType>(texture_query.data_type),
            proj ? (texture_query.sampler_cube ? 5 : 4) : 0, "", "");
        store_op.num = texture_query.offset_in_pa;
        store_op.type = static_cast<DataType>(texture_query.data_type);

        if (static_cast<DataType>(texture_query.data_type) == DataType::UNK) {
            // Manual check
            writer.add_to_current_body(fmt::format("temp4 = {};", fetch_result));

            // S8
            writer.add_to_current_body(fmt::format("if (renderFragInfo.integral_query_formats[{}][{}] > {}) {{", texture_query.sampler_index / 4,
                texture_query.sampler_index % 4, INTEGRAL_TEX_QUERY_TYPE_8BIT_SIGNED));
            writer.indent_current_body();

            store_op.type = DataType::INT8;
            variables.store(store_op, "ivec4(temp4 * 128.0)", 0b1111, 0, true);
            writer.dedent_current_body();

            // U8
            writer.add_to_current_body(fmt::format("}} else if (renderFragInfo.integral_query_formats[{}][{}] > {}) {{", texture_query.sampler_index / 4,
                texture_query.sampler_index % 4, INTEGRAL_TEX_QUERY_TYPE_8BIT_UNSIGNED));

            writer.indent_current_body();
            store_op.type = DataType::UINT8;
            variables.store(store_op, "uvec4(temp4 * 255.0)", 0b1111, 0, true);
            writer.dedent_current_body();

            // F16
            writer.add_to_current_body(fmt::format("}} else if (renderFragInfo.integral_query_formats[{}][{}] > {}) {{", texture_query.sampler_index / 4,
                texture_query.sampler_index % 4, INTEGRAL_TEX_QUERY_TYPE_16BIT));

            writer.indent_current_body();
            store_op.type = DataType::F16;
            variables.store(store_op, "temp4", 0b1111, 0, true);
            writer.dedent_current_body();

            // 32-bit
            writer.add_to_current_body(fmt::format("}} else {{", texture_query.sampler_index / 4, texture_query.sampler_index % 4));

            writer.indent_current_body();
            store_op.type = DataType::F32;
            variables.store(store_op, "temp4", 0b1111, 0, true);
            writer.dedent_current_body();

            writer.add_to_current_body("}");
        } else {
            variables.store(store_op, fetch_result, 0b1111, 0, true);
        }
    }
}

bool USSETranslatorVisitorGLSL::smp(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 nosched,
    Imm1 syncstart,
    Imm1 minpack,
    Imm1 src0_ext,
    Imm1 src1_ext,
    Imm1 src2_ext,
    Imm2 fconv_type,
    Imm2 mask_count,
    Imm2 dim,
    Imm2 lod_mode,
    bool dest_use_pa,
    Imm2 sb_mode,
    Imm2 src0_type,
    Imm1 src0_bank,
    Imm2 drc_sel,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::smp(pred, skipinv, nosched, syncstart, minpack, src0_ext, src2_ext, src2_ext, fconv_type,
            mask_count, dim, lod_mode, dest_use_pa, sb_mode, src0_type, src0_bank, drc_sel, src1_bank, src2_bank,
            dest_n, src0_n, src1_n, src2_n)) {
        return false;
    }

    // LOD mode: none, bias, replace, gradient
    if ((lod_mode != 0) && (lod_mode != 2) && (lod_mode != 3)) {
        LOG_ERROR("Sampler LOD replace not implemented!");
        return true;
    }

    // Base 0, turn it to base 1
    dim += 1;

    std::uint32_t coord_mask = 0b0011;
    if (dim == 3) {
        coord_mask = 0b0111;
    } else if (dim == 1) {
        coord_mask = 0b0001;
    }

    // Generate simple stuff
    // Load the coord
    std::string coord = variables.load(decoded_inst.opr.src0, coord_mask, 0);

    if (coord.empty()) {
        LOG_ERROR("Coord not loaded");
        return false;
    }

    if (dim == 1) {
        // It should be a line, so Y should be zero. There are only two dimensions texture, so this is a guess (seems concise)
        coord = fmt::format("vec2({}, 0)", coord);
    }

    std::string sampler_name;
    if (program_state.samplers_on_offset.count(decoded_inst.opr.src1.num)) {
        sampler_name = program_state.samplers_on_offset.at(decoded_inst.opr.src1.num).first;
    } else {
        LOG_ERROR("Can't get the sampler (sampler doesn't exist!)");
        return true;
    }

    // Either LOD number or gradient number
    std::string extra1;
    std::string extra2;

    if (lod_mode != 0) {
        switch (lod_mode) {
        case 2:
            extra1 = variables.load(decoded_inst.opr.src2, 0b1, 0);
            break;

        case 3:
            if (dim == 3) {
                extra1 = variables.load(decoded_inst.opr.src2, 0b111, 0);
            } else {
                extra1 = variables.load(decoded_inst.opr.src2, 0b11, 0);
            }

            // Skip to next PA/INTERNAL to get DDY. Observed on shader in game like Hatsune Miku Diva X
            if (dim == 3) {
                extra2 = variables.load(decoded_inst.opr.src2, 0b111, 1);
            } else {
                extra2 = variables.load(decoded_inst.opr.src2, 0b11, 2);
            }

            break;

        default:
            break;
        }
    }

    std::string result = do_fetch_texture(sampler_name, coord, DataType::F32, lod_mode, extra1, extra2);

    switch (sb_mode) {
    case 0:
    case 1:
        variables.store(decoded_inst.opr.dest, result, 0b1111, 0);
        break;
    case 3: {
        // TODO: figure out what to fill here
        // store(inst.opr.dest, stub, 0b1111);
        variables.store(decoded_inst.opr.dest, result, 0b1111, 0);
        break;
    }
    default: {
        LOG_ERROR("Unsupported sb_mode: {}", sb_mode);
    }
    }

    return true;
}
