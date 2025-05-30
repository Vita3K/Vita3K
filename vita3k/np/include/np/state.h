// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <np/trophy/context.h>

#include <mem/util.h> // Address.

#include <map>
#include <mutex>
#include <vector>

struct SceNpServiceStateCallback {
    Address pc;
    Address data;
};

typedef std::map<int, SceNpServiceStateCallback> np_callbacks;

struct NpTrophyUnlockCallbackData {
    std::string np_com_id;
    std::string trophy_id;
    std::string trophy_name;
    std::string trophy_detail;
    np::trophy::SceNpTrophyGrade trophy_kind{};
    std::vector<std::uint8_t> icon_buf;
};

using NpTrophyUnlockCallback = std::function<void(NpTrophyUnlockCallbackData &)>;

struct NpTrophyState {
    bool inited = false;
    std::mutex access_mutex;

    std::vector<np::trophy::Context> contexts;
    NpTrophyUnlockCallback trophy_unlock_callback;
};

enum SceNpServiceState : uint32_t {
    SCE_NP_SERVICE_STATE_UNKNOWN = 0,
    SCE_NP_SERVICE_STATE_SIGNED_OUT = 1,
    SCE_NP_SERVICE_STATE_SIGNED_IN = 2,
    SCE_NP_SERVICE_STATE_ONLINE = 3
};

struct NpState {
    bool inited = false;
    np_callbacks cbs;
    SceUID state_cb_id;

    NpTrophyState trophy_state;
    np::CommunicationID comm_id;
};
