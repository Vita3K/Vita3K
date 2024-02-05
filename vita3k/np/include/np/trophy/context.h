// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <np/common.h>
#include <np/trophy/trp_parser.h>
#include <util/fs.h>
#include <util/types.h>

#include <array>

struct IOState;

namespace np::trophy {

using ContextHandle = int32_t;

static constexpr ContextHandle INVALID_CONTEXT_HANDLE = -1;
static constexpr uint32_t MAX_TROPHIES = 128;
static constexpr uint32_t MAX_GROUPS = 16;

// Each bit indicates a trophy unlocked. An uint32_t has total of 32 bits, so divide
// 128 with 32, or 128 >> 5
using TrophyFlagArray = uint32_t[MAX_TROPHIES >> 5];

enum class SceNpTrophyGrade : SceInt32 {
    SCE_NP_TROPHY_GRADE_UNKNOWN = 0,
    SCE_NP_TROPHY_GRADE_PLATINUM = 1,
    SCE_NP_TROPHY_GRADE_GOLD = 2,
    SCE_NP_TROPHY_GRADE_SILVER = 3,
    SCE_NP_TROPHY_GRADE_BRONZE = 4
};

struct Context {
    bool valid{ true };
    TRPFile trophy_file;
    CommunicationID comm_id;
    ContextHandle id;

    SceUID trophy_file_stream;

    TrophyFlagArray trophy_progress;
    TrophyFlagArray trophy_availability; ///< bit 1 set - hidden
    uint32_t group_count;
    uint32_t trophy_count;

    std::array<uint32_t, MAX_GROUPS> trophy_count_by_group;

    std::array<uint64_t, MAX_TROPHIES> unlock_timestamps;
    std::array<SceNpTrophyGrade, MAX_TROPHIES> trophy_kinds;

    int32_t platinum_trophy_id{ SCE_NP_TROPHY_INVALID_TROPHY_ID };

    std::string trophy_progress_output_file_path;
    std::string trophy_detail_xml;

    uint32_t lang{ 1 };

    IOState *io;
    fs::path pref_path;

    void save_trophy_progress_file();
    bool load_trophy_progress_file(const SceUID &progress_input_file);

    int copy_file_data_from_trophy_file(const char *filename, void *buffer, SceSize *size);
    int install_trophy_conf(IOState *io, const fs::path &pref_path, const std::string &np_com_id);
    bool init_info_from_trp();
    bool unlock_trophy(int32_t id, np::NpTrophyError *err, const bool force_unlock = false);

    bool is_trophy_hidden(const uint32_t &trophy_index);
    bool is_trophy_unlocked(const uint32_t &trophy_index);
    int total_trophy_unlocked();
    bool get_trophy_details(const int32_t id, std::string &name, std::string &detail);
    bool get_trophy_set(std::string &name, std::string &detail);

    explicit Context(const CommunicationID &comm_id, IOState *io, const SceUID trophy_stream,
        const std::string &output_progress_path);
    explicit Context() = default;
};

} // namespace np::trophy
