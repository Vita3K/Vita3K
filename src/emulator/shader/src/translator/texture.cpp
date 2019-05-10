
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

#include <shader/usse_translator.h>
#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

using namespace shader;
using namespace usse;

spv::Id shader::usse::USSETranslatorVisitor::do_fetch_texture(const spv::Id tex, const spv::Id coord, const DataType dest_type) {
    auto image_sample = m_b.createOp(spv::OpImageSampleImplicitLod, type_f32_v[4], { tex, coord });

    if (dest_type == DataType::F16) {
        // Pack them
        spv::Id pack1 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 0, 1 });
        pack1 = pack_one(pack1, DataType::F16);

        spv::Id pack2 = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { image_sample, image_sample, 2, 3 });
        pack2 = pack_one(pack2, DataType::F16);

        image_sample = m_b.createCompositeConstruct(type_f32_v[2], { pack1, pack2 });
    }

    return image_sample;
}

void shader::usse::USSETranslatorVisitor::do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries) {
    for (auto &texture_query : texture_queries) {
        m_b.createStore(do_fetch_texture(texture_query.sampler, texture_query.coord, static_cast<DataType>(texture_query.store_type)), texture_query.dest);
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
    Imm7 src2_n)
{
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
    Instruction inst{};
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

    // Try to get the sampler
    auto sampler_bank = get_reg_bank(inst.opr.src1.bank);

    if (!sampler_bank) {
        LOG_ERROR("Can't get the sampler (sampler bank doesn't exist!)");
        return true;
    }

    usse::SpirvReg sampler;
    std::uint32_t temp_comp = 0;
    if (!sampler_bank->find_reg_at(inst.opr.src1.num, sampler, temp_comp) || temp_comp != 0) {
        LOG_ERROR("Can't get the sampler (sampler doesn't exist!)");
        return true;
    }

    spv::Id result = do_fetch_texture(sampler.var_id, coord, inst.opr.dest.type);
    store(inst.opr.dest, result, 0b1111);

    return true;
}
