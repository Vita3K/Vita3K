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

#include <cpu/common.h>
#include <kernel/callback.h>
#include <kernel/debugger.h>
#include <kernel/object_store.h>
#include <kernel/sync_primitives.h>
#include <kernel/types.h>
#include <mem/allocator.h>
#include <mem/block.h>
#include <mem/ptr.h>
#include <mem/util.h>
#include <rtc/rtc.h>
#include <util/containers.h>
#include <util/types.h>

#include <emuenv/app_launch_request.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

struct ThreadState;
struct MemState;

struct CodecEngineBlock;

struct KernelModule {
    SceKernelModuleInfo info;
    Ptr<const uint8_t> info_segment_address;
    uint32_t info_offset;
};
using SceKernelModulePtr = std::shared_ptr<KernelModule>;

using ThreadStateRef = Ref<ThreadState>;
using CodecEngineBlocks = std::map<SceUID, CodecEngineBlock>;
using SlotToAddress = std::map<SceUID, Ptr<Ptr<void>>>;
using ThreadStateRefs = std::map<SceUID, ThreadStateRef>;
using SceKernelModuleInfoPtrs = std::map<SceUID, SceKernelModulePtr>;
using ExportNids = unordered_map_fast<uint32_t, Address>;
// A NID hashes the function name alone, so same-named exports from different libraries collide.
using LibExportNids = unordered_map_fast<uint64_t, Address>;
// The plain NID entry outlives taiHEN and HLE redirects, so its owner is tracked apart from its address.
using ExportNidOwners = unordered_map_fast<uint32_t, uint32_t>;
constexpr uint64_t lib_export_key(uint32_t library_nid, uint32_t nid) {
    return (static_cast<uint64_t>(library_nid) << 32) | nid;
}

using NotFoundVars = std::map<Address, uint32_t>;
using CallImportFunc = std::function<CallResult(CPUState &cpu, uint32_t nid, SceUID thread_id)>;

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

struct FuncBindingInfo {
    Address entry_address;
    uint32_t library_nid;
};

using VarBindingInfos = std::multimap<uint32_t, VarBindingInfo>;
using FuncBindingInfos = std::multimap<uint32_t, FuncBindingInfo>;

using ModuleUidByNid = std::map<uint32_t, uint32_t>;

struct KernelState {
    KernelState();

    std::mutex mutex;
    CodecEngineBlocks codec_blocks;

    Ptr<const void> tls_address = Ptr<const void>(0);
    unsigned int tls_psize = 0;
    unsigned int tls_msize = 0;

    // Thread manager objects other than threads, by UID
    KernelObjects objects;
    // The objects the kernel made for itself, which no guest UID reaches
    KernelObjects kernel_objects;

    ThreadStateRefs threads;
    void *jni_env;
    void *jni_activity;

    SceKernelModuleInfoPtrs loaded_modules;
    LoadedSysmodules loaded_sysmodules;
    LoadedInternalSysmodules loaded_internal_sysmodules;

    // the variables in this block must be accessed by first locking export_nids_mutex
    std::mutex export_nids_mutex;
    ExportNids export_nids;
    LibExportNids export_nids_by_lib;
    ExportNidOwners export_nid_owners;
    FuncBindingInfos func_binding_infos;
    VarBindingInfos var_binding_infos;
    ModuleUidByNid module_uid_by_nid;

    bool cpu_opt;
    CorenumAllocator corenum_allocator;
    CallImportFunc call_import;

    // Thumb NOP and WFI that guest functions called by the kernel return to
    Block halt_instruction;

    ObjectStore obj_store;

    uint64_t start_tick;
    SceRtcTick base_tick;
    // Returns the process time at a time of the host clock that timers count with
    uint64_t process_time_at(uint64_t clock_time) const;
    Ptr<SceProcessParam> process_param;
    Ptr<void> client_vtable = Ptr<void>(0);
    Ptr<Address> shellsvc_client = Ptr<Address>(0);
    Ptr<void> libc_dso_handle_main = Ptr<void>(0);

    Debugger debugger;

    // kubridge exception handlers (DABT=0, PABT=1, UNDEF=2)
    static constexpr int EXCEPTION_HANDLER_MAX = 3;
    std::atomic<Address> exception_handlers[EXCEPTION_HANDLER_MAX]{};
    std::condition_variable thread_deleted_cond;

    // The next UID of any kind, memory blocks too, in the Vita's form: positive, odd and with bit 30
    // set
    SceUID get_next_uid() {
        return static_cast<SceUID>(0x40000000 | next_uid++ << 1 | 1);
    }

    bool init(MemState &mem, const CallImportFunc &call_import, bool cpu_opt);
    void deinit(MemState &mem);
    void load_process_param(MemState &mem, Ptr<uint32_t> ptr);
    // Creates a dormant thread with these creation arguments, attr without
    // SCE_KERNEL_THREAD_ATTR_USER, or returns null
    ThreadStateRef create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr);

    ThreadStateRef get_thread(SceUID thread_id);

    Ptr<Ptr<void>> get_thread_tls_addr(MemState &mem, SceUID thread_id, int key);

    // Whether the emulator pause holds every thread. Threads read it without mutex, under their own lock.
    bool is_threads_paused() const { return threads_paused; }
    // Pauses every thread, and the threads created until the pause ends, or lets them run again
    void set_threads_paused(bool paused);
    // Stops every thread for the debugger, and the threads created until it lets them go, or lets
    // them run again
    void set_threads_suspended(bool suspended);

    // Adds handler to the objects and the list under a new UID and returns the UID
    SceUID register_thread_event_handler(ThreadEventHandlerRef handler);
    // Deletes handler, false if it is no longer listed. A handler still running is unlisted when
    // its last run ends.
    bool unregister_thread_event_handler(ThreadEventHandler &handler);

    // The registered thread event handlers in registration order. A handler stays listed while it runs.
    std::mutex thread_event_handlers_mutex;
    RefList<ThreadEventHandler> thread_event_handlers;

    // Kill all guest threads and block until they have exited. Must only be called from a host thread.
    void process_exit();
    std::function<void(int, std::optional<AppLaunchRequest>)> process_exit_callback;
    // Ends the app from any thread without waiting, with no thread mutex or object lock held
    void request_process_exit(int res, std::optional<AppLaunchRequest> relaunch = std::nullopt);

    void set_memory_watch(bool enabled);
    void invalidate_jit_cache(Address start, size_t length);
    SceKernelModuleInfo *find_module_by_addr(Address address);

private:
    std::atomic<uint32_t> next_uid{ 1 };
    std::atomic<bool> threads_paused{ false };
    bool threads_suspended = false;
    // The process is exiting, so a thread still being created ends as well
    bool process_exiting = false;
};
