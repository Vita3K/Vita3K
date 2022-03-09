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

spv::Id USSETranslatorVisitor::load(Operand op, const Imm4 dest_mask, const int shift_offset) {
    return utils::load(m_b, m_spirv_params, m_util_funcs, m_features, op, dest_mask, shift_offset);
}

void USSETranslatorVisitor::store(Operand dest, spv::Id source, std::uint8_t dest_mask, int shift_offset) {
    utils::store(m_b, m_spirv_params, m_util_funcs, m_features, dest, source, dest_mask, shift_offset);
}

spv::Id USSETranslatorVisitor::swizzle_to_spv_comp(spv::Id composite, spv::Id type, SwizzleChannel swizzle) {
    switch (swizzle) {
    case SwizzleChannel::C_X:
    case SwizzleChannel::C_Y:
    case SwizzleChannel::C_Z:
    case SwizzleChannel::C_W:
        return m_b.createCompositeExtract(composite, type, static_cast<Imm4>(swizzle));

    // TODO: Implement these with OpCompositeExtract
    case SwizzleChannel::C_0: break;
    case SwizzleChannel::C_1: break;
    case SwizzleChannel::C_2: break;

    case SwizzleChannel::C_H: break;
    }

    LOG_WARN("Swizzle channel {} unsupported", static_cast<Imm4>(swizzle));
    return spv::NoResult;
}

size_t USSETranslatorVisitor::dest_mask_to_comp_count(Imm4 dest_mask) {
    std::bitset<4> bs(dest_mask);
    const auto bit_count = bs.count();
    return bit_count;
}
