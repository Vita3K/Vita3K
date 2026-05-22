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

#include <util/types.h>

#include <gdbstub/state.h>

#include <gtest/gtest.h>

#include <util/exit_code.h>

struct AppLaunchRequest;

// libapp calls these from the session controller, and they live in the vita3k executable
ExitCode load_app(int32_t &, EmuEnvState &, const AppLaunchRequest &) { return InvalidApplicationPath; }
ExitCode run_app(EmuEnvState &, int32_t, const AppLaunchRequest &) { return InvalidApplicationPath; }

TEST(gdb_state, destructor_joins_completed_server_thread) {
    // Static initialisers start threads, and a forked child can hang on what they hold
    GTEST_FLAG_SET(death_test_style, "threadsafe");
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
