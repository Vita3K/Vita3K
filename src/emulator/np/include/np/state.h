// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <np/profile/profile_manager.h>
#include <np/trophy/context.h>
#include <np/common.h>

#include <mem/mem.h> // Address.

#include <map>
#include <mutex>
#include <vector>

namespace emu {
struct SceNpServiceStateCallback {
    Address pc;
    Address data;
};
} // namespace emu

typedef std::map<int, emu::SceNpServiceStateCallback> np_callbacks;

struct NpManagerState {
    emu::np::profile::NPProfileManager profile_manager;
    std::mutex access_mutex;
};

struct NpTrophyState {
    bool inited = false;
    std::mutex access_mutex;

    std::vector<emu::np::trophy::Context> contexts;
};

struct NpState {
    bool inited = false;
    np_callbacks cbs;
    int state = -1;

    NpManagerState manager_state;
    NpTrophyState trophy_state;

    emu::np::CommunicationID comm_id;
};

