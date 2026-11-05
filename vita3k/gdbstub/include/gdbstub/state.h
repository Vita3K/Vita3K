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

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <util/types.h>

#ifdef _WIN32
#include <winsock.h>
#endif

#ifdef _WIN32
typedef SOCKET socket_t;
constexpr socket_t BAD_SOCK = INVALID_SOCKET;
#else
typedef int32_t socket_t;
constexpr socket_t BAD_SOCK = -1;
#endif

constexpr uint32_t GDB_SERVER_PORT = 2159;

struct GDBState {
    ~GDBState();

#ifdef _WIN32
    WSADATA wsaData;
    bool winsock_started = false;
#endif

    // 0 lets the system choose one, for the tests
    uint16_t port = GDB_SERVER_PORT;
    socket_t listen_socket = BAD_SOCK;
    socket_t client_socket = BAD_SOCK;

    std::shared_ptr<std::thread> server_thread = nullptr;
    std::mutex server_thread_mutex;
    bool server_die = false;

    std::string last_reply = "";
    std::string input_buffer;
    int thread_info_index = 0;

    SceUID current_thread = 0;

    // Saves the current frame as a PNG. The frontend sets it, since the frame is read on its thread.
    std::function<bool(const std::string &)> save_frame;

    // Frames the next continue runs for before stopping, 0 for no limit
    uint32_t stop_after_frames = 0;

    // vFile fd cache. Each open allocates a fresh fd and stores the
    // file's bytes in memory; pread/fstat/close operate on these buffers.
    // Lets us serve decrypted ELFs for Vita modules from KernelModule's
    // elf_bytes cache, transparent to the gdb client.
    int next_vfile_fd = 1;
    std::map<int, std::vector<uint8_t>> vfile_buffers;
};
