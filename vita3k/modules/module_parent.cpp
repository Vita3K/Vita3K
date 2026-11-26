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

#include "io/functions.h"
#include "io/io.h"

#include <modules/module_parent.h>

#include <cpu/arm.h>
#include <cpu/state.h>
#include <emuenv/state.h>
#include <io/device.h>
#include <io/state.h>
#include <io/vfs.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <module/load_module.h>
#include <nids/functions.h>
#include <packages/license.h>
#include <packages/sce_types.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <unordered_set>

static constexpr bool LOG_UNK_NIDS_ALWAYS = false;

#define LIBRARY(name) extern const LibraryInitFn import_library_init_##name;
#include <modules/library_init_list.inc>
#undef LIBRARY

#define VAR_NID(name, nid) extern const ImportVarFactory import_##name;
#define NID(name, nid) extern const ImportFn import_##name;
#include <nids/nids.inc>
#undef NID
#undef VAR_NID

struct EmuEnvState;

static const ImportFn *resolve_import(uint32_t nid) {
    switch (nid) {
#define VAR_NID(name, nid)
#define NID(name, nid) \
    case nid:          \
        return &import_##name;
#include <nids/nids.inc>
#undef NID
#undef VAR_NID
    default:
        return nullptr;
    }
}

bool has_hle_implementation(uint32_t nid) {
    return resolve_import(nid) != nullptr;
}

struct VarExport {
    uint32_t nid;
    ImportVarFactory factory;
};

void init_exported_vars(EmuEnvState &emuenv) {
    const auto var_exports = std::to_array<VarExport>({
#define NID(name, nid)
#define VAR_NID(name, nid) \
    {                      \
        nid,               \
        import_##name      \
    },
#include <nids/nids.inc>
#undef VAR_NID
#undef NID
    });

    for (const auto &var : var_exports) {
        auto addr = var.factory(emuenv);
        emuenv.kernel.export_nids.emplace(var.nid, addr);
    }
}

Ptr<void> create_vtable(const std::vector<uint32_t> &nids, MemState &mem) {
    // we need 4 bytes for the function pointer and 12 bytes for the syscall
    const uint32_t vtable_size = nids.size() * 4 * sizeof(uint32_t);
    Ptr<void> vtable = Ptr<void>(alloc(mem, vtable_size, "vtable"));
    uint32_t *function_pointer = vtable.cast<uint32_t>().get(mem);
    uint32_t *function_svc = function_pointer + nids.size();
    uint32_t function_location = vtable.address() + nids.size() * sizeof(uint32_t);
    for (uint32_t nid : nids) {
        *function_pointer = function_location;
        // encode svc call
        function_svc[0] = arm::ARM_SVC; // Call our interrupt hook.
        function_svc[1] = arm::ARM_MOV_PC_LR; // Return to the caller.
        function_svc[2] = nid; // Our interrupt hook will read this.

        function_pointer++;
        function_svc += 3;
        function_location += 3 * sizeof(uint32_t);
    }
    return vtable;
}

Ptr<void> get_client_vtable(KernelState &kernel, MemState &mem) {
    if (!kernel.client_vtable) {
        kernel.client_vtable = create_vtable({
                                                 0x101C93F8, // destroy
                                                 0xA22C3E01, // connect
                                                 0xEC73331C, // disconnect
                                                 0xD484D36D, // terminateConnection
                                                 0x28BD5F19, // invokeSyncMethod
                                                 0x73C72FBB, // invokeSyncMethod
                                                 0xAFD10F3B, // invokeAsyncMethod
                                                 0x387AFA3F, // invokeAsyncMethod
                                                 0xF8C2B8BA, // tryGetResult
                                                 0x4EBB01A2, // tryGetResult
                                                 0x8FF23C3C, // pollEventFlag
                                                 0x45C32034, // waitEventFlag
                                                 0x004F48ED, // getUserData
                                                 0xA3E650B0, // getMsg
                                                 0x60EFADE7, // tryGetMsg
                                                 0xA5AA193C, // ~Client
                                             },
            mem);
    }

    return kernel.client_vtable;
}

