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

#include <cstdlib>
#include <string>
#include <thread>

#include <emuenv/state.h>
#include <gdbstub/functions.h>
#include <util/types.h>

#include <gdbstub/state.h>

#include <gtest/gtest.h>

TEST(gdb_state, destructor_joins_completed_server_thread) {
    EXPECT_EXIT(
        {
            {
                GDBState state;
                state.server_thread = std::make_shared<std::thread>([] {});
            }
            std::exit(0);
        },
        testing::ExitedWithCode(0), "");
}

TEST(gdb_state, server_close_is_safe_when_server_thread_also_closes) {
    EXPECT_EXIT(
        {
            for (int i = 0; i < 1000; ++i) {
                EmuEnvState state;
                state.gdb.server_thread = std::make_shared<std::thread>([&state] {
                    server_close(state);
                });
                server_close(state);
            }
            std::exit(0);
        },
        testing::ExitedWithCode(0), "");
}
