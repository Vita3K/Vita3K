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
#include <kernel/codec_engine_block.h>
#include <kernel/debugger.h>
#include <kernel/object_store.h>
#include <kernel/relocation.h>
#include <kernel/scheduler.h>
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

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

struct ThreadState;
struct MemState;
struct SecondaryMemblockState;

struct ModuleImportSnapshot {
    uint32_t library_nid = 0;
    std::vector<uint32_t> function_nids;
    std::vector<Ptr<uint32_t>> function_entries;
    std::vector<std::vector<uint32_t>> function_relocations;
    std::vector<uint32_t> variable_nids;
    std::vector<Ptr<uint32_t>> variable_entries;
    std::vector<std::vector<uint32_t>> variable_relocations;
};

struct ModuleExportSnapshot {
    uint32_t library_nid = 0;
    size_t function_count = 0;
    std::vector<uint32_t> nids;
    std::vector<Ptr<uint32_t>> entries;
};

struct ModuleLinkageSnapshot {
    std::vector<ModuleImportSnapshot> imports;
    std::vector<ModuleExportSnapshot> exports;
};

struct KernelModule {
    explicit KernelModule(MemState &mem)
        : address_space(&mem) {}

    SceKernelModuleInfo info{};
    Ptr<const uint8_t> info_segment_address;
    uint32_t info_offset = 0;
    uint32_t module_nid = 0;
    MemState *const address_space;
    std::optional<ModuleLinkageSnapshot> linkage_snapshot;
    std::array<std::vector<uint8_t>, MODULE_INFO_NUM_SEGMENTS> shared_initial_segments;
    // Acquired under module_linker_mutex so unload cannot free segments during startup.
    std::atomic_uint startup_references{ 0 };
};
using SceKernelModulePtr = std::shared_ptr<KernelModule>;

class ModuleStartLease {
public:
    ModuleStartLease(const ModuleStartLease &) = delete;
    ModuleStartLease &operator=(const ModuleStartLease &) = delete;
    ModuleStartLease(ModuleStartLease &&other) noexcept
        : module_(std::move(other.module_)) {}
    ~ModuleStartLease() {
        if (module_)
            --module_->startup_references;
    }
    const SceKernelModulePtr &module() const { return module_; }

private:
    friend struct KernelState;
    explicit ModuleStartLease(SceKernelModulePtr module)
        : module_(std::move(module)) { ++module_->startup_references; }
    SceKernelModulePtr module_;
};

struct ProcessStartModules {
    ModuleStartLease main;
    std::vector<SceKernelModulePtr> preloaded;
};

using ThreadStatePtr = std::shared_ptr<ThreadState>;
enum class ProcessStartResult {
    Success,
    MainModuleUnavailable,
    MainThreadCreationFailed,
    MainThreadStartFailed,
};

using CodecEngineBlocks = std::map<SceUID, CodecEngineBlock>;
using SlotToAddress = std::map<SceUID, Ptr<Ptr<void>>>;
using ThreadStatePtrs = std::map<SceUID, ThreadStatePtr>;
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
    SegmentInfosForReloc segments;
    std::vector<uint32_t> relocation_entries;
};

struct FuncBindingInfo {
    Address entry_address;
    uint32_t library_nid;
};

struct ThreadTlsInfo {
    Ptr<const void> address = Ptr<const void>(0);
    unsigned int initialized_size = 0;
    unsigned int total_size = 0;
};

struct ProcessIdentity {
    std::string app_path;
    SceUID pid = 0;

    bool is_shell() const { return app_path == "vs0:vsh/shell"; }
    bool is_firmware_app() const { return is_shell() || app_path.starts_with("vs0:app/"); }
};

struct LibcExitHandler {
    Ptr<void> function;
    Ptr<void> object;
    Ptr<void> dso;
};

using VarBindingInfos = std::multimap<uint32_t, VarBindingInfo>;
using FuncBindingInfos = std::multimap<uint32_t, FuncBindingInfo>;
using VariableStubs = std::map<uint32_t, Address>;

struct VariableExportLayer {
    Address module_segment;
    Address address;
};

struct VariableExportStack {
    std::optional<Address> fallback;
    std::vector<VariableExportLayer> layers;
};
using VariableExportStacks = std::map<uint32_t, VariableExportStack>;

using ModuleUidByNid = std::map<uint32_t, uint32_t>;
using ModuleUidLayers = std::map<uint32_t, std::vector<uint32_t>>;

