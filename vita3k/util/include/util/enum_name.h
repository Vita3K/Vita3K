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

#pragma once

#define USE_BOOST_DESCRIBE_ENUM 1

#ifdef USE_BOOST_DESCRIBE_ENUM
#include <boost/describe/enum.hpp>
#include <boost/describe/enum_to_string.hpp>
#else

template <typename E>
static char const *enum_to_string(E e, char const *def = "") noexcept;

#if (defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL))
#define ENUM_NAME_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N

#define ENUM_NAME_fe_0(_call, ...) // nothing
#define ENUM_NAME_fe_1(_call, x) _call(x)
#define ENUM_NAME_fe_2(_call, x, ...) _call(x) ENUM_NAME_fe_1(_call, __VA_ARGS__)
#define ENUM_NAME_fe_3(_call, x, ...) _call(x) ENUM_NAME_fe_2(_call, __VA_ARGS__)
#define ENUM_NAME_fe_4(_call, x, ...) _call(x) ENUM_NAME_fe_3(_call, __VA_ARGS__)
#define ENUM_NAME_fe_5(_call, x, ...) _call(x) ENUM_NAME_fe_4(_call, __VA_ARGS__)
#define ENUM_NAME_fe_6(_call, x, ...) _call(x) ENUM_NAME_fe_5(_call, __VA_ARGS__)
#define ENUM_NAME_fe_7(_call, x, ...) _call(x) ENUM_NAME_fe_6(_call, __VA_ARGS__)
#define ENUM_NAME_fe_8(_call, x, ...) _call(x) ENUM_NAME_fe_7(_call, __VA_ARGS__)
#define ENUM_NAME_fe_9(_call, x, ...) _call(x) ENUM_NAME_fe_8(_call, __VA_ARGS__)
#define ENUM_NAME_fe_10(_call, x, ...) _call(x) ENUM_NAME_fe_9(_call, __VA_ARGS__)
#define ENUM_NAME_fe_11(_call, x, ...) _call(x) ENUM_NAME_fe_10(_call, __VA_ARGS__)
#define ENUM_NAME_fe_12(_call, x, ...) _call(x) ENUM_NAME_fe_11(_call, __VA_ARGS__)
#define ENUM_NAME_fe_13(_call, x, ...) _call(x) ENUM_NAME_fe_12(_call, __VA_ARGS__)
#define ENUM_NAME_fe_14(_call, x, ...) _call(x) ENUM_NAME_fe_13(_call, __VA_ARGS__)
#define ENUM_NAME_fe_15(_call, x, ...) _call(x) ENUM_NAME_fe_14(_call, __VA_ARGS__)
#define ENUM_NAME_fe_16(_call, x, ...) _call(x) ENUM_NAME_fe_15(_call, __VA_ARGS__)
#define ENUM_NAME_fe_17(_call, x, ...) _call(x) ENUM_NAME_fe_16(_call, __VA_ARGS__)
#define ENUM_NAME_fe_18(_call, x, ...) _call(x) ENUM_NAME_fe_17(_call, __VA_ARGS__)
#define ENUM_NAME_fe_19(_call, x, ...) _call(x) ENUM_NAME_fe_18(_call, __VA_ARGS__)

#define FOR_EACH(x, ...)                                                                                                                                                          \
    ENUM_NAME_GET_NTH_ARG("ignored", ##__VA_ARGS__,                                                                                                                               \
        ENUM_NAME_fe_19, ENUM_NAME_fe_18, ENUM_NAME_fe_17, ENUM_NAME_fe_16, ENUM_NAME_fe_15, ENUM_NAME_fe_14, ENUM_NAME_fe_13, ENUM_NAME_fe_12, ENUM_NAME_fe_11, ENUM_NAME_fe_10, \
        ENUM_NAME_fe_9, ENUM_NAME_fe_8, ENUM_NAME_fe_7, ENUM_NAME_fe_6, ENUM_NAME_fe_5, ENUM_NAME_fe_4, ENUM_NAME_fe_3, ENUM_NAME_fe_2, ENUM_NAME_fe_1, ENUM_NAME_fe_0)(x, ##__VA_ARGS__)
#else

#define PARENS ()

// Rescan macro tokens 256 times
#define EXPAND(arg) EXPAND1(EXPAND1(EXPAND1(EXPAND1(arg))))
#define EXPAND1(arg) EXPAND2(EXPAND2(EXPAND2(EXPAND2(arg))))
#define EXPAND2(arg) EXPAND3(EXPAND3(EXPAND3(EXPAND3(arg))))
#define EXPAND3(arg) EXPAND4(EXPAND4(EXPAND4(EXPAND4(arg))))
#define EXPAND4(arg) arg

#define FOR_EACH(macro, ...) \
    __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
    macro(a1)                           \
        __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER
#endif

#define ENUM_NAME_CASE(enum_value) \
    case enum_value: return #enum_value;

#define BOOST_DESCRIBE_ENUM(type, ...)                             \
    template <>                                                    \
    char const *enum_to_string(type e, char const *def) noexcept { \
        switch (e) {                                               \
            FOR_EACH(ENUM_NAME_CASE, __VA_ARGS__)                  \
        }                                                          \
        return def;                                                \
    }
#endif