static void log_import_call(char emulation_level, uint32_t nid, SceUID thread_id, const std::unordered_set<uint32_t> &nid_blacklist, Address lr) {
    if (!nid_blacklist.contains(nid)) {
        const char *const name = import_name(nid);
        LOG_TRACE("[{}LE] TID: {:<3} FUNC: {} {} at {}", emulation_level, thread_id, log_hex(nid), name, log_hex(lr));
    }
}

CallResult call_import(EmuEnvState &emuenv, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    // HLE - call our C++ function
    if (emuenv.kernel.debugger.watch_import_calls) {
        const std::unordered_set<uint32_t> hle_nid_blacklist = {
            0xB295EB61, // sceKernelGetTLSAddr
            0x46E7BE7B, // sceKernelLockLwMutex
            0x91FA6614, // sceKernelUnlockLwMutex
        };
        auto lr = cpu.get_lr();
        log_import_call('H', nid, thread_id, hle_nid_blacklist, lr);
    }
    const ImportFn *fn = resolve_import(nid);
    if (fn)
        return (*fn)(emuenv, cpu, thread_id);

    if (!emuenv.missing_nids.contains(nid) || LOG_UNK_NIDS_ALWAYS) {
        const ThreadStateRef thread = emuenv.kernel.get_thread(thread_id);
        LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);
        if (!LOG_UNK_NIDS_ALWAYS)
            emuenv.missing_nids.insert(nid);
    }
    // A missing function returns 0
    return { .regs = { 0 }, .count = 1 };
}

struct SceKernelBootimageModules {
    Ptr<const char> path;
    Ptr<const void> data;
    SceSize size;
};

struct SceKernelBootimageInfo {
    SceSize number;
    Ptr<const SceKernelBootimageModules> list;
};

constexpr uint32_t nid_sceKernelBootimageInfo = 0x9C08E88A;

