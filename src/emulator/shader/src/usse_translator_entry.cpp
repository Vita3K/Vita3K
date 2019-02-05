// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <shader/usse_translator_entry.h>

#include <gxm/types.h>
#include <shader/usse_disasm.h>
#include <shader/usse_translator.h>

//
// Decoder entry
//
#include <shader/decoder_detail.h>
#include <shader/matcher.h>

namespace shader {
namespace usse {

template <typename Visitor>
using USSEMatcher = shader::decoder::Matcher<Visitor, uint64_t>;

template <typename V>
boost::optional<const USSEMatcher<V> &> DecodeUSSE(uint64_t instruction) {
    static const std::vector<USSEMatcher<V>> table = {
    // clang-format off
#define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)

        // Vector ops (except MAD)
        /*
            MISC: p = predicate, s = skipinv, y = syncstart, n = nosched, 
            TYPE: t = op2
            DEST: u dest, b = dest bank sel, x = dest bank ext, w = dest mask
            SRC1: i = src1, g = src1 bank sel, h = src1 bank ext, z = src1 mod, k = src2 swizzle p1
            SRC1 SWIZZLE: k = src2 swizzle 0-6, v = src2 swizzle 7-8, c = src2 swizzle 9, m = src2 swizzle 10-11
            SRC2: j = src2, l = src2 bank sel, f = src2 bank ext, r = src2 mod (abs)
            SRC2 SWIZZLE: o = src2 swizzle
        */
        INST(&V::vnmad, "VNMADF32 ()",       "00001pppsmmyxchfoooonwwwwzzrvvbbgglluuuuuukkkkkkktttiiiiiijjjjjj"),
        INST(&V::vnmad, "VNMADF16 ()",       "00010pppsmmyxchfoooonwwwwzzrvvbbgglluuuuuukkkkkkktttiiiiiijjjjjj"),

		// Vector move
		/*
            MISC: p = predicate, s = skipinv, y = syncstart, e = end / src0 bank ext (COND ONLY), n = nosched, r = repeat
            TYPE: t = move type, z = move data type
            TEST: c = test bit 1, o = test bit 2
            DEST: u = dest, b = dest bank sel, x = dest bank ext, w = dest mask
            SRC:  q = src swizzle
            SRC0 (COND ONLY): d = src0, v = src0 bank sel, a = src0 comp sel
            SRC1: i = src1, g = src1 bank sel, h = src1 bank ext
            SRC2 (COND ONLY): j = src2, l = src2 bank sel, f = src2 bank ext
		*/
        INST(&V::vmov, "VMOV ()",            "00111pppsolyxehfttrrnzzzcqqqqvbbggaawwwwuuuuuuddddddiiiiiijjjjjj"),

		// Vector pack/unpack
		/*
		    MISC: p = predicate, r = repeat, k = skipinv, y = syncstart, e = end, n = nosched, c = scale
            FMT: s = src fmt, d = dest fmt
			DEST: u = dest, i = dest bank, x = dest bank ext, w = dest mask
            SRC:  q = comp sel 0 bit 0, m = comp sel 1, b = comp sel 2, v = comp sel 3 bit 0
			SRC1: f = src1 (COND2 ONLY), g = src1 bank sel, j = src1 bank ext
			SRC (COND1 ONLY): h = comp0 sel bit1
            SRC2 (COND1 ONLY): k = src2 / comp sel 0 bit 1 (NOT COND1) , o = src2 bank sel, t = src2 bank ext
			COND1: if src is f32
			COND2: if src is f32, f16 or c10
		*/
		INST(&V::vpck, "VPCK ()",            "01000pppknoyxejt-rrrsssdddwwwwiigg--uuuuuuuvvcmmbbffffffh--kkkkq"),

		// Vector multiply-add
        /*
           * a = dest_bank
           * b = dest_use_extended_bank
           * c = end
           * d = gpi0_abs
           * e = gpi0_swizz_extended
           * f = gpi1_abs
           * g = gpi1_neg
           * h = gpi1_swizz_extend
           * i = increment_mode
           * j = no_sched
           * k = opcode2
           * l = predicate
           * m = repeat_count
           * n = skipinv
           * o = src0_abs
           * p = src0_extended_bank
           * q = src0_neg
           * r = write_mask
           * s = dest_n
           * t = gpi0_n
           * u = gpi0_neg
           * v = gpi0_swizz
           * w = gpi1_n
           * x = gpi1_swizz
           * y = src0_bank
           * z = src0_n
           * A = src0_swizz
           * B = src0_swizz_extended
        */
        INST(&V::vmad, "VMAD ()",     "00011lllnh1kbcpiidmmjrrrrqogfeaayyttssssssvvvvxxxxwwuBAAAAzzzzzz"),

        // Special
        // s = special, c = category
		INST(&V::special_phas, "PHAS ()",    "11111----100----------------------------------------------------"),
		INST(&V::special, "SPEC ()",         "11111----scc----------------------------------------------------"),

        // clang-format on
    };
#undef INST

    const auto matches_instruction = [instruction](const auto &matcher) { return matcher.Matches(instruction); };

    auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
    return iter != table.end() ? boost::optional<const USSEMatcher<V> &>(*iter) : boost::none;
}

} // namespace usse
} // namespace shader

//
// Decoder/translator usage
//
namespace shader {
namespace usse {

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const SpirvShaderParameters &parameters) {
    const uint64_t *code_ptr = program.get_code_start_ptr();
    const uint64_t instr_count = program.code_instr_count;
    const emu::SceGxmProgramType program_type = program.get_type();

    uint64_t instr;
    usse::USSETranslatorVisitor visitor(b, instr, parameters, program_type);

    for (auto instr_idx = 0; instr_idx < instr_count; ++instr_idx) {
        instr = code_ptr[instr_idx];

        //LOG_DEBUG("instr: 0x{:016x}", instr);

        usse::instr_idx = instr_idx;

        auto decoder = usse::DecodeUSSE<usse::USSETranslatorVisitor>(instr);
        if (decoder)
            decoder->call(visitor, instr);
        else
            LOG_DISASM("{:016x}: error: instruction unmatched", instr);
    }
}

} // namespace usse
} // namespace shader
