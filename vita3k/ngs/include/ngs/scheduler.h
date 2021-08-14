// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <util/types.h>

#include <mem/ptr.h>

#include <thread>

#include <optional>
#include <vector>

struct MemState;
struct KernelState;

namespace ngs {
struct PatchSetupInfo;
struct Voice;
struct Patch;

struct VoiceScheduler {
    std::vector<Voice *> queue;
    std::vector<Voice *> pending_deque;

    std::mutex lock;
    std::optional<std::thread::id> updater;

protected:
    bool deque_voice(Voice *voice);
    bool deque_voice_impl(Voice *voice);

    bool resort_to_respect_dependencies(const MemState &mem, Voice *source);

    std::int32_t get_position(Voice *v);

public:
    bool play(const MemState &mem, Voice *voice);
    bool pause(Voice *voice);
    bool resume(const MemState &mem, Voice *voice);
    bool stop(Voice *voice);
    bool off(Voice *voice);

    void update(KernelState &kern, const MemState &mem, const SceUID thread_id);

    Ptr<Patch> patch(const MemState &mem, PatchSetupInfo *info);
};
} // namespace ngs