struct ModuleBindingTables {
    ExportNids exports;
    LibExportNids library_exports;
    ExportNidOwners export_owners;
    FuncBindingInfos function_imports;
    VarBindingInfos variable_imports;
    VariableStubs variable_stubs;
    VariableExportStacks variable_exports;
};

struct ProcessIpmiObjects {
    Block client_vtable;
    Block shellsvc_client;
};

struct ModuleBindingView {
    ExportNids &exports;
    LibExportNids &library_exports;
    ExportNidOwners &export_owners;
    FuncBindingInfos &function_imports;
    VarBindingInfos &variable_imports;
    VariableStubs &variable_stubs;
    VariableExportStacks &variable_exports;
};

struct KernelState {
    KernelState();

    std::mutex mutex;
    CodecEngineBlocks codec_blocks;

    Ptr<const void> thread_event_start = Ptr<const void>(0);
    Address thread_event_start_arg = 0;
    Ptr<const void> thread_event_end = Ptr<const void>(0);
    Address thread_event_end_arg = 0;

    // Thread manager objects other than threads, by UID
    KernelObjects objects;

    ThreadStatePtrs threads;
    void *jni_env;
    void *jni_activity;

    SceKernelModuleInfoPtrs loaded_modules;
    // Both maps are protected by mutex and keyed by the owning address space.
    std::map<const MemState *, LoadedSysmodules> loaded_sysmodules;
    std::map<const MemState *, LoadedInternalSysmodules> loaded_internal_sysmodules;

    std::mutex module_linker_mutex;
    // the variables in this block must be accessed by first locking export_nids_mutex
    std::mutex export_nids_mutex;
    ExportNids export_nids;
    LibExportNids export_nids_by_lib;
    ExportNidOwners export_nid_owners;
    FuncBindingInfos func_binding_infos;
    VarBindingInfos var_binding_infos;
    VariableStubs variable_stubs;
    VariableExportStacks variable_exports;
    std::map<const MemState *, ModuleUidByNid> module_uid_by_nid;
    std::map<const MemState *, ModuleUidLayers> module_uid_layers;
    std::map<const MemState *, ModuleBindingTables> secondary_module_bindings;

    // The caller holds export_nids_mutex while using the returned references.
    ModuleBindingView module_bindings(const MemState &mem);

    bool cpu_opt;
    CorenumAllocator corenum_allocator;
    Scheduler sched;
    CallImportFunc call_import;

    ObjectStore obj_store;

    uint64_t start_tick;
    SceRtcTick base_tick;
    // Returns the process time at a time of the host clock that timers count with
    uint64_t process_time_at(uint64_t clock_time) const;
    // Protected by mutex; guest allocations must be released before their MemState.
    std::map<MemState *, ProcessIpmiObjects> ipmi_objects_by_memory;
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

