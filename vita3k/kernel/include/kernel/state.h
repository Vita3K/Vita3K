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

#include <cpu/functions.h>
#include <kernel/sync_primitives.h>
#include <kernel/thread/thread_state.h>
#include <kernel/types.h>
#include <mem/allocator.h>
#include <mem/ptr.h>
#include <rtc/rtc.h>
#include <util/pool.h>

#include <atomic>
#include <kernel/object_store.h>
#include <map>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

struct ThreadState;

struct SDL_Thread;

struct WatchMemory;

struct InitialFiber;

struct CodecEngineBlock;

typedef std::shared_ptr<SceKernelMemBlockInfo> SceKernelMemBlockInfoPtr;
typedef std::map<SceUID, SceKernelMemBlockInfoPtr> Blocks;
typedef std::map<SceUID, CodecEngineBlock> CodecEngineBlocks;
typedef std::map<SceUID, Ptr<Ptr<void>>> SlotToAddress;
typedef std::map<SceUID, ThreadStatePtr> ThreadStatePtrs;
typedef std::shared_ptr<SDL_Thread> ThreadPtr;
typedef std::map<SceUID, ThreadPtr> ThreadPtrs;
typedef std::shared_ptr<SceKernelModuleInfo> SceKernelModuleInfoPtr;
typedef std::map<SceUID, SceKernelModuleInfoPtr> SceKernelModuleInfoPtrs;
typedef std::unordered_map<uint32_t, Address> ExportNids;
typedef std::map<Address, uint32_t> NidFromExport;
typedef std::map<Address, uint32_t> NotFoundVars;
typedef std::map<Address, WatchMemory> WatchMemoryAddrs;
typedef std::vector<ModuleRegion> ModuleRegions;
typedef Pool<CPUState> CPUPool;

struct CodecEngineBlock {
    uint32_t size;
    int32_t vaddr;
};

struct TimerState {
    std::string name;

    enum class ThreadBehaviour {
        FIFO,
        PRIORITY,
    };

    enum class NotifyBehaviour {
        ALL,
        ONLY_WAKE
    };

    enum class ResetBehaviour {
        MANUAL,
        AUTOMATIC,
    };

    bool openable = false;
    ThreadBehaviour thread_behaviour = ThreadBehaviour::FIFO;
    NotifyBehaviour notify_behaviour = NotifyBehaviour::ALL;
    ResetBehaviour reset_behaviour = ResetBehaviour::MANUAL;

    bool is_started = false;
    bool repeats = false;
    uint64_t time = 0;
};

typedef std::shared_ptr<TimerState> TimerPtr;
typedef std::map<SceUID, TimerPtr> TimerStates;

using LoadedSysmodules = std::vector<SceSysmoduleModuleId>;

struct WatchMemory {
    Address start;
    size_t size;
};

struct CorenumAllocator {
    BitmapAllocator alloc;
    std::mutex lock;

    void set_max_core_count(const std::size_t max);

    int new_corenum();
    void free_corenum(const int num);
};

struct KernelState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    CodecEngineBlocks codec_blocks;
    Ptr<const void> tls_address = Ptr<const void>(0);
    unsigned int tls_psize = 0;
    unsigned int tls_msize = 0;
    SemaphorePtrs semaphores;
    CondvarPtrs condvars;
    CondvarPtrs lwcondvars;
    MutexPtrs mutexes;
    MutexPtrs lwmutexes; // also Mutexes for now
    EventFlagPtrs eventflags;
    MsgPipePtrs msgpipes;
    ThreadStatePtrs threads;
    ThreadPtrs running_threads;
    KernelWaitingThreadStates waiting_threads;
    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    ExportNids export_nids;
    NidFromExport nid_from_export;
    NotFoundVars not_found_vars;
    WatchMemoryAddrs watch_memory_addrs;
    ModuleRegions module_regions;
    ExclusiveMonitorPtr exclusive_monitor;
    CPUPool cpu_pool;
    CPUBackend cpu_backend;
    bool cpu_opt;
    CPUProtocolBase *cpu_protocol;

    ObjectStore obj_store;

    SceRtcTick start_tick;
    SceRtcTick base_tick;
    TimerStates timers;
    Ptr<uint32_t> process_param;

    CorenumAllocator corenum_allocator;

    bool wait_for_debugger = false;
    bool watch_import_calls = false;
    bool watch_code = false;
    bool watch_memory = false;

    SceUID get_next_uid() {
        return next_uid++;
    }

private:
    std::atomic<SceUID> next_uid{ 0 };
};
