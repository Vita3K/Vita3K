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

#include <kernel/callback.h>
#include <kernel/cpu_protocol.h>
#include <kernel/debugger.h>
#include <kernel/object_store.h>
#include <kernel/sync_primitives.h>
#include <kernel/types.h>
#include <mem/allocator.h>
#include <mem/ptr.h>
#include <mem/util.h>
#include <rtc/rtc.h>
#include <util/containers.h>
#include <util/types.h>

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

struct ThreadState;

struct SDL_Thread;

struct CodecEngineBlock;

struct KernelModule {
    SceKernelModuleInfo info;
    Ptr<const uint8_t> info_segment_address;
    uint32_t info_offset;
};
typedef std::shared_ptr<KernelModule> SceKernelModulePtr;

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
typedef std::map<SceUID, CodecEngineBlock> CodecEngineBlocks;
typedef std::map<SceUID, Ptr<Ptr<void>>> SlotToAddress;
typedef std::map<SceUID, ThreadStatePtr> ThreadStatePtrs;
typedef std::shared_ptr<SDL_Thread> ThreadPtr;
typedef std::map<SceUID, ThreadPtr> ThreadPtrs;
typedef std::map<SceUID, SceKernelModulePtr> SceKernelModuleInfoPtrs;
typedef std::map<SceUID, CallbackPtr> CallbackPtrs;
typedef unordered_map_fast<uint32_t, Address> ExportNids;

typedef std::map<Address, uint32_t> NotFoundVars;
typedef std::unique_ptr<CPUProtocol> CPUProtocolPtr;

struct CodecEngineBlock {
    uint32_t size;
    int32_t vaddr;
};

using LoadedSysmodules = std::map<SceSysmoduleModuleId, std::vector<SceUID>>;
using LoadedInternalSysmodules = std::vector<SceSysmoduleInternalModuleId>;

struct CorenumAllocator {
    BitmapAllocator alloc;
    std::mutex lock;

    void set_max_core_count(const std::size_t max);

    int new_corenum();
    void free_corenum(const int num);
};

struct VarBindingInfo {
    void *entries;
    uint32_t size;
    uint32_t module_nid;
};

typedef std::multimap<uint32_t, VarBindingInfo> VarBindingInfos;
typedef std::multimap<uint32_t, Address> FuncBindingInfos;

typedef std::map<uint32_t, uint32_t> ModuleUidByNid;

struct KernelState {
    KernelState();

    std::mutex mutex;
    CodecEngineBlocks codec_blocks;

    Ptr<const void> tls_address = Ptr<const void>(0);
    unsigned int tls_psize = 0;
    unsigned int tls_msize = 0;

    Ptr<const void> thread_event_start = Ptr<const void>(0);
    Address thread_event_start_arg = 0;
    Ptr<const void> thread_event_end = Ptr<const void>(0);
    Address thread_event_end_arg = 0;

    SimpleEventPtrs simple_events;
    TimerPtrs timers;
    SemaphorePtrs semaphores;
    CondvarPtrs condvars;
    CondvarPtrs lwcondvars;
    MutexPtrs mutexes;
    MutexPtrs lwmutexes; // also Mutexes for now
    RWLockPtrs rwlocks;
    EventFlagPtrs eventflags;
    MsgPipePtrs msgpipes;
    CallbackPtrs callbacks;

    ThreadStatePtrs threads;

    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    LoadedInternalSysmodules loaded_internal_sysmodules;

    // the variables in this block must be accessed by first locking export_nids_mutex
    std::mutex export_nids_mutex;
    ExportNids export_nids;
    FuncBindingInfos func_binding_infos;
    VarBindingInfos var_binding_infos;
    ModuleUidByNid module_uid_by_nid;

    bool cpu_opt;
    CorenumAllocator corenum_allocator;
    CPUProtocolPtr cpu_protocol;
    ExclusiveMonitorPtr exclusive_monitor;

    ObjectStore obj_store;

    uint64_t start_tick;
    SceRtcTick base_tick;
    Ptr<SceProcessParam> process_param;

    Debugger debugger;

    SceUID get_next_uid() {
        return next_uid++;
    }

    bool init(MemState &mem, const CallImportFunc &call_import, bool cpu_opt);
    void load_process_param(MemState &mem, Ptr<uint32_t> ptr);
    ThreadStatePtr create_thread(MemState &mem, const char *name, Ptr<const void> entry_point = Ptr<const void>(0));
    ThreadStatePtr create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option);

    ThreadStatePtr get_thread(SceUID thread_id);
    Ptr<Ptr<void>> get_thread_tls_addr(MemState &mem, SceUID thread_id, int key);

    void exit_delete_all_threads();
    bool is_threads_paused() { return !paused_threads_status.empty(); }
    void pause_threads();
    void resume_threads();

    void set_memory_watch(bool enabled);
    void invalidate_jit_cache(Address start, size_t length);
    SceKernelModuleInfo *find_module_by_addr(Address address);

private:
    std::atomic<SceUID> next_uid{ 1 };
    std::map<SceUID, ThreadStatus> paused_threads_status;
};
