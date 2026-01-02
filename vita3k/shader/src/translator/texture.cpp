
// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <shader/spirv_recompiler.h>
#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_translator.h>
#include <shader/usse_types.h>

#include <gxm/functions.h>
#include <util/log.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

using namespace shader;
using namespace usse;

// Return the uv coefficients (a v4f32), each in [0,1]
// coords are the normalized coordinates in the sampled image
static spv::Id get_uv_coeffs(spv::Builder &b, const spv::Id std_builtins, spv::Id sampled_image, spv::Id coords, spv::Id lod = spv::NoResult) {
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures-texel-filtering
    const spv::Id f32 = b.makeFloatType(32);
    const spv::Id v2f32 = b.makeVectorType(f32, 2);
    const spv::Id i32 = b.makeIntType(32);
    const spv::Id v2i32 = b.makeVectorType(i32, 2);

    if (lod == spv::NoResult) {
        // compute the lod here
        const spv::Id query_lod = b.createOp(spv::OpImageQueryLod, v2f32, { sampled_image, coords });
        lod = b.createOp(spv::OpVectorExtractDynamic, f32, { query_lod, b.makeIntConstant(0) });
    }

    const spv::Id layer = b.createUnaryOp(spv::OpConvertFToS, i32, lod);

    // first we need to get the image size
    const spv::Id image_type = b.makeImageType(f32, spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
    const spv::Id image = b.createUnaryOp(spv::OpImage, image_type, sampled_image);
    spv::Id image_size = b.createOp(spv::OpImageQuerySizeLod, v2i32, { image, layer });
    image_size = b.createUnaryOp(spv::OpConvertSToF, v2f32, image_size);

    // un-normalize the coordinates
    coords = b.createBinOp(spv::OpFMul, v2f32, coords, image_size);

    // subtract 0.5 to each coord
    const spv::Id half = b.makeFloatConstant(0.5f);
    const spv::Id v2half = b.makeCompositeConstant(v2f32, { half, half });
    coords = b.createBinOp(spv::OpFSub, v2f32, coords, v2half);

    // the uv coefficients are the fractional values
    return b.setPrecision(b.createBuiltinCall(v2f32, std_builtins, GLSLstd450Fract, { coords }), spv::DecorationRelaxedPrecision);
}

spv::Id shader::usse::USSETranslatorVisitor::do_fetch_texture(const spv::Id tex, int texture_index, const int dim, const Coord &coord, const DataType dest_type, const int lod_mode, const spv::Id extra1, const spv::Id extra2, const int gather4_comp) {
    auto coord_id = coord.first;

    if (coord.second != static_cast<int>(DataType::F32)) {
        coord_id = m_b.createOp(spv::OpVectorExtractDynamic, type_f32, { m_b.createLoad(coord_id, spv::NoPrecision), m_b.makeIntConstant(0) });
        coord_id = utils::unpack_one(m_b, m_util_funcs, m_features, coord_id, static_cast<DataType>(coord.second));

        // Shuffle if number of components is larger than 2
        if (m_b.getNumComponents(coord_id) > 2) {
            coord_id = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { { true, coord_id }, { true, coord_id }, { false, 0 }, { false, 1 } });
        }
    }

    if (m_b.isPointer(coord_id)) {
        coord_id = m_b.createLoad(coord_id, spv::NoPrecision);
    }

    assert(m_b.getTypeClass(m_b.getContainedTypeId(m_b.getTypeId(coord_id))) == spv::OpTypeFloat);

    // the texture viewport is only useful for surfaces and they are never cubes
    // also for the time being ignore sampleProj ops
    if (m_features.use_texture_viewport && dim == 2) {
        // coord = coord * viewport_ratio + viewport_offset
        spv::Id viewport_ratio = utils::create_access_chain(m_b, spv::StorageClassUniform, m_spirv_params.render_info_id, { m_b.makeIntConstant(m_spirv_params.viewport_ratio_id), m_b.makeIntConstant(texture_index) });
        viewport_ratio = m_b.createLoad(viewport_ratio, spv::NoPrecision);
        spv::Id viewport_offset = utils::create_access_chain(m_b, spv::StorageClassUniform, m_spirv_params.render_info_id, { m_b.makeIntConstant(m_spirv_params.viewport_offset_id), m_b.makeIntConstant(texture_index) });
        viewport_offset = m_b.createLoad(viewport_offset, spv::NoPrecision);

        if (extra1 != spv::NoResult || lod_mode != 4) {
            // only keep the first two coordinates (x,y)
            coord_id = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { { true, coord_id }, { true, coord_id }, { false, 0 }, { false, 1 } });
            coord_id = m_b.createBuiltinCall(m_b.getTypeId(coord_id), std_builtins, GLSLstd450Fma, { coord_id, viewport_ratio, viewport_offset });
        } else {
            // extract the x,y and proj coordinate
            spv::Id coord_xy = m_b.createOp(spv::OpVectorShuffle, type_f32_v[2], { { true, coord_id }, { true, coord_id }, { false, 0 }, { false, 1 } });
            spv::Id third_comp = m_b.createBinOp(spv::OpVectorExtractDynamic, type_f32, coord_id, m_b.makeIntConstant(2));
            third_comp = m_b.createCompositeConstruct(type_f32_v[2], { third_comp, third_comp });

            // multiply the offset by the third component
            viewport_offset = m_b.createBinOp(spv::OpFMul, type_f32_v[2], viewport_offset, third_comp);

            // do the fma
            coord_xy = m_b.createBuiltinCall(m_b.getTypeId(coord_xy), std_builtins, GLSLstd450Fma, { coord_xy, viewport_ratio, viewport_offset });

            // add back the proj component
            coord_id = m_b.createOp(spv::OpVectorShuffle, type_f32_v[3], { { true, coord_xy }, { true, coord_id }, { false, 0 }, { false, 1 }, { false, 4 } });
        }
    }

    spv::Id image_sample = spv::NoResult;
    spv::Op op;
    std::vector<spv::Id> params = { tex, coord_id };
    if (gather4_comp != -1) {
        // The gpx support gather with a grad but I don't think this is possible with spirV
        op = spv::OpImageGather;
        params.push_back(m_b.makeIntConstant(gather4_comp));
    } else if (extra1 == spv::NoResult) {
        if (lod_mode == 4)
            op = spv::OpImageSampleProjImplicitLod;
        else
            op = spv::OpImageSampleImplicitLod;
    } else {
        op = spv::OpImageSampleExplicitLod;
        switch (lod_mode) {
        case 1:
            op = spv::OpImageSampleImplicitLod;
            params.push_back(spv::ImageOperandsBiasMask);
            params.push_back(extra1);
            break;
        case 2:
            params.push_back(spv::ImageOperandsLodMask);
            params.push_back(extra1);
            break;
        case 3:
            params.push_back(spv::ImageOperandsGradMask);
            params.push_back(extra1);
            params.push_back(extra2);
            break;
        default:
            break;
        }
    }

    image_sample = m_b.createOp(op, type_f32_v[4], params);

    if (get_data_type_size(dest_type) < 4 && dest_type != DataType::UINT16 && dest_type != DataType::INT16)
        m_b.setPrecision(image_sample, spv::DecorationRelaxedPrecision);

    if (is_integer_data_type(dest_type))
        image_sample = utils::convert_to_int(m_b, m_util_funcs, image_sample, dest_type, true);

    return image_sample;
}

