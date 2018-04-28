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

#include <kernel/thread_functions.h>
#include <mem/ptr.h>
#include <psp2/types.h>

#include <cstdint>

struct HostState;

bool init(HostState &state, std::uint32_t window_width, std::uint32_t border_width, std::uint32_t window_height, std::uint32_t border_height);
bool handle_events(HostState &host);
void call_import(HostState &host, uint32_t nid, SceUID thread_id);

// Needed because Sony decided to split Mutex functions to both LibKernel and ThreadMgr
int unlock_mutex(HostState &host, const char *export_name, SceUID thread_id, MutexPtrs &host_mutexes, SceUID mutexid, int unlock_count);
int delete_mutex(HostState &host, const char *export_name, SceUID thread_id, MutexPtrs &host_mutexes, SceUID mutexid);
int signal_sema(HostState& host, const char *export_name, SceUID semaid, int signal);
