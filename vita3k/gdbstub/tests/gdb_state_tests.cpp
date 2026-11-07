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

#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include <emuenv/state.h>
#include <gdbstub/functions.h>
#include <util/types.h>

#include <gdbstub/state.h>

#include <gtest/gtest.h>

#include <util/exit_code.h>

#ifndef _WIN32
#include <unistd.h>
#endif

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

TEST(gdb_state, server_close_is_safe_when_server_thread_also_closes) {
    // Static initialisers start threads, and a forked child can hang on what they hold
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(
        {
            for (int i = 0; i < 1000; ++i) {
                EmuEnvState state;
                // Shared, so the promise outlives a set_value() still returning when the wait ends
                const auto closed = std::make_shared<std::promise<void>>();
                std::future<void> done = closed->get_future();
                // The thread can take server_thread as soon as it runs
                std::unique_lock guard(state.gdb.server_thread_mutex);
                state.gdb.server_thread = std::make_shared<std::thread>([&state, closed] {
                    server_close(state);
                    closed->set_value();
                });
                guard.unlock();
                server_close(state);
                // The thread detaches itself when it takes server_thread first, and still uses state
                done.wait();
            }
            std::exit(0);
        },
        testing::ExitedWithCode(0), "");
}

#ifndef _WIN32
// closesocket() already wakes accept() on Windows
TEST(gdb_state, server_close_wakes_server_waiting_for_client) {
    // Static initialisers start threads, and a forked child can hang on what they hold
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(
        {
            // A hang in the join fails the test instead of blocking it
            alarm(10);
            EmuEnvState state;
            // Not the port of a Vita3K that may be running
            state.gdb.port = 0;
            server_open(state);
            if (!state.gdb.server_thread)
                std::exit(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            server_close(state);
            std::exit(0);
        },
        testing::ExitedWithCode(0), "");
}
#endif
