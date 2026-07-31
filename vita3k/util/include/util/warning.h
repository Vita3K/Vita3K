// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

// 'warning' pragma: https://learn.microsoft.com/en-us/cpp/preprocessor/warning
// MSVC warnings: https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warnings-c4800-through-c4999
#if defined(_MSC_VER) && !defined(__clang__)
#define DISABLE_MSVC_WARNING_BEGIN(warning_number) __pragma(warning(push)) __pragma(warning(disable : warning_number))
#define DISABLE_MSVC_WARNING_END __pragma(warning(pop))
#else
#define DISABLE_MSVC_WARNING_BEGIN(warning_number)
#define DISABLE_MSVC_WARNING_END
#endif

// 'diagnostic' pragma: https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#if defined(__GNUC__) || defined(__clang__)
#define PRAGMA_(x) _Pragma(#x)
#define DISABLE_GNU_LIKE_WARNING_BEGIN(diag_flag) _Pragma("GCC diagnostic push") PRAGMA_(GCC diagnostic ignored diag_flag)
#define DISABLE_GNU_LIKE_WARNING_END _Pragma("GCC diagnostic pop")
#else
#define DISABLE_GNU_LIKE_WARNING_BEGIN(diag_flag)
#define DISABLE_GNU_LIKE_WARNING_END
#endif

// GCC warnings: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
#if defined(__GNUC__) && !defined(__clang__)
#define DISABLE_GCC_WARNING_BEGIN(diag_flag) DISABLE_GNU_LIKE_WARNING_BEGIN(diag_flag)
#define DISABLE_GCC_WARNING_END DISABLE_GNU_LIKE_WARNING_END
#else
#define DISABLE_GCC_WARNING_BEGIN(diag_flag)
#define DISABLE_GCC_WARNING_END
#endif

// Clang warnings: https://clang.llvm.org/docs/DiagnosticsReference.html
#ifdef __clang__
#define DISABLE_CLANG_WARNING_BEGIN(diag_flag) DISABLE_GNU_LIKE_WARNING_BEGIN(diag_flag)
#define DISABLE_CLANG_WARNING_END DISABLE_GNU_LIKE_WARNING_END
#else
#define DISABLE_CLANG_WARNING_BEGIN(diag_flag)
#define DISABLE_CLANG_WARNING_END
#endif

#define DISABLE_WARNING_BEGIN(warning_number, diag_flag) \
    DISABLE_MSVC_WARNING_BEGIN(warning_number)           \
    DISABLE_GNU_LIKE_WARNING_BEGIN(diag_flag)

#define DISABLE_WARNING_END      \
    DISABLE_GNU_LIKE_WARNING_END \
    DISABLE_MSVC_WARNING_END
