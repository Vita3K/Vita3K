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

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/usse_constant_table.h>
#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <gxm/functions.h>

#include <bitset>

using namespace shader;
using namespace usse;

static spv::Id get_int_max_value_constant(spv::Builder &b, DataType type) {
    int max_value;
    switch (type) {
    case DataType::UINT8:
        max_value = 0xFF;
        break;
    case DataType::INT8:
        max_value = 0x7F;
        break;
    case DataType::UINT16:
        max_value = 0xFFFF;
        break;
    case DataType::INT16:
        max_value = 0x7FFF;
        break;
    case DataType::UINT32:
        max_value = 0xFFFFFFFF;
        break;
    case DataType::INT32:
        max_value = 0x7FFFFFFF;
        break;
    }
    return b.makeIntConstant(max_value);
}

spv::Id USSETranslatorVisitor::load(Operand op, const Imm4 dest_mask, const int shift_offset, bool is_integer) {
    spv::Id out = utils::load(m_b, m_spirv_params, m_util_funcs, m_features, op, dest_mask, shift_offset);
    if (is_integer) {
        out = m_b.createBinOp(spv::OpFMul, type_f32_v[4], out, get_int_max_value_constant(m_b, op.type));
        spv::Id out_type = m_b.makeVectorType(type_ui32, 4);
        out = m_b.createUnaryOp(spv::OpConvertSToF, out_type, out);
    }
    return out;
}

void USSETranslatorVisitor::store(Operand dest, spv::Id source, std::uint8_t dest_mask, int shift_offset, bool is_integer) {
    if (is_integer) {
        source = m_b.createUnaryOp(spv::OpConvertSToF, type_f32_v[4], source);
        source = m_b.createBinOp(spv::OpFDiv, type_f32_v[4], source, get_int_max_value_constant(m_b, dest.type));
    }
    utils::store(m_b, m_spirv_params, m_util_funcs, m_features, dest, source, dest_mask, shift_offset);
}

spv::Id USSETranslatorVisitor::swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle) {
    switch (swizzle) {
    case SwizzleChannel::_X:
    case SwizzleChannel::_Y:
    case SwizzleChannel::_Z:
    case SwizzleChannel::_W:
        return m_b.createCompositeExtract(composite, type, static_cast<Imm4>(swizzle));

    // TODO: Implement these with OpCompositeExtract
    case SwizzleChannel::_0: break;
    case SwizzleChannel::_1: break;
    case SwizzleChannel::_2: break;

    case SwizzleChannel::_H: break;
    }

    LOG_WARN("Swizzle channel {} unsupported", static_cast<Imm4>(swizzle));
    return spv::NoResult;
}

size_t USSETranslatorVisitor::dest_mask_to_comp_count(Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    assert(bit_count <= 4 && bit_count > 0);
    return bit_count;
}