void shader::usse::USSETranslatorVisitor::do_texture_queries(const NonDependentTextureQueryCallInfos &texture_queries) {
    Operand store_op;
    store_op.bank = RegisterBank::PRIMATTR;
    store_op.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    for (auto &texture_query : texture_queries) {
        store_op.type = static_cast<DataType>(texture_query.store_type);
        if (store_op.type == DataType::UNK) {
            // get the type from the hint
            store_op.type = texture_query.component_type;
        }

        bool proj = (texture_query.prod_pos >= 0);
        shader::usse::Coord coord_inst = texture_query.coord;

        if (texture_query.prod_pos >= 0) {
            spv::Id texture_coord = m_b.createLoad(texture_query.coord.first, spv::NoPrecision);
            coord_inst.first = m_b.createOp(spv::OpVectorShuffle, type_f32_v[3], { { true, texture_coord }, { true, texture_coord }, { false, 0 }, { false, 1 }, { false, static_cast<uint32_t>(texture_query.prod_pos) } });
            proj = true;
        }

        spv::Id fetch_result = do_fetch_texture(m_b.createLoad(texture_query.sampler, spv::NoPrecision), texture_query.sampler_index, texture_query.dim, coord_inst, store_op.type, proj ? 4 : 0, 0);
        store_op.num = texture_query.dest_offset;

        const Imm4 mask = (1U << texture_query.component_count) - 1;

        store(store_op, fetch_result, mask);
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
    // Decode src0
    Instruction inst;
    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_ext, true, 8, m_second_program);
    inst.opr.src0.type = (src0_type == 0) ? DataType::F32 : ((src0_type == 1) ? DataType::F16 : DataType::C10);

    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, true, 8, m_second_program);

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    bool is_texture_buffer_load = false;
    // only used for a load using the texture buffer
    spv::Id texture_index = 0;
    if (!m_spirv_params.samplers.contains(inst.opr.src1.num)) {
        if (m_spirv_params.texture_buffer_sa_offset == -1) {
            LOG_ERROR("Can't get the sampler (sampler doesn't exist!)");
            return true;
        }

        if (sb_mode != 0 || lod_mode != 2) {
            LOG_ERROR("Unhandled load using texture buffer with sb mode {} and lod mode {}", sb_mode, lod_mode);
            return true;
        }

        is_texture_buffer_load = true;
        inst.opr.src1.type = DataType::INT32;
        texture_index = load(inst.opr.src1, 0b1);
    }

    // if this is a texture buffer load, just attribute the first available sampler to it
    const SamplerInfo &sampler = is_texture_buffer_load ? m_spirv_params.samplers.begin()->second : m_spirv_params.samplers.at(inst.opr.src1.num);

    constexpr DataType tb_dest_fmt[] = {
        DataType::F32,
        DataType::UNK,
        DataType::F16,
        DataType::F32
    };

    // Decode dest
    inst.opr.dest.bank = (dest_use_pa) ? RegisterBank::PRIMATTR : RegisterBank::TEMP;
    if (dest_use_pa && m_second_program)
        // PA can't be used in the secondary program
        inst.opr.dest.bank = RegisterBank::SECATTR;
    inst.opr.dest.num = dest_n;
    inst.opr.dest.type = tb_dest_fmt[fconv_type];

    if (inst.opr.dest.type == DataType::UNK)
        inst.opr.dest.type = sampler.component_type;

    // Base 0, turn it to base 1
    dim += 1;

    spv::Id coord_mask = 0b0011;
    if (dim == 3) {
        coord_mask = 0b0111;
    } else if (dim == 1) {
        coord_mask = 0b0001;
    }

    std::string additional_info;
    switch (sb_mode) {
    case 1:
        additional_info = ".gather4";
        break;
    case 2:
        additional_info = ".info";
        break;
    case 3:
        additional_info = ".gather4.uv";
        break;
    default:
        additional_info.clear();
    }

    LOG_DISASM("{:016x}: {}SMP{}d.{}.{}{} {} {} {} {}", m_instr, disasm::e_predicate_str(pred), dim, disasm::data_type_str(inst.opr.dest.type), disasm::data_type_str(inst.opr.src0.type), additional_info,
        disasm::operand_to_str(inst.opr.dest, 0b0001), disasm::operand_to_str(inst.opr.src0, coord_mask), disasm::operand_to_str(inst.opr.src1, 0b0000), (lod_mode == 0) ? "" : disasm::operand_to_str(inst.opr.src2, 0b0001));

    m_b.setDebugSourceLocation(m_recompiler.cur_pc, nullptr);

    // Generate simple stuff
    // Load the coord
    spv::Id coords = load(inst.opr.src0, coord_mask);

    if (coords == spv::NoResult) {
        LOG_ERROR("Coord not loaded");
        return false;
    }

    if (dim == 1) {
        // It should be a line, so Y should be zero. There are only two dimensions texture, so this is a guess (seems concise)
        coords = m_b.createCompositeConstruct(m_b.makeVectorType(m_b.makeFloatType(32), 2), { coords, m_b.makeFloatConstant(0.0f) });
        dim = 2;
    }

    spv::Id image_sampler = m_b.createLoad(sampler.id, spv::NoPrecision);

    if (sb_mode == 2) {
        if (lod_mode != 0)
            LOG_WARN("SMP info with non-zero lod mode is not implemented");

        const spv::Id v4u32 = m_b.makeVectorType(type_ui32, 4);

        // query info
        const spv::Id query_lod = m_b.createOp(spv::OpImageQueryLod, type_f32_v[2], { image_sampler, coords });
        const spv::Id lod = m_b.createBinOp(spv::OpVectorExtractDynamic, type_f32, query_lod, m_b.makeIntConstant(0));

        // xy are the uv coefficients
        spv::Id uv = get_uv_coeffs(m_b, std_builtins, image_sampler, coords, lod);
        // z is the trilinear fraction, w the LOD
        spv::Id tri_frac = m_b.createBuiltinCall(type_f32, std_builtins, GLSLstd450Fract, { lod });
        m_b.setPrecision(tri_frac, spv::DecorationRelaxedPrecision);
        const spv::Id lod_level = m_b.createUnaryOp(spv::OpConvertFToU, type_ui32, lod);
        m_b.setPrecision(lod_level, spv::DecorationRelaxedPrecision);

        // the result is stored as a vector of uint8, we must convert it
        uv = utils::convert_to_int(m_b, m_util_funcs, uv, DataType::UINT8, true);
        tri_frac = utils::convert_to_int(m_b, m_util_funcs, tri_frac, DataType::UINT8, true);

        const spv::Id u = m_b.createBinOp(spv::OpVectorExtractDynamic, type_ui32, uv, m_b.makeIntConstant(0));
        const spv::Id v = m_b.createBinOp(spv::OpVectorExtractDynamic, type_ui32, uv, m_b.makeIntConstant(1));

        const spv::Id result = m_b.createCompositeConstruct(v4u32, { u, v, tri_frac, lod_level });
        inst.opr.dest.type = DataType::UINT8;
        store(inst.opr.dest, result, 0b1111);
    } else {
        // Either LOD number or ddx
        spv::Id extra1 = spv::NoResult;
        // ddy
        spv::Id extra2 = spv::NoResult;

        // LOD mode: none, bias, replace, gradient
        if (lod_mode != 0) {
            inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, true, 8, m_second_program);
            inst.opr.src2.type = inst.opr.src0.type;

            switch (lod_mode) {
            case 1:
            case 2:
                extra1 = load(inst.opr.src2, 0b1);
                break;

            case 3:
                switch (dim) {
                case 2:
                    extra1 = load(inst.opr.src2, 0b0011);
                    extra2 = load(inst.opr.src2, 0b1100);
                    break;
                case 3:
                    extra1 = load(inst.opr.src2, 0b0111);
                    extra2 = load(inst.opr.src2, 0b0111, 1);
                }
                break;

            default:
                break;
            }
        }

        if (is_texture_buffer_load) {
            // maybe put this in a function instead

            // do a big switch with all the different textures:
            // switch(texture_idx) {
            // case 0:
            //   dest = texture(texture0, pos);
            //   break;
            // case 1:
            //   dest = texture(texture1, pos);
            //   break;
            // ....

            std::vector<const SamplerInfo *> samplers;
            std::vector<int> sampler_indices;
            std::vector<int> index_to_segment;
            constexpr int sa_count = 32 * 4;
            // if dim is 2, do not look for cubes and if dim is 3, only look for cubes
            const bool request_cube = dim == 3;
            for (auto &smp : m_spirv_params.samplers) {
                if (smp.first < sa_count)
                    continue;

                if (request_cube != smp.second.is_cube)
                    continue;

                samplers.push_back(&smp.second);
                index_to_segment.push_back(sampler_indices.size());
                sampler_indices.push_back(smp.first - sa_count);
            }

            std::vector<spv::Block *> segment_blocks;
            m_b.makeSwitch(texture_index, spv::SelectionControlMaskNone, samplers.size(), sampler_indices, index_to_segment, -1, segment_blocks);
            for (size_t s = 0; s < samplers.size(); s++) {
                const SamplerInfo *smp = samplers[s];

                m_b.nextSwitchSegment(segment_blocks, s);
                if (tb_dest_fmt[fconv_type] == DataType::UNK)
                    inst.opr.dest.type = smp->component_type;

                spv::Id result = do_fetch_texture(m_b.createLoad(smp->id, spv::NoPrecision), smp->index, dim, { coords, static_cast<int>(DataType::F32) }, inst.opr.dest.type, lod_mode, extra1);
                const Imm4 dest_mask = (1U << smp->component_count) - 1;
                store(inst.opr.dest, result, dest_mask);

                m_b.addSwitchBreak(true);
            }
            m_b.endSwitch(segment_blocks);
        } else if (sb_mode == 0) {
            spv::Id result = do_fetch_texture(image_sampler, sampler.index, dim, { coords, static_cast<int>(DataType::F32) }, inst.opr.dest.type, lod_mode, extra1, extra2);
            const Imm4 dest_mask = (1U << sampler.component_count) - 1;
            store(inst.opr.dest, result, dest_mask);
        } else {
            // sb_mode = 1 or 3 : gather 4 (+ uv if sb_mode = 3)
            // first gather all components
            std::vector<spv::Id> g4_comps;
            g4_comps.reserve(sampler.component_count);
            for (int comp = 0; comp < sampler.component_count; comp++) {
                g4_comps.push_back(do_fetch_texture(image_sampler, sampler.index, dim, { coords, static_cast<int>(DataType::F32) }, inst.opr.dest.type, lod_mode, extra1, extra2, comp));
            }

            if (sampler.component_count == 1) {
                // easy, no need to do all this reordering
                store(inst.opr.dest, g4_comps[0], 0b1111);
                inst.opr.dest.num += get_data_type_size(inst.opr.dest.type);
            } else {
                if (sampler.component_count == 3)
                    LOG_ERROR("Sampler is not supposed to have 3 components");

                // we have the values in the following layout x1 x2 ... y1 y2 ...
                // we want to store them with the layout x1 x2 x3 ... y1 y2 y3 ...
                const spv::Id comp_type = utils::unwrap_type(m_b, m_b.getTypeId(g4_comps[0]));

                std::vector<spv::Id> comps_alone;
                for (int pixel = 0; pixel < 4; pixel++) {
                    for (int comp = 0; comp < sampler.component_count; comp++) {
                        comps_alone.push_back(m_b.createBinOp(spv::OpVectorExtractDynamic, comp_type, g4_comps[comp], m_b.makeIntConstant(pixel)));
                    }
                }

                for (size_t idx = 0; idx < comps_alone.size(); idx += 4) {
                    // pack them by 4 so each pack size is a multiple of 32 bits
                    const spv::Id comp_packed = m_b.createCompositeConstruct(m_b.getTypeId(g4_comps[0]), { comps_alone[idx], comps_alone[idx + 1], comps_alone[idx + 2], comps_alone[idx + 3] });
                    store(inst.opr.dest, comp_packed, 0b1111);

                    inst.opr.dest.num += get_data_type_size(inst.opr.dest.type);
                }
            }

            if (sb_mode == 3) {
                // compute and save bilinear coefficients
                const spv::Id uv = get_uv_coeffs(m_b, std_builtins, image_sampler, coords);
                // the pixels returned by gather4 are in the following order : (0,1) (1,1) (1,0) (0,0)
                // so at the end we want (1-u)v uv u(1-v) (1-u)(1-v)
                // however, looking at the generated shader code in some games, it looks like the coefficients are
                // expected to be in this order but reversed...
                const spv::Id one = m_b.makeFloatConstant(1.0);
                const spv::Id u = m_b.createBinOp(spv::OpVectorExtractDynamic, type_f32, uv, m_b.makeIntConstant(0));
                const spv::Id v = m_b.createBinOp(spv::OpVectorExtractDynamic, type_f32, uv, m_b.makeIntConstant(1));

                const spv::Id onemu = m_b.createBinOp(spv::OpFSub, type_f32, one, u);
                m_b.setPrecision(onemu, spv::DecorationRelaxedPrecision);
                const spv::Id onemv = m_b.createBinOp(spv::OpFSub, type_f32, one, v);
                m_b.setPrecision(onemv, spv::DecorationRelaxedPrecision);

                // (1-u) u
                const spv::Id x_coeffs = m_b.createCompositeConstruct(type_f32_v[2], { onemu, u });
                // (1-u)v uv
                const spv::Id comp1 = m_b.createBinOp(spv::OpVectorTimesScalar, type_f32_v[2], x_coeffs, v);
                m_b.setPrecision(comp1, spv::DecorationRelaxedPrecision);
                // (1-u)(1-v) u(1-v)
                const spv::Id comp2 = m_b.createBinOp(spv::OpVectorTimesScalar, type_f32_v[2], x_coeffs, onemv);
                m_b.setPrecision(comp2, spv::DecorationRelaxedPrecision);
                // (1-u)v uv u(1-v) (1-u)(1-v) in reversed order
                const spv::Id coeffs = m_b.createOp(spv::OpVectorShuffle, type_f32_v[4], { { true, comp1 }, { true, comp2 }, { false, 2 }, { false, 3 }, { false, 1 }, { false, 0 } });

                // bilinear coeffs are stored as float16
                inst.opr.dest.type = DataType::F16;
                store(inst.opr.dest, coeffs, 0b1111);
            }
        }
    }

    return true;
}
