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
#include <shader/usse_translator_types.h>
#include <util/log.h>

#include <boost/optional/optional.hpp>

#include <map>

namespace shader::usse {

template <typename Visitor>
using USSEMatcher = shader::decoder::Matcher<Visitor, uint64_t>;

template <typename V>
boost::optional<const USSEMatcher<V> &> DecodeUSSE(uint64_t instruction) {
    static const std::vector<USSEMatcher<V>> table = {
#define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)
        // clang-format off

        // Vector move
        /*
                              00111 = op1
                                    ppp = pred (3 bits, ExtPredicate)
                                      s = skipinv (1 bit, bool)
                                        t = test_bit_2 (1 bit)
                                        r = src0_comp_sel (1 bit)
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
                                                                gg = src2_bank_sel (2 bits)
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

        // Vector multiply-add (Normal version)
        /*
                                00000 = opcode1
                                      d = dat_fmt (1 bit)
                                      pp = pred (2 bits)
                                        s = skipinv (1 bit)
                                          - = don't care
                                          r = src0_swiz_bits2 (1 bit)
                                            y = syncstart (1 bit)
                                            - = don't care
                                              c = src0_abs (1 bit)
                                              b = src1_bank_ext (1 bit)
                                                a = src2_bank_ext (1 bit)
                                                www = src2_swiz (3 bits)
                                                    i = src1_swiz_bit2 (1 bit)
                                                    n = nosched (1 bit)
                                                      eeee = dest_mask (4 bits)
                                                          mm = src1_mod (2 bits)
                                                            oo = src2_mod (2 bits)
                                                              k = src0_bank (1 bit)
                                                              tt = dest_bank (2 bits)
                                                                ff = src1_bank (2 bits)
                                                                  gg = src2_bank (2 bits)
                                                                    hhhhhh = dest_n (6 bits)
                                                                          zz = src1_swiz_bits01 (2 bits)
                                                                            jj = src0_swiz_bits01 (2 bits)
                                                                              llllll = src0_n (6 bits)
                                                                                    qqqqqq = src1_n (6 bits)
                                                                                          uuuuuu = src2_n (6 bits)
        */
        INST(&V::vmad2, "VMAD2 ()", "00000dpps-ry-cbawwwineeeemmookttffgghhhhhhzzjjllllllqqqqqquuuuuu"),

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
                                        u = unknown (1 bit)
                                          y = syncstart (1 bit, bool)
                                          d = dest_bank_ext (1 bit)
                                            e = end (1 bit)
                                            r = src1_bank_ext (1 bit)
                                              c = src2_bank_ext (1 bit)
                                              - = don't care
                                                aaa = repeat_count (3 bits, RepeatCount)
                                                  fff = src_fmt (3 bits)
                                                      ttt = dest_fmt (3 bits)
                                                        mmmm = dest_mask (4 bits)
                                                            bb = dest_bank_sel (2 bits)
                                                              kk = src1_bank_sel (2 bits)
                                                                ll = src2_bank_sel (2 bits)
                                                                  ggggggg = dest_n (7 bits)
                                                                          oo = comp_sel_3 (2 bits)
                                                                            h = scale (1 bit)
                                                                            ii = comp_sel_1 (2 bits)
                                                                              jj = comp_sel_2 (2 bits)
                                                                                qqqqqq = src1_n (6 bits)
                                                                                      v = comp0_sel_bit1 (1 bit)
                                                                                        wwwwww = src2_n (6 bits)
                                                                                              x = comp_sel_0_bit0 (1 bit)
        */
        INST(&V::vpck, "VPCK ()", "01000pppsnuyderc-aaaffftttmmmmbbkkllgggggggoohiijjqqqqqqvwwwwwwx"),

        // Sum of Products
        /*
                              10000 = op1
                                    pp = pred (2 bits)
                                      c = cmod1 (1 bit)
                                      s = skipinv (1 bit)
                                        n = nosched (1 bit)
                                        aa = asel1 (2 bits)
                                          d = dest_bank_ext (1 bit)
                                            e = end (1 bit)
                                            r = src1_bank_ext (1 bit)
                                              b = src2_bank_ext (1 bit)
                                              m = cmod2 (1 bit)
                                                ooo = count (3 bits)
                                                  f = amod1 (1 bit)
                                                    ll = asel2 (2 bits)
                                                      ggg = csel1 (3 bits)
                                                        hhh = csel2 (3 bits)
                                                            i = amod2 (1 bit)
                                                            tt = dest_bank (2 bits)
                                                              kk = src1_bank (2 bits)
                                                                jj = src2_bank (2 bits)
                                                                  qqqqqqq = dest_n (7 bits)
                                                                          u = src1_mod (1 bit)
                                                                          vv = cop (2 bits)
                                                                            ww = aop (2 bits)
                                                                              x = asrc1_mod (1 bit)
                                                                                y = dest_mod (1 bit)
                                                                                zzzzzzz = src1_n (7 bits)
                                                                                        AAAAAAA = src2_n (7 bits)
        */
        INST(&V::sop2, "SOP2 ()", "10000ppcsnaaderbmooofllggghhhittkkjjqqqqqqquvvwwxyzzzzzzzAAAAAAA"),

        // Test Instructions
        /*
                              01001 = op1
                                    ppp = pred (3 bits)
                                      s = skipinv (1 bit)
                                        - = don't care
                                        o = onceonly (1 bit)
                                          y = syncstart (1 bit)
                                          d = dest_ext (1 bit)
                                            r = src1_neg (1 bit)
                                            c = src1_ext (1 bit)
                                              e = src2_ext (1 bit)
                                              a = prec (1 bit)
                                                v = src2_vscomp (1 bit)
                                                tt = rpt_count (2 bits, RepeatCount)
                                                  ii = sign_test (2 bits)
                                                    zz = zero_test (2 bits)
                                                      m = test_crcomb_and (1 bit)
                                                        hhh = chan_cc (3 bits)
                                                          nn = pdst_n (2 bits)
                                                            bb = dest_bank (2 bits)
                                                              kk = src1_bank (2 bits)
                                                                ff = src2_bank (2 bits)
                                                                  ggggggg = dest_n (7 bits)
                                                                          w = test_wben (1 bit)
                                                                          ll = alu_sel (2 bits)
                                                                            uuuu = alu_op (4 bits)
                                                                                jjjjjjj = src1_n (7 bits)
                                                                                        qqqqqqq = src2_n (7 bits)
        */
        INST(&V::vtst, "VTST ()", "01001ppps-oydrceavttiizzmhhhnnbbkkffgggggggwlluuuujjjjjjjqqqqqqq"),

        // Test mask Instructions
        /*
                                    01111 = op1
                                          ppp = pred (3 bits)
                                            s = skipinv (1 bit)
                                              - = don't care
                                              o = onceonly (1 bit)
                                                y = syncstart (1 bit)
                                                d = dest_ext (1 bit)
                                                  t = test_flag_2 (1 bit)
                                                  r = src1_ext (1 bit)
                                                    c = src2_ext (1 bit)
                                                    e = prec (1 bit)
                                                      v = src2_vscomp (1 bit)
                                                      uu = rpt_count (2 bits, RepeatCount)
                                                        ii = sign_test (2 bits)
                                                          zz = zero_test (2 bits)
                                                            m = test_crcomb_and (1 bit)
                                                              - = don't care
                                                              aa = tst_mask_type (2 bits)
                                                                -- = don't care
                                                                  bb = dest_bank (2 bits)
                                                                    nn = src1_bank (2 bits)
                                                                      kk = src2_bank (2 bits)
                                                                        fffffff = dest_n (7 bits)
                                                                                w = test_wben (1 bit)
                                                                                ll = alu_sel (2 bits)
                                                                                  gggg = alu_op (4 bits)
                                                                                      hhhhhhh = src1_n (7 bits)
                                                                                              jjjjjjj = src2_n (7 bits)
        */
        INST(&V::vtstmsk, "VTSTMSK ()", "01111ppps-oydtrcevuuiizzm-aa--bbnnkkfffffffwllgggghhhhhhhjjjjjjj"),

        // Bitwise Instructions
        /*
                             01 = op1_cnst
                               ooo = op1 (3 bits)
                                  ppp = pred (3 bits)
                                     s = skipinv (1 bit)
                                      n = nosched (1 bit)
                                       r = repeat_sel (1 bit)
                                        y = sync_start (1 bit)
                                         d = dest_ext (1 bit)
                                          e = end (1 bit)
                                           c = src1_ext (1 bit)
                                            x = src2_ext (1 bit)
                                             mmmm = repeat_count (4 bits)
                                                 i = src2_invert (1 bit)
                                                  ttttt = src2_rot (5 bits)
                                                       hh = src2_exth (2 bits)
                                                         a = op2 (1 bit)
                                                          b = bitwise_partial (1 bit)
                                                           kk = dest_bank (2 bits)
                                                             ff = src1_bank (2 bits)
                                                               gg = src2_bank (2 bits)
                                                                 jjjjjjj = dest_n (7 bits)
                                                                        lllllll = src2_sel (7 bits)
                                                                               qqqqqqq = src1_n (7 bits)
                                                                                    uuuuuuu = src2_n (7 bits)
        */
        INST(&V::vbw, "VBW ()", "01ooopppsnrydecxmmmmittttthhabkkffggjjjjjjjlllllllqqqqqqquuuuuuu"),

        // Phase
        /*
                               11111 = op1
                                    ---- = don't care
                                        100 = phas
                                           ---------------------------------------------------- = don't care
        */
        INST(&V::phas, "PHAS ()", "11111----100----------------------------------------------------"),

        // Nop
        /*
                            11111 = op1
                                  ---- = don't care
                                      0 = opcat_extra
                                      00 = op2_flow_ctrl
                                        ----------- = don't care
                                                    101 = nop
                                                      -------------------------------------- = don't care
        */
        INST(&V::nop, "NOP ()", "11111----000-----------101--------------------------------------"),

        // Branch
        /*
                          11111 = op1
                                ppp = pred (3 bits, ExtPredicate)
                                  s = syncend (1 bit)
                                    0 = opcat_extra
                                    00 = op2_flow_ctrl
                                      e = exception (1 bit, bool)
                                        ----- = don't care
                                            w = pwait (1 bit, bool)
                                              y = sync_ext (1 bit)
                                              n = nosched (1 bit, bool)
                                                b = br_monitor (1 bit, bool)
                                                a = save_link (1 bit, bool)
                                                  00 = br_op
                                                    r = br_type (1 bit)
                                                    ---------------- = don't care
                                                                    i = any_inst (1 bit)
                                                                      l = all_inst (1 bit)
                                                                      oooooooooooooooooooo = br_off (20 bits)
        */
        INST(&V::br, "BR ()", "11111ppps000e-----wynba00r----------------iloooooooooooooooooooo"),

        // Sample Instructions
        /*
                            11100 = op1
                                  ppp = pred (3 bits, ExtPredicate)
                                    s = skipinv (1 bit)
                                      n = nosched (1 bit)
                                      - = don't care
                                        y = syncstart (1 bit)
                                        m = minpack (1 bit)
                                          r = src0_ext (1 bit)
                                          c = src1_ext (1 bit)
                                            e = src2_ext (1 bit)
                                            ff = fconv_type (2 bits)
                                              aa = mask_count (2 bits)
                                                dd = dim (2 bits)
                                                  ll = lod_mode (2 bits)
                                                    t = dest_use_pa (1 bit, bool)
                                                      bb = sb_mode (2 bits)
                                                        gg = src0_type (2 bits)
                                                          k = src0_bank (1 bit)
                                                          hh = drc_sel (2 bits)
                                                            ii = src1_bank (2 bits)
                                                              jj = src2_bank (2 bits)
                                                                ooooooo = dest_n (7 bits)
                                                                        qqqqqqq = src0_n (7 bits)
                                                                              uuuuuuu = src1_n (7 bits)
                                                                                      vvvvvvv = src2_n (7 bits)
        */
        INST(&V::smp, "SMP ()", "11100pppsn-ymrceffaaddlltbbggkhhiijjoooooooqqqqqqquuuuuuuvvvvvvv"),

        // SMLSI control instruction
        /*
                                11111 = op1
                                      010 = op2
                                        -- = don't care
                                          01 = opcat
                                            - = don't care
                                              n = nosched (1 bit)
                                              -- = don't care
                                                tttt = temp_limit (4 bits)
                                                    pppp = pa_limit (4 bits)
                                                        ssss = sa_limit (4 bits)
                                                            d = dest_inc_mode (1 bit)
                                                              r = src0_inc_mode (1 bit)
                                                              c = src1_inc_mode (1 bit)
                                                                i = src2_inc_mode (1 bit)
                                                                eeeeeeee = dest_inc (8 bits)
                                                                        aaaaaaaa = src0_inc (8 bits)
                                                                                bbbbbbbb = src1_inc (8 bits)
                                                                                        ffffffff = src2_inc (8 bits)
        */
        INST(&V::smlsi, "SMLSI ()", "11111010--01-n--ttttppppssssdrcieeeeeeeeaaaaaaaabbbbbbbbffffffff"),
        // Kill program
        /*
                           11111 = op1
                                001 = op2
                                   -- = don't care
                                     11 = opcat
                                       000000000 = kill
                                                pp = pred (2 bits, ShortPredicate)
                                                  00000011011110000000000000000000000000000 = kill2
        */
        INST(&V::kill, "KILL ()", "11111001--11000000000pp00000011011110000000000000000000000000000"),
        // Special
        /*
                               11111 = op1
                                    ---- = don't care
                                        s = special (1 bit, bool)
                                         cc = category (2 bits, SpecialCategory)
                                           ---------------------------------------------------- = don't care
        */
        INST(&V::spec, "SPEC ()", "11111----scc----------------------------------------------------"),

        // Vector Complex Instructions
        /*
                                00110 = op1
                                      ppp = pred (3 bits, ExtPredicate)
                                        s = skipinv (1 bit, bool)
                                          dd = dest_type (2 bits)
                                            y = syncstart (1 bit, bool)
                                            e = dest_bank_ext (1 bit, bool)
                                              n = end (1 bit, bool)
                                              r = src1_bank_ext (1 bit, bool)
                                                - = don't care
                                                aaaa = repeat_count (4 bits, RepeatCount)
                                                    o = nosched (1 bit, bool)
                                                      bb = op2 (2 bits)
                                                        cc = src_type (2 bits)
                                                          mm = src1_mod (2 bits)
                                                            ff = src_comp (2 bits)
                                                              - = don't care
                                                              tt = dest_bank (2 bits)
                                                                kk = src1_bank (2 bits)
                                                                  -- = don't care
                                                                    ggggggg = dest_n (7 bits)
                                                                            -------- = don't care
                                                                                    hhhhhhh = src1_n (7 bits)
                                                                                          --- = don't care
                                                                                            wwww = write_mask (4 bits)
        */
        INST(&V::vcomp, "VCOMP ()", "00110pppsddyenr-aaaaobbccmmff-ttkk--ggggggg-------hhhhhhh---wwww"),

        // Vector Dot Product (single issue)
        /*
                            00011 = opcode1
                                  ppp = pred (3 bits, ExtPredicate)
                                    s = skipinv (1 bit)
                                      c = clip_plane_enable (1 bit, bool)
                                      0 = present_bit_0
                                        o = opcode2 (1 bit)
                                        d = dest_use_bank_ext (1 bit)
                                          e = end (1 bit)
                                          r = src0_bank_ext (1 bit)
                                            ii = increment_mode (2 bits)
                                              g = gpi0_abs (1 bit)
                                              aa = repeat_count (2 bits, RepeatCount)
                                                n = nosched (1 bit, bool)
                                                  wwww = write_mask (4 bits)
                                                      b = src0_neg (1 bit)
                                                      f = src0_abs (1 bit)
                                                        lll = clip_plane_n (3 bits)
                                                          tt = dest_bank (2 bits)
                                                            kk = src0_bank (2 bits)
                                                              hh = gpi0_n (2 bits)
                                                                jjjjjj = dest_n (6 bits)
                                                                      zzzz = gpi0_swiz (4 bits)
                                                                          mmm = src0_swiz_w (3 bits)
                                                                              qqq = src0_swiz_z (3 bits)
                                                                                yyy = src0_swiz_y (3 bits)
                                                                                    xxx = src0_swiz_x (3 bits)
                                                                                      uuuuuu = src0_n (6 bits)
        */
        INST(&V::vdp, "VDP ()", "00011pppsc0oderiigaanwwwwbflllttkkhhjjjjjjzzzzmmmqqqyyyxxxuuuuuu"),

        // Dual issue instruction
        /*
                                 0010 = op1
                                     c = comp_count_type (1 bit)
                                      g = gpi1_neg (1 bit)
                                       ss = sv_pred (2 bits)
                                         k = skipinv (1 bit)
                                          d = dual_op1_ext_vec3_or_has_w_vec4 (1 bit)
                                           t = type_f16 (1 bit, bool)
                                            p = gpi1_swizz_ext (1 bit)
                                             uuuu = unified_store_swizz (4 bits)
                                                 n = unified_store_neg (1 bit)
                                                  aaa = dual_op1 (3 bits)
                                                     l = dual_op2_ext (1 bit)
                                                      r = prim_ustore (1 bit, bool)
                                                       iiii = gpi0_swizz (4 bits)
                                                           wwww = gpi1_swizz (4 bits)
                                                               mm = prim_dest_bank (2 bits)
                                                                 ff = unified_store_slot_bank (2 bits)
                                                                   ee = prim_dest_num_gpi_case (2 bits)
                                                                     bbbbbbb = prim_dest_num (7 bits)
                                                                            ooo = dual_op2 (3 bits)
                                                                               hh = src_config (2 bits)
                                                                                 j = gpi2_slot_num_bit_1 (1 bit)
                                                                                  q = gpi2_slot_num_bit_0_or_unified_store_abs (1 bit)
                                                                                   vv = gpi1_slot_num (2 bits)
                                                                                     xx = gpi0_slot_num (2 bits)
                                                                                       yyy = write_mask_non_gpi (3 bits)
                                                                                          zzzzzzz = unified_store_slot_num (7 bits)
        */
        INST(&V::vdual, "VDUAL ()", "0010cgsskdtpuuuunaaalriiiiwwwwmmffeebbbbbbbooohhjqvvxxyyyzzzzzzz"),

      // Load and Store
      /*
                                  111 = op1_cnst
                                      oo = op1 (2 bits)
                                        ppp = pred (3 bits, ExtPredicate)
                                          s = skipinv (1 bit)
                                            n = nosched (1 bit)
                                            m = moe_expand (1 bit)
                                              y = sync_start (1 bit)
                                              c = cache_ext (1 bit)
                                                r = src0_bank_ext (1 bit)
                                                b = src1_bank_ext (1 bit)
                                                  a = src2_bank_ext (1 bit)
                                                  kkkk = mask_count (4 bits)
                                                      dd = addr_mode (2 bits)
                                                        ee = mode (2 bits)
                                                          t = dest_bank_primattr (1 bit)
                                                            g = range_enable (1 bit)
                                                            ff = data_type (2 bits)
                                                              i = increment_or_decrement (1 bit)
                                                                h = src0_bank (1 bit)
                                                                j = cache_by_pass12 (1 bit)
                                                                  l = drc_sel (1 bit)
                                                                  qq = src1_bank (2 bits)
                                                                    uu = src2_bank (2 bits)
                                                                      vvvvvvv = dest_n (7 bits)
                                                                              wwwwwww = src0_n (7 bits)
                                                                                    xxxxxxx = src1_n (7 bits)
                                                                                            zzzzzzz = src2_n (7 bits)
      */
      INST(&V::vldst, "VLDST ()", "111oopppsnmycrbakkkkddeetgffihjlqquuvvvvvvvwwwwwwwxxxxxxxzzzzzzz"),
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

USSERecompiler::USSERecompiler(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features, const SpirvShaderParameters &parameters,
    utils::SpirvUtilFunctions &utils, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries)
    : inst(nullptr)
    , count(0)
    , b(b)
    , visitor(b, *this, program, features, utils, cur_instr, parameters, queries, true)
    , end_hook_func(end_hook_func) {
}

void USSERecompiler::reset(const std::uint64_t *_inst, const std::size_t _count) {
    inst = _inst;
    count = _count;
    cache.clear();
    avail_blocks.clear();

    usse::analyze(
        static_cast<shader::usse::USSEOffset>(_count - 1), [&](usse::USSEOffset off) -> std::uint64_t { return inst[off]; },
        [&](const usse::USSEBlock &sub) -> usse::USSEBlock * {
            auto result = avail_blocks.emplace(sub.offset, sub);
            if (result.second) {
                return &(result.first->second);
            }

            return nullptr;
        });
}

spv::Function *USSERecompiler::get_or_recompile_block(const usse::USSEBlock &block) {
    auto result = cache.find(block.offset);

    if (result != cache.end()) {
        return result->second;
    }

    // Make a new function (subroutine)
    spv::Block *last_build_point = b.getBuildPoint();
    spv::Block *new_sub_block = nullptr;

    const auto sub_name = fmt::format("block_{}_{}", visitor.is_translating_secondary_program() ? "sec" : "prim",
        block.offset);

    spv::Function *ret_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), sub_name.c_str(), {}, {},
        &new_sub_block);

    std::unique_ptr<spv::Builder::If> cond_builder;
    if (block.pred != 0) {
        spv::Id pred_v = spv::NoResult;
        const ExtPredicate predicator = static_cast<ExtPredicate>(block.pred);

        Operand pred_opr{};
        pred_opr.bank = RegisterBank::PREDICATE;

        bool do_neg = false;

        if (predicator >= ExtPredicate::P0 && predicator <= ExtPredicate::P3) {
            pred_opr.num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::P0);
        } else if (predicator >= ExtPredicate::NEGP0 && predicator <= ExtPredicate::NEGP1) {
            pred_opr.num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::NEGP0);
            do_neg = true;
        }

