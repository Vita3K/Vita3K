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

#include <util/fs.h>
#include <util/types.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace savedata {

struct SlotInfo {
    uint32_t id;
    std::string title;
    std::string subtitle;
    std::string detail;
    SceDateTime modified;
};

// A folder copied from a PS Vita without mounting it still has its PFS layout
bool is_encrypted(const fs::path &save_path);
// Only savedata that came from a PS Vita has the sealedkey needed to encrypt it again
bool can_export(const fs::path &save_path);
std::vector<SlotInfo> read_slots(const fs::path &save_path);

bool import_encrypted(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error);
bool export_encrypted(const fs::path &src, const fs::path &dst, const std::function<void(float)> &progress, std::string &error);

} // namespace savedata
