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

#include <v3kn/account.h>
#include <v3kn/storage.h>

#include <util/net_utils.h>

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct AccountState {
    UserInfo user_info;
    std::atomic<bool> is_logged_in{ false };
    uint32_t selected_server_index = 0;
};

struct StorageState {
    OnlineStorageState online_storage_state = ONLINE_STORAGE_SELECT;
    std::map<SaveDataType, SaveDataInfo> savedata_info;
    std::atomic<float> progress{ 0 };
    std::atomic<uint64_t> remaining{ 0 };
    std::atomic<uint64_t> bytes_done{ 0 };
    net_utils::ProgressState progress_state{};

    uint64_t progress_done_timestamp = 0;
    uint64_t please_wait_timestamp = 0;
    std::atomic<bool> please_wait_done{ false };
    std::mutex mutex_progress;
    std::condition_variable cv_progress;
};

struct V3KNState {
    AccountState account_state;
    StorageState storage_state;
};
