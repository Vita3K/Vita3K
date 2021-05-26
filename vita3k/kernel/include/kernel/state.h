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
#include <kernel/cpu_protocol.h>
#include <kernel/debugger.h>
#include <kernel/sync_primitives.h>
#include <kernel/types.h>
#include <mem/allocator.h>
#include <mem/ptr.h>
#include <mem/util.h>
#include <rtc/rtc.h>
#include <util/pool.h>

#include <atomic>
#include <kernel/object_store.h>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

struct ThreadState;

struct Breakpoint;

struct SDL_Thread;

struct WatchMemory;

struct InitialFiber;

struct CodecEngineBlock;

struct ThreadState;

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
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
typedef std::unique_ptr<CPUProtocol> CPUProtocolPtr;

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
    ThreadStatePtr guest_func_runner;

    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    ExportNids export_nids;
    NidFromExport nid_from_export;

    bool cpu_opt;
    CPUBackend cpu_backend;
    CorenumAllocator corenum_allocator;
    CPUProtocolPtr cpu_protocol;
    ExclusiveMonitorPtr exclusive_monitor;

    ObjectStore obj_store;

    uint64_t start_tick;
    SceRtcTick base_tick;
    TimerStates timers;
    Ptr<uint32_t> process_param;

    NotFoundVars not_found_vars;

    Debugger debugger;

    SceUID get_next_uid() {
        return next_uid++;
    }

    bool init(MemState &mem, CallImportFunc call_import, CPUBackend cpu_backend, bool cpu_opt);
    ThreadStatePtr create_thread(MemState &mem, const char *name);
    ThreadStatePtr create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, int stack_size, const SceKernelThreadOptParam *option);
    void exit_thread(ThreadStatePtr thread);
    void exit_delete_thread(ThreadStatePtr thread);

    ThreadStatePtr get_thread(SceUID thread_id);
    Ptr<Ptr<void>> get_thread_tls_addr(MemState &mem, SceUID thread_id, int key);
    void exit_delete_all_threads();

    int run_guest_function(Address callback_address, const std::vector<uint32_t> &args);

    void set_memory_watch(bool enabled);
    void invalidate_jit_cache(Address start, size_t length);
    std::shared_ptr<SceKernelModuleInfo> find_module_by_addr(Address address);

private:
    std::atomic<SceUID> next_uid{ 1 };
};
