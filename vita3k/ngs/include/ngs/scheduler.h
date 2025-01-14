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

#include <util/types.h>

#include <mem/ptr.h>

#include <condition_variable>
#include <queue>
#include <vector>

struct MemState;
struct KernelState;

struct SceNgsPatchSetupInfo;

namespace ngs {
struct Voice;
struct Patch;
struct Rack;
struct System;
struct State;

enum class PendingType {
    ReleaseRack
    // TODO add voice operations during system lock
};

struct OperationPending {
    PendingType type;
    System *system;
    union {
        // ReleaseRack
        struct {
            State *state;
            Rack *rack;
            // can't use a ptr, otherwise the default constructor is deleted
            Address callback;
        } release_data;
    };
};

struct VoiceScheduler {
    std::vector<Voice *> queue;
    std::queue<OperationPending> operations_pending;

    std::recursive_mutex mutex;
    std::condition_variable_any condvar;
    bool is_updating = false;

protected:
    void deque_insert(const MemState &mem, Voice *voice);

    bool resort_to_respect_dependencies(const MemState &mem, Voice *source);

    std::int32_t get_position(Voice *v);

public:
    bool deque_voice(Voice *voice);

    bool play(const MemState &mem, Voice *voice);
    bool pause(const MemState &mem, Voice *voice);
    bool resume(const MemState &mem, Voice *voice);
    bool stop(const MemState &mem, Voice *voice);
    bool off(const MemState &mem, Voice *voice);

    void update(KernelState &kern, const MemState &mem, const SceUID thread_id);

    Ptr<Patch> patch(const MemState &mem, SceNgsPatchSetupInfo *info);
};
} // namespace ngs
