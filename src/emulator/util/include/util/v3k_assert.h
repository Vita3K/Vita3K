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

#pragma once

#include <util/log.h>

#include <cstdlib>

#define STRINGIZE_DETAIL(x) #x ""
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define HERE "(in file " __FILE__ ":" STRINGIZE(__LINE__) ")"

static bool g_should_abort = false;
inline void v3k_assert_set_abort(bool should_abort) {
    g_should_abort = should_abort;
}

#define v3k_assert(flag, fmt, ...)                                                           \
    do {                                                                                     \
        if (!(flag)) {                                                                       \
            LOG_CRITICAL("Assertion \"{}\" failed!\n{}\n" fmt, #flag, HERE, ##__VA_ARGS__); \
            if (g_should_abort) {                                                           \
                abort();                                                                     \
            }                                                                                \
        }                                                                                    \
    } while (false)
