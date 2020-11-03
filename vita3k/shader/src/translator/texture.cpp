
// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_translator.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

using namespace shader;
using namespace usse;

spv::Id shader::usse::USSETranslatorVisitor::do_fetch_texture(const spv::Id tex, const Coord &coord, const DataType dest_type) {
    auto coord_id = coord.first;

    if (coord.second != static_cast<int>(DataType::F32)) {
        coord_id = m_b.createOp(spv::OpVectorExtractDynamic, type_f32, { m_b.createLoad(coord_id), m_b.makeIntConstant(0) });
        coord_id = utils::unpack_one(m_b, m_util_funcs, m_features, coord_id, static_cast<DataType>(coord.second));

        // Shuffle if number of components is larger than 2
        if (m_b.getNumComponents(coord_id) > 2) {
            coord_id = m_b.createOp(spv::OpVectorShuffle, m_b.makeVectorType(type_f32, 2), { coord_id, coord_id, 0, 1 });
        }
    }

    if (m_b.isPointer(coord_id)) {
        coord_id = m_b.createLoad(coord_id);
    }

    assert(m_b.getTypeClass(m_b.getContainedTypeId(m_b.getTypeId(coord_id))) == spv::OpTypeFloat);

    auto image_sample = m_b.createOp(spv::OpImageSampleImplicitLod, type_f32_v[4], { m_b.createLoad(tex), coord_id });

    if (dest_type == DataType::F16) {
        // Pack them
        spv::Id pack1 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 0, 1 });
        pack1 = utils::pack_one(m_b, m_util_funcs, m_features, pack1, DataType::F16);

        spv::Id pack2 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 2, 3 });
        pack2 = utils::pack_one(m_b, m_util_funcs, m_features, pack2, DataType::F16);

        image_sample = m_b.createCompositeConstruct(type_f32_v[2], { pack1, pack2 });
    }

    if (dest_type == DataType::UINT8) {
        image_sample = utils::convert_to_int(m_b, image_sample, DataType::UINT8, true);
        image_sample = utils::pack_one(m_b, m_util_funcs, m_features, image_sample, DataType::UINT8);
    }

    return image_sample;
}

void shader::usse::USSETranslatorVisitor::do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries) {
    Operand store_op;
    store_op.bank = RegisterBank::PRIMATTR;
    store_op.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    store_op.type = DataType::F32;

    for (auto &texture_query : texture_queries) {
        Imm4 dest_mask;
        switch (texture_query.store_type) {
        case (int)DataType::F16: {
            dest_mask = 0b11;
            break;
        }

        case (int)DataType::F32: {
            dest_mask = 0b1111;
            break;
        }
        case (int)DataType::UINT8: {
            dest_mask = 0b1;
            break;
        }
        default:
            assert(false);
        }
        spv::Id fetch_result = do_fetch_texture(texture_query.sampler, texture_query.coord, static_cast<DataType>(texture_query.store_type));

        store_op.num = texture_query.dest_offset;
        store(store_op, fetch_result, dest_mask);
    }
}

bool USSETranslatorVisitor::smp(
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
    // LOD mode: none, bias, replace, gradient
    if (lod_mode != 0 || dim + 1 != 2) {
        LOG_ERROR("Sampler LOD custom mode not implemented!");
        return true;
    }

    constexpr DataType tb_dest_fmt[] = {
        DataType::F32,
        DataType::UNK,
        DataType::F16,
        DataType::F32
    };

    // Decode dest
    Instruction inst;
    inst.opr.dest.bank = (dest_use_pa) ? RegisterBank::PRIMATTR : RegisterBank::TEMP;
    inst.opr.dest.num = dest_n;
    inst.opr.dest.type = tb_dest_fmt[fconv_type];

    // Decode src0
    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_ext, true, 7, m_second_program);
    inst.opr.src0.type = (src0_type == 0) ? DataType::F32 : ((src0_type == 1) ? DataType::F16 : DataType::C10);

    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, true, 7, m_second_program);

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    // Base 0, turn it to base 1
    dim += 1;

    LOG_DISASM("{:016x}: {}SMP{}d.{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), dim, disasm::data_type_str(inst.opr.dest.type), disasm::data_type_str(inst.opr.src0.type),
        disasm::operand_to_str(inst.opr.dest, 0b0001), disasm::operand_to_str(inst.opr.src0, 0b0011), disasm::operand_to_str(inst.opr.src1, 0b0000));

    m_b.setLine(m_recompiler.cur_pc);

    // Generate simple stuff
    // Load the coord
    const spv::Id coord = load(inst.opr.src0, 0b0011);

    if (coord == spv::NoResult) {
        LOG_ERROR("Coord not loaded");
        return false;
    }

    spv::Id sampler = spv::NoResult;
    if (m_spirv_params.samplers.count(inst.opr.src1.num)) {
        sampler = m_spirv_params.samplers.at(inst.opr.src1.num);
    } else {
        LOG_ERROR("Can't get the sampler (sampler doesn't exist!)");
        return true;
    }

    spv::Id result = do_fetch_texture(sampler, { coord, static_cast<int>(DataType::F32) }, DataType::F32);

    switch (sb_mode) {
    case 1: {
        store(inst.opr.dest, result, 0b1111);
        break;
    }

    case 3: {
        // TODO: figure out what to fill here
        //store(inst.opr.dest, stub, 0b1111);

        inst.opr.dest.num += 4;
        store(inst.opr.dest, result, 0b1111);
        break;
    }
    default: {
        LOG_ERROR("Unsupported sb_mode: {}", sb_mode);
    }
    }

    return true;
}
