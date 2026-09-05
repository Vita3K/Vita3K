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

#include <cheat/cheat.h>

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Segment of a loaded module a `$B2` code makes addresses relative to, `size` 0 when absent.
struct CheatModuleSegment {
    uint32_t address = 0;
    uint32_t size = 0;
};

struct CheatModule {
    std::string name;
    std::array<CheatModuleSegment, 2> segments;
};

struct CheatState {
    // Guards every member below, the vblank thread reads while the GUI thread toggles cheats.
    mutable std::mutex mutex;

    bool enabled = false;

    // Buttons the game last read, kept outside the mutex so that reading the pad stays cheap.
    std::atomic<uint32_t> buttons{ 0 };

    cheat::CheatFile file;
    std::vector<CheatModule> modules;

    bool has_cheats() const {
        return !file.cheats.empty();
    }
};