SceUID load_module(EmuEnvState &emuenv, const std::string &module_path, uint16_t flags) {
    LOG_INFO("Loading module \"{}\"", module_path);
    if (module_path.starts_with("vs0:sys/external/")) {
        // check if module is LLEd or not
        // check only for this path because app modules are always LLEd and os0 modules can be loaded only by developer.
        const auto module_name = fs::path(module_path).stem().string();
        if (!is_lle_module(module_name, emuenv)) {
            LOG_INFO("Module {} is HLE. Skipping load.", module_name);
            const SceKernelModulePtr kernelModuleInfo = std::make_shared<KernelModule>();

            auto *sceKernelModuleInfo = &kernelModuleInfo->info;
            sceKernelModuleInfo->size = sizeof(*sceKernelModuleInfo);
            strncpy(sceKernelModuleInfo->path, module_path.c_str(), sizeof(sceKernelModuleInfo->path) - 1);
            // module_name is stubbed.
            strncpy(sceKernelModuleInfo->module_name, module_name.c_str(), 28);
            kernelModuleInfo->flags = flags;
            kernelModuleInfo->set_state(ModuleState::loaded);

            const SceUID uid = emuenv.kernel.get_next_uid();
            sceKernelModuleInfo->modid = uid;
            {
                const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
                emuenv.kernel.loaded_modules[uid] = kernelModuleInfo;
            }
            return uid;
        }
    }

    const auto load_module_data = [&](const void *module_data) -> SceUID {
        SceUID module_id = load_self(emuenv.kernel, emuenv.mem, module_data, module_path, emuenv.log_path / "elfdumps" / emuenv.io.title_id, flags);

        if (module_id >= 0) {
            const auto module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
            LOG_INFO("Module {} (at \"{}\") loaded", module->info.module_name, module_path);
        } else {
            LOG_ERROR("Failed to load module {}", module_path);
        }

        return module_id;
    };

    VitaIoDevice device = device::get_device(module_path);
    auto device_for_icase = device;
    fs::path translated_module_path = translate_path(module_path.c_str(), device, emuenv.io.device_paths);
    auto system_path = device::construct_emulated_path(device, translated_module_path, emuenv.vita_fs_path, emuenv.io.redirect_stdio);

    if (module_path.starts_with("os0:kd/")) {
        if (!fs::exists(system_path)) {
            // The boot image module's info isn't exported until its start
            Address bootimage_info_address = 0;
            {
                const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
                for (const auto &[_, module] : emuenv.kernel.loaded_modules) {
                    if (module->state != ModuleState::none && std::string_view(module->info.path) == "os0:kd/bootimage.skprx")
                        bootimage_info_address = find_module_export(emuenv.mem, *module, nid_sceKernelBootimageInfo);
                }
            }
            SceKernelBootimageInfo *bootimage_info = Ptr<SceKernelBootimageInfo>(bootimage_info_address).get(emuenv.mem);
            if (bootimage_info) {
                for (SceSize i = 0; i < bootimage_info->number; i++) {
                    const SceKernelBootimageModules &module_content = bootimage_info->list.get(emuenv.mem)[i];
                    if (module_content.path && module_content.data && module_content.size > 0) {
                        if (module_content.path.get(emuenv.mem) == module_path) {
                            // Load the module from the boot image
                            return load_module_data(module_content.data.get(emuenv.mem));
                        }
                    }
                }
            }
        }
    }

    if (emuenv.io.case_isens_find_enabled && !fs::exists(system_path)) {
        // Attempt a case-insensitive file search.
        const auto original_translated_module_path = translated_module_path;
        const auto cached_path = find_in_cache(emuenv.io, string_utils::tolower(translated_module_path.string()));
        if (!cached_path.empty()) {
            translated_module_path = cached_path;
            LOG_TRACE("Found cached filepath at {}", translated_module_path);
        } else {
            const bool path_found = find_case_isens_path(emuenv.io, device_for_icase, translated_module_path, system_path);
            translated_module_path = find_in_cache(emuenv.io, string_utils::tolower(system_path.string()));
            if (!translated_module_path.empty() && path_found) {
                LOG_TRACE("Found file on case-sensitive filesystem at {}", translated_module_path);
                translated_module_path = translated_module_path.string().substr(emuenv.vita_fs_path.string().length());
                translated_module_path = translated_module_path.string().substr(translated_module_path.string().find('/') + 1);
            } else {
                LOG_ERROR("Missing file at {} (target path: {})", original_translated_module_path.string(), module_path);
                return SCE_ERROR_ERRNO_ENOENT;
            }
        }
    }

    vfs::FileBuffer module_buffer;
    bool res;
    if (device == VitaIoDevice::app0)
        res = vfs::read_app_file(module_buffer, emuenv.vita_fs_path, emuenv.io.app_path, translated_module_path);
    else
        res = vfs::read_file(device, module_buffer, emuenv.vita_fs_path, translated_module_path);
    if (!res) {
        LOG_ERROR("Failed to read module file {}", module_path);
        return SCE_ERROR_ERRNO_ENOENT;
    }

    // Decrypt module file if necessary
    module_buffer = decrypt_fself(module_buffer, emuenv.license.rif[emuenv.io.title_id].key);
    if (module_buffer.empty()) {
        LOG_ERROR("Failed to decrypt module file {}", module_path);
        return SCE_ERROR_ERRNO_ENOENT;
    }

    return load_module_data(module_buffer.data());
}

int unload_module(EmuEnvState &emuenv, SceUID module_id) {
    const auto module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    {
        // Only a module that isn't started can go
        const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
        if (module->state != ModuleState::loaded && module->state != ModuleState::stopped)
            return SCE_KERNEL_ERROR_MODULEMGR_NOT_STOPPED;
        module->set_state(ModuleState::none);
    }
    LOG_INFO("Unloading module {} ({})", module_id, module->info.module_name);
    if (module->info.segments[0].memsz == 0) {
        LOG_WARN("Module {} has no segments, skipping unload", module->info.module_name);
        {
            const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
            emuenv.kernel.loaded_modules.erase(module->info.modid);
        }
        return 0;
    }
    return unload_self(emuenv.kernel, emuenv.mem, *module);
}

