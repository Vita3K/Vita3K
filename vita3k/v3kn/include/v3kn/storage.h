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

#include <atomic>
#include <ctime>
#include <map>
#include <string>
#include <vector>

struct EmuEnvState;
struct GuiState;

struct SaveEntry {
    std::string rel_path;
    time_t last_updated;
};

struct SaveDataInfo {
    time_t last_updated;
    time_t last_uploaded;
    uint64_t total_size;
    std::vector<SaveEntry> files;
};

struct TrophySync {
    time_t last_updated;
    std::vector<std::string> unlocked_ids;
    uint32_t platinum = 0;
    uint32_t gold = 0;
    uint32_t silver = 0;
    uint32_t bronze = 0;
    uint32_t progress = 0;
};

struct TrophyProgressInfo {
    std::map<std::string, uint32_t> progress = {};
    std::map<std::string, std::vector<std::string>> unlocked_ids = {};
    std::map<std::string, std::map<np::trophy::SceNpTrophyGrade, uint32_t>> unlocked_type_count = {};
    time_t last_updated;
};

enum SaveDataType {
    SAVE_DATA_TYPE_CLOUD = 0,
    SAVE_DATA_TYPE_LOCAL = 1,
};

enum OnlineStorageState {
    ONLINE_STORAGE_SELECT,
    ONLINE_STORAGE_UPLOAD,
    ONLINE_STORAGE_DOWNLOAD,
    ONLINE_STORAGE_COPYING,
};

bool load_and_compute_trophy_stats(EmuEnvState &emuenv, np::trophy::Context &context, const std::string &np_com_id, TrophyProgressInfo &trophy_progress_info);

namespace v3kn {
void start_trophy_sync(GuiState &gui, EmuEnvState &emuenv, std::map<std::string, TrophySync> &trophy_progress, std::atomic<float> &sync_progress, std::atomic<bool> &sync_cancel, std::vector<std::string> &sync_downloaded);
void start_trophy_auto_sync(GuiState &gui, EmuEnvState &emuenv);
void add_pending_trophy_sync(EmuEnvState &emuenv, const std::string &np_com_id);
void download_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid);
void upload_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid);
void open_online_storage(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid);
void sync_game_stitle(EmuEnvState &emuenv);

} // namespace v3kn