        pred_v = visitor.load(pred_opr, 0b0001);
        if (do_neg) {
            std::vector<spv::Id> ops{ pred_v };
            pred_v = b.createOp(spv::OpLogicalNot, b.makeBoolType(), ops);
        }

        // Construct the IF
        cond_builder = std::make_unique<spv::Builder::If>(pred_v, spv::SelectionControlMaskNone, b);
    }

    const auto last_pc = cur_pc;

    if (block.size > 0) {
        LOG_TRACE("Recompiling block_{}, size = {}, id = {}", block.offset, block.size, ret_func->getId());

        cache.emplace(block.offset, ret_func);
        const usse::USSEOffset pc_end = block.offset + block.size - 1;

        for (usse::USSEOffset pc = block.offset; pc <= pc_end; pc++) {
            cur_pc = pc;
            cur_instr = inst[pc];

            // Recompile the instruction, to the current block
            auto decoder = usse::DecodeUSSE<usse::USSETranslatorVisitor>(cur_instr);
            if (decoder)
                decoder->call(visitor, cur_instr);
            else
                LOG_DISASM("{:016x}: error: instruction unmatched", cur_instr);
        }
    }

    if (block.offset + block.size >= count && !visitor.is_translating_secondary_program()) {
        // We reach the end, for whatever the current block is.
        // Call end hook
        b.createFunctionCall(end_hook_func, {});
    }

    if (cond_builder) {
        cond_builder->makeEndIf();
    }

    if (block.offset_link != -1) {
        b.createFunctionCall(get_or_recompile_block(avail_blocks[block.offset_link]), {});
    }

    // Make a return
    b.leaveFunction();
    b.setBuildPoint(last_build_point);

    cur_pc = last_pc;
    return ret_func;
}

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features, const SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils,
    spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries) {
    const uint64_t *primary_program = program.primary_program_start();
    const uint64_t primary_program_instr_count = program.primary_program_instr_count;

    const uint64_t *secondary_program_start = program.secondary_program_start();
    const uint64_t *secondary_program_end = program.secondary_program_end();

    std::map<ShaderPhase, std::pair<const std::uint64_t *, std::uint64_t>> shader_code;

    // Collect instructions of Pixel (primary) phase
    shader_code[ShaderPhase::Pixel] = std::make_pair(primary_program, primary_program_instr_count);

    // Collect instructions of Sample rate (secondary) phase
    shader_code[ShaderPhase::SampleRate] = std::make_pair(secondary_program_start, secondary_program_end - secondary_program_start);

    // Decode and recompile
    // TODO: Reuse this
    usse::USSERecompiler recomp(b, program, features, parameters, utils, end_hook_func, queries);

    // Set the program
    recomp.program = &program;

    for (auto phase = 0; phase < (uint32_t)ShaderPhase::Max; ++phase) {
        const auto cur_phase_code = shader_code[(ShaderPhase)phase];

        if (cur_phase_code.second != 0) {
            if (static_cast<ShaderPhase>(phase) == ShaderPhase::SampleRate) {
                recomp.visitor.set_secondary_program(true);
            } else {
                recomp.visitor.set_secondary_program(false);
            }

            recomp.reset(cur_phase_code.first, cur_phase_code.second);
            spv::Function *main_block = recomp.get_or_recompile_block(recomp.avail_blocks[0]);

            b.createFunctionCall(main_block, {});
        }
    }

    std::vector<spv::Id> empty_args;
    if (features.should_use_shader_interlock() && program.is_fragment() && program.is_native_color())
        b.createNoResultOp(spv::OpEndInvocationInterlockEXT);

    b.leaveFunction();
}

} // namespace shader::usse
