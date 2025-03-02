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

#if defined(__GNUC__) || defined(__clang__)
#define DISABLE_WARNING_PUSH _Pragma("GCC diagnostic push")
#define DISABLE_WARNING_POP _Pragma("GCC diagnostic pop")
#define STRINGIZE__(x) #x
#define DISABLE_WARNING_NO_PUSH(warningNumber, warningName) _Pragma(STRINGIZE__(GCC diagnostic ignored warningName))
#elif defined(_MSC_VER)
#define DISABLE_WARNING_PUSH __pragma(warning(push))
#define DISABLE_WARNING_POP __pragma(warning(pop))
#define DISABLE_WARNING_NO_PUSH(warningNumber, warningName) __pragma(warning(disable \
                                                                             : warningNumber))
#else
#define DISABLE_WARNING_PUSH
#define DISABLE_WARNING_POP
#define DISABLE_WARNING_NO_PUSH(warningNumber, warningName)
#endif
#define DISABLE_WARNING_BEGIN(warningNumber, warningName) \
    DISABLE_WARNING_PUSH                                  \
    DISABLE_WARNING_NO_PUSH(warningNumber, warningName)
#define DISABLE_WARNING_END DISABLE_WARNING_POP