// A retail game or a homebrew built as unsafe
static bool is_game_program(uint64_t auth_id) {
    return ((auth_id >> 32) & 0xFFF00000) == 0x21000000 || auth_id == 0x2F00000000000001ULL;
}

// Runs a module's start or stop function on a thread of its own and returns what it returned
static ModuleStatus run_module_entry(EmuEnvState &emuenv, const KernelModule &module, Ptr<const void> entry, Address thread_param, SceSize args, Ptr<const void> argp) {
    const std::string name = fmt::format("{} EntryCall", module.info.module_name);
    // Only a game's own modules get the bigger stack
    const bool game_stack = is_game_program(emuenv.kernel.auth_id) && !(module.flags & SCE_KERNEL_MODULE_FLAG_PRELOAD);
    SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
    SceSize stack_size = game_stack ? SCE_KERNEL_MODULE_ENTRY_STACK_SIZE_GAME : SCE_KERNEL_MODULE_ENTRY_STACK_SIZE;
    SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
    // The module's own thread parameters replace all three, a block that can't be read is ignored
    if (thread_param && is_valid_addr_len(emuenv.mem, thread_param, sizeof(SceKernelModuleThreadParam))) {
        const SceKernelModuleThreadParam &param = *Ptr<const SceKernelModuleThreadParam>(thread_param).get(emuenv.mem);
        if (param.size == 4) {
            priority = param.priority;
            stack_size = param.stack_size;
            affinity = param.cpu_affinity_mask;
            if (!is_user_priority(priority))
                return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
            if (affinity & ~SCE_KERNEL_CPU_MASK_USER_ALL)
                return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
            if (stack_size == 0 || stack_size > SCE_KERNEL_THREAD_STACK_SIZE_MAX)
                return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE);
        }
    }
    const ThreadStateRef thread = emuenv.kernel.create_thread(emuenv.mem, name.c_str(), entry, priority, affinity, stack_size,
        SCE_KERNEL_ATTR_SUSPEND_ENDS_WAITS);
    if (!thread)
        return std::unexpected(SCE_KERNEL_ERROR_ERROR);
    const ModuleStatus status = thread->start_and_wait(args, argp.cast<void>());
    thread->remove();
    return status;
}

// Sets a module's state under the kernel lock
static void set_module_state(EmuEnvState &emuenv, KernelModule &module, ModuleState state) {
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    module.set_state(state);
}

ModuleStatus start_module(EmuEnvState &emuenv, KernelModule &module, SceSize args, Ptr<const void> argp) {
    {
        const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
        switch (module.state) {
        case ModuleState::loaded:
            break;
        case ModuleState::none:
            return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
        case ModuleState::stopped:
            return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_STOPPED);
        default:
            return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_STARTED);
        }
        module.set_state(ModuleState::starting);
    }
    // The libraries registered before one that fails stay registered
    if (const int error = link_module(emuenv.kernel, emuenv.mem, module); error < 0) {
        LOG_ERROR("Module {} can't link: {}", module.info.module_name, log_hex(error));
        set_module_state(emuenv, module, ModuleState::loaded);
        return std::unexpected(error);
    }
    ModuleStatus status = SCE_KERNEL_START_SUCCESS;
    if (module.info.start_entry && !(module.flags & SCE_KERNEL_MODULE_FLAG_MAIN)) {
        LOG_DEBUG("Running module_start of library: {} at address {}", module.info.module_name, log_hex(module.info.start_entry.address()));
        status = run_module_entry(emuenv, module, module.info.start_entry, module.start_thread_param, args, argp);
        if (!status) {
            LOG_ERROR("Module {} can't run its module_start: {}", module.info.module_name, log_hex(status.error()));
            unlink_module(emuenv.kernel, emuenv.mem, module, true);
            set_module_state(emuenv, module, ModuleState::loaded);
            return status;
        }
        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module.info.module_name, module.info.path, log_hex(*status));
    }
    // A failed start leaves the module loaded, one with nothing to keep resident is unloaded. Both
    // take its libraries even with strong imports linked to them.
    if (*status == SCE_KERNEL_START_FAILED) {
        LOG_ERROR("Module {} did not start successfully!", module.info.module_name);
        unlink_module(emuenv.kernel, emuenv.mem, module, true);
        set_module_state(emuenv, module, ModuleState::loaded);
        return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_START_FAILED);
    }
    if (*status == SCE_KERNEL_START_NO_RESIDENT) {
        unlink_module(emuenv.kernel, emuenv.mem, module, true);
        set_module_state(emuenv, module, ModuleState::stopped);
        unload_module(emuenv, module.info.modid);
    } else
        set_module_state(emuenv, module, ModuleState::resident);
    return status;
}

