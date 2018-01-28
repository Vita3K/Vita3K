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

#include <host/state.h>
#include <kernel/thread_state.h>
#include <util/lock_and_find.h>

#include <microprofile.h>

#include <cassert>

struct CPUState;

template <typename... Args>
struct ArgLayout;

template <typename Ret>
struct BridgeReturn;

template <typename Fn, Fn, typename Indices>
struct CallAndBridgeReturn;

// Function returns a value that requires bridging.
template <typename Ret, typename... Args, Ret (*export_fn)(HostState &, SceUID, Args...), size_t... Indices>
struct CallAndBridgeReturn<Ret (*)(HostState &, SceUID, Args...), export_fn, std::index_sequence<Indices...>> {
    static void call(SceUID thread_id, CPUState &cpu, HostState &host) {
        using ArgLayoutType = ArgLayout<Args...>;
        const Ret ret = (*export_fn)(host, thread_id, ArgLayoutType::template read<Args, Indices>(cpu, host.mem)...);
        BridgeReturn<Ret>::write(cpu, ret);
    }
};

// Function does not return a value.
template <typename... Args, void (*export_fn)(HostState &, SceUID, Args...), size_t... Indices>
struct CallAndBridgeReturn<void (*)(HostState &, SceUID, Args...), export_fn, std::index_sequence<Indices...>> {
    static void call(SceUID thread_id, CPUState &cpu, HostState &host) {
        using ArgLayoutType = ArgLayout<Args...>;
        (*export_fn)(host, thread_id, ArgLayoutType::template read<Args, Indices>(cpu, host.mem)...);
    }
};

template <typename FnPtr, FnPtr>
struct Bridge;

template <typename Ret, typename... Args, Ret (*export_fn)(HostState &, SceUID, Args...)>
struct Bridge<Ret (*)(HostState &, SceUID, Args...), export_fn> {
    static void call(HostState &host, SceUID thread_id) {
        MICROPROFILE_SCOPEI("HLE", "", MP_YELLOW);

        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        assert(thread);

        using Indices = std::index_sequence_for<Args...>;
        CallAndBridgeReturn<Ret (*)(HostState &, SceUID, Args...), export_fn, Indices>::call(thread_id, *thread->cpu, host);
    }
};
