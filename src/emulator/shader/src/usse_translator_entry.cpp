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
#include <shader/decoder_detail.h>
#include <shader/matcher.h>
#include <shader/usse_disasm.h>
#include <shader/usse_translator.h>
#include <util/log.h>

#include <boost/optional/optional.hpp>

#include <map>

namespace shader::usse {

template <typename Visitor>
using USSEMatcher = shader::decoder::Matcher<Visitor, uint64_t>;

template <typename V>
boost::optional<const USSEMatcher<V> &> DecodeUSSE(uint64_t instruction) {
    static const std::vector<USSEMatcher<V>> table = {
// clang-format off
    #define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)

    // Vector move
    /*
                               00111 = op1
                                    ppp = pred (3 bits, ExtPredicate)
                                       s = skipinv (1 bit, bool)
                                        t = test_bit_2 (1 bit)
                                         r = src2_bank_sel (1 bit)
                                          y = syncstart (1 bit, bool)
                                           d = dest_bank_ext (1 bit)
                                            e = end_or_src0_bank_ext (1 bit)
                                             c = src1_bank_ext (1 bit)
                                              b = src2_bank_ext (1 bit)
                                               mm = move_type (2 bits, MoveType)
                                                 aa = repeat_count (2 bits, RepeatCount)
                                                   n = nosched (1 bit, bool)
                                                    ooo = move_data_type (3 bits, MoveDataType)
                                                       i = test_bit_1 (1 bit)
                                                        wwww = src0_swiz (4 bits)
                                                            k = src0_bank_sel (1 bit)
                                                             ll = dest_bank_sel (2 bits)
                                                               ff = src1_bank_sel (2 bits)
                                                                 gg = src0_comp_sel (2 bits)
                                                                   hhhh = dest_mask (4 bits)
                                                                       jjjjjj = dest_n (6 bits)
                                                                             qqqqqq = src0_n (6 bits)
                                                                                   uuuuuu = src1_n (6 bits)
                                                                                         vvvvvv = src2_n (6 bits)
    */
    INST(&V::vmov, "VMOV ()", "00111pppstrydecbmmaanoooiwwwwkllffgghhhhjjjjjjqqqqqquuuuuuvvvvvv"),

    // Vector multiply-add
    /*
                               00011 = opcode1
                                    ppp = pred (3 bits, ExtPredicate)
                                       s = skipinv (1 bit)
                                        g = gpi1_swiz_ext (1 bit)
                                         1 = present_bit_1
                                          o = opcode2 (1 bit)
                                           d = dest_use_bank_ext (1 bit)
                                            e = end (1 bit)
                                             r = src0_bank_ext (1 bit)
                                              ii = increment_mode (2 bits)
                                                a = gpi0_abs (1 bit)
                                                 tt = repeat_count (2 bits, RepeatCount)
                                                   n = nosched (1 bit, bool)
                                                    wwww = write_mask (4 bits)
                                                        c = src0_neg (1 bit)
                                                         b = src0_abs (1 bit)
                                                          f = gpi1_neg (1 bit)
                                                           h = gpi1_abs (1 bit)
                                                            z = gpi0_swiz_ext (1 bit)
                                                             kk = dest_bank (2 bits)
                                                               jj = src0_bank (2 bits)
                                                                 ll = gpi0_n (2 bits)
                                                                   mmmmmm = dest_n (6 bits)
                                                                         qqqq = gpi0_swiz (4 bits)
                                                                             uuuu = gpi1_swiz (4 bits)
                                                                                 vv = gpi1_n (2 bits)
                                                                                   x = gpi0_neg (1 bit)
                                                                                    y = src0_swiz_ext (1 bit)
                                                                                     AAAA = src0_swiz (4 bits)
                                                                                         BBBBBB = src0_n (6 bits)
    */
    INST(&V::vmad, "VMAD ()", "00011pppsg1oderiiattnwwwwcbfhzkkjjllmmmmmmqqqquuuuvvxyAAAABBBBBB"),

    // Vector operations except for MAD (F32)
    /*
                                     00001 = opcode1
                                          ppp = pred (3 bits, ExtPredicate)
                                             s = skipinv (1 bit, bool)
                                              rr = src1_swiz_10_11 (2 bits)
                                                y = syncstart (1 bit, bool)
                                                 d = dest_bank_ext (1 bit)
                                                  c = src1_swiz_9 (1 bit)
                                                   b = src1_bank_ext (1 bit)
                                                    a = src2_bank_ext (1 bit)
                                                     wwww = src2_swiz (4 bits)
                                                         n = nosched (1 bit, bool)
                                                          eeee = dest_mask (4 bits)
                                                              mm = src1_mod (2 bits)
                                                                o = src2_mod (1 bit)
                                                                 ii = src1_swiz_7_8 (2 bits)
                                                                   tt = dest_bank_sel (2 bits)
                                                                     kk = src1_bank_sel (2 bits)
                                                                       ll = src2_bank_sel (2 bits)
                                                                         ffffff = dest_n (6 bits)
                                                                               zzzzzzz = src1_swiz_0_6 (7 bits)
                                                                                      ggg = op2 (3 bits)
                                                                                         hhhhhh = src1_n (6 bits)
                                                                                               jjjjjj = src2_n (6 bits)
    */
    INST(&V::vnmad32, "VNMAD32 ()", "00001pppsrrydcbawwwwneeeemmoiittkkllffffffzzzzzzzggghhhhhhjjjjjj"),

    // Vector operations except for MAD (F16)
    /*
                                     00010 = opcode1
                                          ppp = pred (3 bits, ExtPredicate)
                                             s = skipinv (1 bit, bool)
                                              rr = src1_swiz_10_11 (2 bits)
                                                y = syncstart (1 bit, bool)
                                                 d = dest_bank_ext (1 bit)
                                                  c = src1_swiz_9 (1 bit)
                                                   b = src1_bank_ext (1 bit)
                                                    a = src2_bank_ext (1 bit)
                                                     wwww = src2_swiz (4 bits)
                                                         n = nosched (1 bit, bool)
                                                          eeee = dest_mask (4 bits)
                                                              mm = src1_mod (2 bits)
                                                                o = src2_mod (1 bit)
                                                                 ii = src1_swiz_7_8 (2 bits)
                                                                   tt = dest_bank_sel (2 bits)
                                                                     kk = src1_bank_sel (2 bits)
                                                                       ll = src2_bank_sel (2 bits)
                                                                         ffffff = dest_n (6 bits)
                                                                               zzzzzzz = src1_swiz_0_6 (7 bits)
                                                                                      ggg = op2 (3 bits)
                                                                                         hhhhhh = src1_n (6 bits)
                                                                                               jjjjjj = src2_n (6 bits)
    */
    INST(&V::vnmad16, "VNMAD16 ()", "00010pppsrrydcbawwwwneeeemmoiittkkllffffffzzzzzzzggghhhhhhjjjjjj"),

    // Vector pack/unpack
    /*
                               01000 = op1
                                    ppp = pred (3 bits, ExtPredicate)
                                       s = skipinv (1 bit, bool)
                                        n = nosched (1 bit, bool)
                                         r = src2_bank_sel (1 bit)
                                          y = syncstart (1 bit, bool)
                                           d = dest_bank_ext (1 bit)
                                            e = end (1 bit)
                                             c = src1_bank_ext (1 bit)
                                              b = src2_bank_ext (1 bit)
                                               - = don't care
                                                aaa = repeat_count (3 bits, RepeatCount)
                                                   fff = src_fmt (3 bits)
                                                      ttt = dest_fmt (3 bits)
                                                         mmmm = dest_mask (4 bits)
                                                             kk = dest_bank_sel (2 bits)
                                                               ll = src1_bank_sel (2 bits)
                                                                 -- = don't care
                                                                   ggggggg = dest_n (7 bits)
                                                                          oo = comp_sel_3 (2 bits)
                                                                            h = scale (1 bit)
                                                                             ii = comp_sel_1 (2 bits)
                                                                               jj = comp_sel_2 (2 bits)
                                                                                 qqqqqq = src1_n (6 bits)
                                                                                       u = comp0_sel_bit1 (1 bit)
                                                                                        -- = don't care
                                                                                          vvvv = src2_n (4 bits)
                                                                                              w = comp_sel_0_bit0 (1 bit)
    */
    INST(&V::vpck, "VPCK ()", "01000pppsnrydecb-aaaffftttmmmmkkll--gggggggoohiijjqqqqqqu--vvvvw"),

    // Phase
    /*
                               11111 = op1
                                    ---- = don't care
                                        100 = phas
                                           ---------------------------------------------------- = don't care
    */
    INST(&V::phas, "PHAS ()", "11111----100----------------------------------------------------"),

    // Special
    /*
                               11111 = op1
                                    ---- = don't care
                                        s = special (1 bit, bool)
                                         cc = category (2 bits, SpecialCategory)
                                           ---------------------------------------------------- = don't care
    */
    INST(&V::spec, "SPEC ()", "11111----scc----------------------------------------------------"),

        // clang-format on
    };
#undef INST

    const auto matches_instruction = [instruction](const auto &matcher) { return matcher.Matches(instruction); };

    auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
    return iter != table.end() ? boost::optional<const USSEMatcher<V> &>(*iter) : boost::none;
}

//
// Decoder/translator usage
//

void usse::convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const SpirvShaderParameters &parameters) {
    const uint64_t *primary_program = program.primary_program_start();
    const uint64_t primary_program_instr_count = program.primary_program_instr_count;

    const uint64_t *secondary_program_start = program.secondary_program_start();
    const uint64_t *secondary_program_end = program.secondary_program_end();

    std::map<ShaderPhase, std::vector<uint64_t>> shader_code;

    // Collect insructions of Pixel (primary) phase
    for (auto instr_idx = 0; instr_idx < primary_program_instr_count; ++instr_idx)
        shader_code[ShaderPhase::Pixel].push_back(primary_program[instr_idx]);

    // Collect insructions of Sample rate (secondary) phase
    for (auto instr_idx = secondary_program_start; instr_idx != secondary_program_end; ++instr_idx)
        shader_code[ShaderPhase::SampleRate].push_back(*instr_idx);

    // Decode and recompile
    uint64_t instr;
    usse::USSETranslatorVisitor visitor(b, instr, parameters, program);

    for (auto phase = 0; phase < (uint32_t)ShaderPhase::Max; ++phase) {
        const auto cur_phase_code = shader_code[(ShaderPhase)phase];

        for (auto _instr : cur_phase_code) {
            instr = _instr;
            //LOG_DEBUG("instr: 0x{:016x}", instr);

            usse::instr_idx = instr_idx;

            auto decoder = usse::DecodeUSSE<usse::USSETranslatorVisitor>(instr);
            if (decoder)
                decoder->call(visitor, instr);
            else
                LOG_DISASM("{:016x}: error: instruction unmatched", instr);
        }
    }

    if (program.get_type() == emu::Fragment && !program.is_native_color()) {
        visitor.emit_non_native_frag_output();
    }
}

} // namespace shader::usse
