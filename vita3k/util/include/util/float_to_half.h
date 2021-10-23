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

//original source:https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
//public domain

#include <climits> // CHAR_BIT
#include <cstdint> // uint32_t, uint64_t, etc.
#include <cstring> // memcpy
#include <limits> // numeric_limits
#include <utility> // is_integral_v, is_floating_point_v, forward

namespace std {
template <typename T, typename U>
T bit_cast(U &&u) {
    static_assert(sizeof(T) == sizeof(U));
    union {
        T t;
    }; // prevent construction
    std::memcpy(&t, &u, sizeof(t));
    return t;
}
} // namespace std

namespace util {
template <typename T>
struct native_float_bits;
template <>
struct native_float_bits<float> { using type = std::uint32_t; };
template <>
struct native_float_bits<double> { using type = std::uint64_t; };
template <typename T>
using native_float_bits_t = typename native_float_bits<T>::type;

static_assert(sizeof(float) == sizeof(native_float_bits_t<float>));
static_assert(sizeof(double) == sizeof(native_float_bits_t<double>));

template <typename T, int SIG_BITS, int EXP_BITS>
struct raw_float_type_info {
    using raw_type = T;

    static constexpr int sig_bits = SIG_BITS;
    static constexpr int exp_bits = EXP_BITS;
    static constexpr int bits = sig_bits + exp_bits + 1;

    static_assert(std::is_integral_v<raw_type>);
    static_assert(sig_bits >= 0);
    static_assert(exp_bits >= 0);
    static_assert(bits <= sizeof(raw_type) * CHAR_BIT);

    static constexpr int exp_max = (1 << exp_bits) - 1;
    static constexpr int exp_bias = exp_max >> 1;

    static constexpr raw_type sign = raw_type(1) << (bits - 1);
    static constexpr raw_type inf = raw_type(exp_max) << sig_bits;
    static constexpr raw_type qnan = inf | (inf >> 1);

    static constexpr auto abs(raw_type v) { return raw_type(v & (sign - 1)); }
    static constexpr bool is_nan(raw_type v) { return abs(v) > inf; }
    static constexpr bool is_inf(raw_type v) { return abs(v) == inf; }
    static constexpr bool is_zero(raw_type v) { return abs(v) == 0; }
};
using raw_flt16_type_info = raw_float_type_info<std::uint16_t, 10, 5>;
using raw_flt32_type_info = raw_float_type_info<std::uint32_t, 23, 8>;
using raw_flt64_type_info = raw_float_type_info<std::uint64_t, 52, 11>;
//using raw_flt128_type_info = raw_float_type_info< uint128_t, 112, 15 >;

template <typename T, int SIG_BITS = std::numeric_limits<T>::digits - 1,
    int EXP_BITS = sizeof(T) * CHAR_BIT - SIG_BITS - 1>
struct float_type_info
    : raw_float_type_info<native_float_bits_t<T>, SIG_BITS, EXP_BITS> {
    using flt_type = T;
    static_assert(std::is_floating_point_v<flt_type>);
};

template <typename E>
struct raw_float_encoder {
    using enc = E;
    using enc_type = typename enc::raw_type;

    template <bool DO_ROUNDING, typename F>
    static auto encode(F value) {
        using flt = float_type_info<F>;
        using raw_type = typename flt::raw_type;
        static constexpr auto sig_diff = flt::sig_bits - enc::sig_bits;
        static constexpr auto bit_diff = flt::bits - enc::bits;
        static constexpr auto do_rounding = DO_ROUNDING && sig_diff > 0;
        static constexpr auto bias_mul = raw_type(enc::exp_bias) << flt::sig_bits;
        if constexpr (!do_rounding) { // fix exp bias
            // when not rounding, fix exp first to avoid mixing float and binary ops
            value *= std::bit_cast<F>(bias_mul);
        }
        auto bits = std::bit_cast<raw_type>(value);
        auto sign = bits & flt::sign; // save sign
        bits ^= sign; // clear sign
        auto is_nan = flt::inf < bits; // compare before rounding!!
        if constexpr (do_rounding) {
            static constexpr auto min_norm = raw_type(flt::exp_bias - enc::exp_bias + 1) << flt::sig_bits;
            static constexpr auto sub_rnd = enc::exp_bias < sig_diff
                ? raw_type(1) << (flt::sig_bits - 1 + enc::exp_bias - sig_diff)
                : raw_type(enc::exp_bias - sig_diff) << flt::sig_bits;
            static constexpr auto sub_mul = raw_type(flt::exp_bias + sig_diff) << flt::sig_bits;
            bool is_sub = bits < min_norm;
            auto norm = std::bit_cast<F>(bits);
            auto subn = norm;
            subn *= std::bit_cast<F>(sub_rnd); // round subnormals
            subn *= std::bit_cast<F>(sub_mul); // correct subnormal exp
            norm *= std::bit_cast<F>(bias_mul); // fix exp bias
            bits = std::bit_cast<raw_type>(norm);
            bits += (bits >> sig_diff) & 1; // add tie breaking bias
            bits += (raw_type(1) << (sig_diff - 1)) - 1; // round up to half
            //if( is_sub ) bits = std::bit_cast< raw_type >( subn );
            bits ^= -is_sub & (std::bit_cast<raw_type>(subn) ^ bits);
        }
        bits >>= sig_diff; // truncate
        //if( enc::inf < bits ) bits = enc::inf; // fix overflow
        bits ^= -(enc::inf < bits) & (enc::inf ^ bits);
        //if( is_nan ) bits = enc::qnan;
        bits ^= -is_nan & (enc::qnan ^ bits);
        bits |= sign >> bit_diff; // restore sign
        return enc_type(bits);
    }

    template <typename F>
    static F decode(enc_type value) {
        using flt = float_type_info<F>;
        using raw_type = typename flt::raw_type;
        static constexpr auto sig_diff = flt::sig_bits - enc::sig_bits;
        static constexpr auto bit_diff = flt::bits - enc::bits;
        static constexpr auto bias_mul = raw_type(2 * flt::exp_bias - enc::exp_bias) << flt::sig_bits;
        raw_type bits = value;
        auto sign = bits & enc::sign; // save sign
        bits ^= sign; // clear sign
        auto is_norm = bits < enc::inf;
        bits = (sign << bit_diff) | (bits << sig_diff);
        auto val = std::bit_cast<F>(bits) * std::bit_cast<F>(bias_mul);
        bits = std::bit_cast<raw_type>(val);
        //if( !is_norm ) bits |= flt::inf;
        bits |= -!is_norm & flt::inf;
        return std::bit_cast<F>(bits);
    }
};

using flt16_encoder = raw_float_encoder<raw_flt16_type_info>;

template <typename F>
auto quick_encode_flt16(F &&value) { return flt16_encoder::encode<false>(std::forward<F>(value)); }

template <typename F>
auto encode_flt16(F &&value) { return flt16_encoder::encode<true>(std::forward<F>(value)); }

template <typename F = float, typename X>
auto decode_flt16(X &&value) { return flt16_encoder::decode<F>(std::forward<X>(value)); }
} // namespace util