    bool init(MemState &mem, const CallImportFunc &call_import, bool cpu_opt, bool thread_scheduler);
    void deinit(MemState &mem);
    // The caller holds export_nids_mutex when loading a module's process parameters.
    void load_process_param(MemState &mem, Ptr<uint32_t> ptr);
    // The caller holds export_nids_mutex when removing a module's process parameters.
    void unload_process_param(MemState &mem, Ptr<uint32_t> ptr);
    Ptr<SceProcessParam> get_process_param(MemState &mem);
    bool register_process_identity(MemState &mem, ProcessIdentity identity);
    std::optional<ProcessIdentity> get_process_identity(MemState &mem);
    std::shared_ptr<SecondaryMemblockState> secondary_memblocks(MemState &mem, std::shared_ptr<SecondaryMemblockState> candidate);
    std::optional<uint64_t> process_elapsed_time_at(MemState &mem, uint64_t clock_tick);
    bool set_libc_dso_handle_main(MemState &mem, Ptr<void> handle);
    std::optional<Ptr<void>> get_libc_dso_handle_main(MemState &mem);
    bool register_libc_exit_handler(MemState &mem, LibcExitHandler handler);
    std::optional<LibcExitHandler> take_libc_exit_handler(MemState &mem, Ptr<void> dso);
    void set_thread_tls_info(MemState &mem, ThreadTlsInfo info);
    ThreadTlsInfo get_thread_tls_info(MemState &mem);
    // Call before destroying an address space, after its threads have stopped.
    void clear_thread_tls_info(MemState &mem);
    // Blocks until this address space's guest threads and in-flight creations have ended. Host only.
    void stop_threads_in_address_space(MemState &mem);
    // Release thread resources after stop_threads_in_address_space; the caller must prevent new
    // uses of the address space, unload its modules, and release outstanding ThreadState references
    // before destroying it.
    bool release_thread_address_space(MemState &mem);
    // Creates a dormant thread with these creation arguments, attr without
    // SCE_KERNEL_THREAD_ATTR_USER, or returns null
    ThreadStatePtr create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr);
    ThreadStatePtr create_process_main_thread(MemState &mem, const char *name, Ptr<const void> entry_point);
    SceInt32 start_process_main_thread(const ThreadStatePtr &thread, std::span<const std::string> args);
    ProcessStartResult start_process(MemState &mem, const char *name, SceUID main_module_id, std::span<const std::string> args,
        const std::function<void(const ThreadStatePtr &)> &on_main_created,
        const std::function<void(const KernelModule &)> &start_module);
    SceInt32 run_module_entry(const KernelModule &module, bool start, SceSize args, Ptr<const void> argp);
    ThreadStatePtr create_thread_from_caller(const ThreadStatePtr &caller, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr);

    ThreadStatePtr get_thread(SceUID thread_id);
    std::optional<SceUID> get_thread_process_id(SceUID thread_id);
    bool is_primary_address_space(const MemState &mem) const { return primary_address_space == &mem; }
    SceInt32 check_primary_address_space_access(SceUID thread_id, const char *operation);
    SceInt32 check_primary_process_target(SceUID thread_id, SceUID target_pid, const char *operation);
    // The session module loader currently only supports the primary address space.
    SceInt32 check_primary_module_loader(SceUID thread_id);

    Ptr<Ptr<void>> get_thread_tls_addr(SceUID thread_id, int key);
    Ptr<Ptr<void>> get_thread_tls_addr(MemState &mem, SceUID thread_id, int key);

    // Whether the emulator pause holds every thread. Threads read it without mutex, under their own lock.
    bool is_threads_paused() const { return threads_paused; }
    // Pauses every thread, and the threads created until the pause ends, or lets them run again
    void set_threads_paused(bool paused);
    // Stops every thread for the debugger, and the threads created until it lets them go, or lets
    // them run again
    void set_threads_suspended(bool suspended);

    // Kill all guest threads and block until they have exited. Must only be called from a host thread.
    void process_exit();
    std::function<void(int, std::optional<AppLaunchRequest>)> process_exit_callback;
    // The session callback is only valid for the primary address space until targeted exit exists.
    SceInt32 request_process_exit(SceUID caller_id, int res, std::optional<AppLaunchRequest> relaunch = std::nullopt);

    void set_memory_watch(bool enabled);
    void invalidate_jit_cache(MemState &mem, Address start, size_t length);
    SceKernelModulePtr find_module(const MemState &mem, SceUID module_id);
    std::optional<ProcessStartModules> get_process_start_modules(const MemState &mem, SceUID main_module_id);
    std::optional<ModuleStartLease> acquire_module_start(const MemState &mem, const SceKernelModulePtr &module);
    SceKernelModulePtr find_module_by_path(const MemState &mem, const std::string &path);
    SceKernelModulePtr find_module_by_addr(const MemState &mem, Address address);

private:
    struct ThreadAddressSpaceState {
        size_t creating = 0;
        bool stopping = false;
        std::optional<ProcessIdentity> identity;
        uint64_t start_tick = 0;
        Ptr<void> libc_dso_handle_main{};
        std::vector<LibcExitHandler> libc_exit_handlers;
        bool libc_exit_warning_logged = false;
        std::shared_ptr<SecondaryMemblockState> memblocks;
    };
    Address get_thread_return_code(MemState &mem);
    std::mutex process_param_mutex;
    std::map<MemState *, Ptr<SceProcessParam>> process_params;
    struct ProcessLibcExport {
        Address published;
        std::optional<Address> previous;
    };
    std::map<MemState *, ProcessLibcExport> process_libc_exports;
    MemState *primary_address_space = nullptr;
    std::map<MemState *, ThreadTlsInfo> thread_tls_by_memory;
    std::map<MemState *, Block> thread_return_code_by_memory;
    std::map<MemState *, ThreadAddressSpaceState> thread_address_spaces;
    SceUID next_process_id = 1;
    std::atomic<uint32_t> next_uid{ 1 };
    std::atomic<bool> threads_paused{ false };
    bool threads_suspended = false;
    // The process is exiting, so a thread still being created ends as well
    bool process_exiting = false;
};