ModuleStatus stop_module(EmuEnvState &emuenv, KernelModule &module, SceSize args, Ptr<const void> argp) {
    {
        const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
        switch (module.state) {
        case ModuleState::resident:
            break;
        case ModuleState::stopped:
            return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_STOPPED);
        default:
            return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED);
        }
        module.set_state(ModuleState::stopping);
    }
    ModuleStatus status = SCE_KERNEL_STOP_SUCCESS;
    if (module.info.stop_entry) {
        LOG_DEBUG("Running module_stop of library: {} at address {}", module.info.module_name, log_hex(module.info.stop_entry.address()));
        status = run_module_entry(emuenv, module, module.info.stop_entry, module.stop_thread_param, args, argp);
        if (!status) {
            LOG_ERROR("Module {} can't run its module_stop: {}", module.info.module_name, log_hex(status.error()));
            set_module_state(emuenv, module, ModuleState::resident);
            return status;
        }
        LOG_INFO("Module {} (at \"{}\") module_stop returned {}", module.info.module_name, module.info.path, log_hex(*status));
    }
    // A failed stop keeps the module started
    if (*status == SCE_KERNEL_STOP_FAIL) {
        LOG_ERROR("Module {} did not stop successfully!", module.info.module_name);
        set_module_state(emuenv, module, ModuleState::resident);
        return std::unexpected(SCE_KERNEL_ERROR_MODULEMGR_STOP_FAIL);
    }
    // A library a strong import links to keeps the module started, the libraries before it stay
    // unregistered
    if (const int error = unlink_module(emuenv.kernel, emuenv.mem, module, false); error < 0) {
        LOG_ERROR("Module {} can't unlink: {}", module.info.module_name, log_hex(error));
        set_module_state(emuenv, module, ModuleState::resident);
        return std::unexpected(error);
    }
    set_module_state(emuenv, module, ModuleState::stopped);
    return status;
}

// Modules sysmodule skips, those for development mode or dipsw settings. Vita3K is a retail Vita
// with no dipsw set, and takes every game as built with 1.80 or later, which skips the
// np_message_padding module.
static bool is_skipped(uint8_t index) {
    constexpr uint8_t NP_MESSAGE_PADDING = 38;
    const SysmoduleModule &module = sysmodule_modules[index];
    if (index == NP_MESSAGE_PADDING || (module.flags == SYSMODULE_MODULE_NONE && !module.name))
        return true;
    return module.flags & (SYSMODULE_MODULE_DEVELOPMENT | SYSMODULE_MODULE_DIPSW_D4 | SYSMODULE_MODULE_DIPSW_C2);
}

