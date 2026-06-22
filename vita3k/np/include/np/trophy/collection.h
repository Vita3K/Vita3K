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

#include <np/trophy/context.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct IOState;

namespace np::trophy {

struct TrophyRecord {
    int id = 0;
    std::string name;
    std::string detail;
    int grade = 0;
    bool hidden = false;
    bool earned = false;
    uint64_t timestamp = 0;
    std::string icon_path;
};

struct CollectionSnapshot {
    CollectionSnapshot() { grade_unlocked.fill(0); }

    std::string np_com_id;
    std::string title;
    std::string icon_path;
    int total = 0;
    int unlocked = 0;
    std::array<int, 5> grade_unlocked;

    std::vector<TrophyRecord> trophies;

    Context context;
};

struct CollectionSource {
    IOState *io = nullptr;
    fs::path vita_fs_path;
    std::string user_id;
    uint32_t lang = 0;
};

std::vector<std::string> list_collection_ids(const CollectionSource &source);
bool load_collection(const CollectionSource &source, const std::string &np_com_id, CollectionSnapshot &snapshot);

} // namespace np::trophy