// Loads and starts the modules of an id that aren't loaded yet. A failure keeps the modules loaded
// so far and a later load goes on from the module that failed. result gets the start status of the
// last module started.
static SceInt32 load_sysmodule_id(EmuEnvState &emuenv, const SysmoduleId &id, SysmoduleSlot &slot, SceSize args, Ptr<const void> argp, int *result) {
    if (slot.count) {
        slot.count++;
        return SCE_KERNEL_OK;
    }
    if (!(id.flags & SYSMODULE_ID_ARGS)) {
        args = 0;
        argp = Ptr<const void>{};
    }
    while (slot.progress < id.modules.size()) {
        const uint8_t index = id.modules[id.modules.size() - slot.progress - 1];
        SysmoduleRecord &record = emuenv.kernel.sysmodules.records[index];
        if (record.count) {
            record.count++;
        } else if (is_skipped(index)) {
            record = { 1, -1 };
        } else {
            const SysmoduleModule &module = sysmodule_modules[index];
            const char *const prefix = (module.flags & SYSMODULE_MODULE_APP0) ? "app0:sce_module/" : "vs0:sys/external/";
            const SceUID uid = load_module(emuenv, fmt::format("{}{}.suprx", prefix, module.name), SCE_KERNEL_MODULE_FLAG_PRELOAD);
            if (uid < 0)
                return uid;
            const auto loaded = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
            if (!loaded)
                return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
            // A module whose start fails stays loaded and isn't recorded
            const ModuleStatus status = start_module(emuenv, *loaded, args, argp);
            if (!status)
                return status.error();
            if (result)
                *result = static_cast<int>(*status);
            record = { 1, uid };
        }
        slot.progress++;
    }
    slot.count = 1;
    return SCE_KERNEL_OK;
}

// Stops and unloads the modules of an id that no other loaded id uses, newest first. A stop that
// fails keeps that module and the ones before it, and a later unload goes on from it.
static SceInt32 unload_sysmodule_id(EmuEnvState &emuenv, const SysmoduleId &id, SysmoduleSlot &slot) {
    if (slot.count) {
        if (--slot.count)
            return SCE_KERNEL_OK;
    } else if (!slot.progress)
        return SCE_SYSMODULE_ERROR_UNLOADED;
    while (slot.progress) {
        const uint8_t index = id.modules[id.modules.size() - slot.progress];
        SysmoduleRecord &record = emuenv.kernel.sysmodules.records[index];
        if (!record.count) {
            slot.count = 1;
            return SCE_SYSMODULE_ERROR_UNLOADED;
        }
        if (--record.count == 0 && record.uid != -1) {
            SceInt32 error = SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
            if (const auto module = lock_and_find(record.uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex)) {
                const ModuleStatus status = stop_module(emuenv, *module);
                error = status ? unload_module(emuenv, record.uid) : status.error();
            }
            if (error < 0) {
                record.count++;
                slot.count++;
                return error;
            }
        }
        slot.progress--;
    }
    return SCE_KERNEL_OK;
}

static bool is_sysmodule_id_loaded(const SysmoduleId &id, const SysmoduleSlot &slot) {
    return slot.count && slot.progress == id.modules.size();
}

SceInt32 load_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    return load_sysmodule_id(emuenv, sysmodule_ids[module_id], sysmodules.ids[module_id], 0, Ptr<const void>{}, nullptr);
}

SceInt32 unload_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    return unload_sysmodule_id(emuenv, sysmodule_ids[module_id], sysmodules.ids[module_id]);
}

bool is_sys_module_loaded(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    return is_sysmodule_id_loaded(sysmodule_ids[module_id], sysmodules.ids[module_id]);
}

SceInt32 load_sys_module_internal_with_arg(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, int *retcode) {
    const uint16_t index = module_id & 0xFFFF;
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    if (retcode)
        *retcode = 0;
    return load_sysmodule_id(emuenv, sysmodule_internal_ids[index], sysmodules.internal_ids[index], args, argp, retcode);
}

SceInt32 unload_sys_module_internal(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id) {
    const uint16_t index = module_id & 0xFFFF;
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    return unload_sysmodule_id(emuenv, sysmodule_internal_ids[index], sysmodules.internal_ids[index]);
}

bool is_sys_module_internal_loaded(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id) {
    const uint16_t index = module_id & 0xFFFF;
    Sysmodules &sysmodules = emuenv.kernel.sysmodules;
    const std::lock_guard<std::mutex> lock(sysmodules.mutex);
    return is_sysmodule_id_loaded(sysmodule_internal_ids[index], sysmodules.internal_ids[index]);
}

void init_libraries(EmuEnvState &emuenv) {
#define LIBRARY(name) import_library_init_##name(emuenv);
#include <modules/library_init_list.inc>
#undef LIBRARY
}
