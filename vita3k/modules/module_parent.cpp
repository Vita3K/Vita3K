// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <cpu/functions.h>
#include <emuenv/state.h>
#include <io/device.h>
#include <io/state.h>
#include <io/vfs.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <module/load_module.h>
#include <nids/functions.h>
#include <util/arm.h>
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

static ImportFn resolve_import(uint32_t nid) {
    switch (nid) {
#define VAR_NID(name, nid)
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.inc>
#undef NID
#undef VAR_NID
    }

    return ImportFn();
}

const std::array<VarExport, var_exports_size> &get_var_exports() {
    static std::array<VarExport, var_exports_size> var_exports = { {
#define NID(name, nid)
#define VAR_NID(name, nid) \
    {                      \
        nid,               \
        import_##name,     \
        #name              \
    },
#include <nids/nids.inc>
#undef VAR_NID
#undef NID
    } };
    return var_exports;
}

/**
 * \brief Resolves a function imported from a loaded module.
 * \param kernel Kernel state struct
 * \param nid NID to resolve
 * \return Resolved address, 0 if not found
 */
Address resolve_export(KernelState &kernel, uint32_t nid) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const ExportNids::iterator export_address = kernel.export_nids.find(nid);
    if (export_address == kernel.export_nids.end()) {
        return 0;
    }

    return export_address->second;
}

static void log_import_call(char emulation_level, uint32_t nid, SceUID thread_id, const std::unordered_set<uint32_t> &nid_blacklist, Address lr) {
    if (!nid_blacklist.contains(nid)) {
        const char *const name = import_name(nid);
        LOG_TRACE("[{}LE] TID: {:<3} FUNC: {} {} at {}", emulation_level, thread_id, log_hex(nid), name, log_hex(lr));
    }
}

void call_import(EmuEnvState &emuenv, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(emuenv.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function
        if (emuenv.kernel.debugger.watch_import_calls) {
            const std::unordered_set<uint32_t> hle_nid_blacklist = {
                0xB295EB61, // sceKernelGetTLSAddr
                0x46E7BE7B, // sceKernelLockLwMutex
                0x91FA6614, // sceKernelUnlockLwMutex
            };
            auto lr = read_lr(cpu);
            log_import_call('H', nid, thread_id, hle_nid_blacklist, lr);
        }
        const ImportFn fn = resolve_import(nid);
        if (fn) {
            fn(emuenv, cpu, thread_id);
        } else {
            const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
            // make the function return 0
            write_reg(*thread->cpu, 0, 0);

            if (!emuenv.missing_nids.contains(nid) || LOG_UNK_NIDS_ALWAYS) {
                LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);

                if (!LOG_UNK_NIDS_ALWAYS)
                    emuenv.missing_nids.insert(nid);
            }
        }
    } else {
        // Note: the following code is absolutely not thread safe, invalidating the memory
        // on other processes won't change anything about it
        // If two threads recompile the nid call instruction at the same time, the second one
        // will possibly read a random nid
        // A way to mitigate that would be save the nid in the recompiled code instead of reading
        // it when a svc is found

        auto pc = read_pc(cpu);

        assert((pc & 1) == 0);

        pc -= 4; // Move back to SVC (SuperVisor Call) instruction

        uint32_t *const stub = Ptr<uint32_t>(pc).get(emuenv.mem);

        stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)export_pc, 12);
        stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(export_pc >> 16), 12);
        stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);

        // LLE - directly run ARM code imported from some loaded module
        // TODO: resurrect this
        /*if (is_returning(cpu)) {
            LOG_TRACE("[LLE] TID: {:<3} FUNC: {} returned {}", thread_id, import_name(nid), log_hex(read_reg(cpu, 0)));
            return;
        }*/

        const std::unordered_set<uint32_t> lle_nid_blacklist = {};
        log_import_call('L', nid, thread_id, lle_nid_blacklist, pc);
        write_pc(cpu, export_pc);
        // invalidate this small region (without it, this code will be called again)
        invalidate_jit_cache(cpu, pc, 3 * sizeof(uint32_t));
    }
}

SceUID load_module(EmuEnvState &emuenv, const std::string &module_path) {
    // Check if module is already loaded
    {
        const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
        const auto &loaded_modules = emuenv.kernel.loaded_modules;
        auto module_iter = std::find_if(loaded_modules.begin(), loaded_modules.end(), [&](const auto &p) {
            return module_path == p.second->info.path;
        });

        if (module_iter != loaded_modules.end()) {
            return module_iter->first;
        }
    }

    LOG_INFO("Loading module \"{}\"", module_path);
    vfs::FileBuffer module_buffer;
    bool res;
    VitaIoDevice device = device::get_device(module_path);
    auto translated_module_path = translate_path(module_path.c_str(), device, emuenv.io.device_paths);
    if (device == VitaIoDevice::app0)
        res = vfs::read_app_file(module_buffer, emuenv.pref_path, emuenv.io.app_path, translated_module_path);
    else
        res = vfs::read_file(device, module_buffer, emuenv.pref_path, translated_module_path);
    if (!res) {
        LOG_ERROR("Failed to read module file {}", module_path);
        return SCE_ERROR_ERRNO_ENOENT;
    }
    SceUID module_id = load_self(emuenv.kernel, emuenv.mem, module_buffer.data(), module_path, emuenv.log_path);
    if (module_id >= 0) {
        const auto module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
        LOG_INFO("Module {} (at \"{}\") loaded", module->info.module_name, module_path);
    } else {
        LOG_ERROR("Failed to load module {}", module_path);
    }
    return module_id;
}

int unload_module(EmuEnvState &emuenv, SceUID module_id) {
    const auto module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    LOG_INFO("Unloading module {} ({})", module_id, module->info.module_name);

    return unload_self(emuenv.kernel, emuenv.mem, *module);
}

uint32_t start_module(EmuEnvState &emuenv, const SceKernelModuleInfo &module, SceSize args, Ptr<const void> argp) {
    const auto module_start = module.start_entry;
    if (module_start) {
        const auto module_name = module.module_name;

        LOG_DEBUG("Running module_start of library: {} at address {}", module_name, log_hex(module_start.address()));
        SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
        SceInt32 stack_size = SCE_KERNEL_STACK_SIZE_USER_MAIN;
        SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
        // module_start is always called from new thread
        const ThreadStatePtr module_thread = emuenv.kernel.create_thread(emuenv.mem, module_name, module_start, priority, affinity, stack_size, nullptr);

        const uint32_t ret = module_thread->run_guest_function(module_start.address(), args, argp.cast<void>());
        module_thread->exit_delete(false);

        LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module.path, log_hex(ret));
        if (ret != SCE_KERNEL_START_SUCCESS)
            LOG_ERROR("Module {} did not return successfully!", module_name);

        return ret;
    }
    return 0;
}

uint32_t stop_module(EmuEnvState &emuenv, const SceKernelModuleInfo &module, SceSize args, Ptr<const void> argp) {
    const auto module_stop = module.stop_entry;
    if (module_stop) {
        const auto module_name = module.module_name;

        LOG_DEBUG("Running module_stop of library: {} at address {}", module_name, log_hex(module_stop.address()));
        SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
        SceInt32 stack_size = SCE_KERNEL_STACK_SIZE_USER_MAIN;
        SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
        // module_stop is always called from new thread
        const ThreadStatePtr module_thread = emuenv.kernel.create_thread(emuenv.mem, module_name, module_stop, priority, affinity, stack_size, nullptr);

        const uint32_t ret = module_thread->run_guest_function(module_stop.address(), args, argp.cast<void>());
        module_thread->exit_delete(false);

        LOG_INFO("Module {} (at \"{}\") module_stop returned {}", module_name, module.path, log_hex(ret));
        if (ret != SCE_KERNEL_STOP_SUCCESS)
            LOG_ERROR("Module {} did not stop successfully!", module_name);
        return ret;
    }
    return 0;
}

/**
 * \return False on failure, true on success
 */
bool load_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    const auto &module_paths = sysmodule_paths[module_id];
    std::vector<SceUID> loaded_uids;
    for (const auto module_filename : module_paths) {
        std::string module_path;
        if (module_id == SCE_SYSMODULE_SMART || module_id == SCE_SYSMODULE_FACE || module_id == SCE_SYSMODULE_ULT) {
            module_path = fmt::format("app0:sce_module/{}.suprx", module_filename);
        } else {
            module_path = fmt::format("vs0:sys/external/{}.suprx", module_filename);
        }

        auto loaded_module_uid = load_module(emuenv, module_path);

        if (loaded_module_uid < 0) {
            if (module_id == SCE_SYSMODULE_ULT && loaded_module_uid == SCE_ERROR_ERRNO_ENOENT) {
                module_path = fmt::format("vs0:sys/external/{}.suprx", module_filename);
                loaded_module_uid = load_module(emuenv, module_path);
                if (loaded_module_uid < 0)
                    return false;
            } else
                return false;
        }
        loaded_uids.push_back(loaded_module_uid);
        const auto module = lock_and_find(loaded_module_uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
        start_module(emuenv, module->info);
    }

    std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
    emuenv.kernel.loaded_sysmodules[module_id] = std::move(loaded_uids);
    return true;
}

int unload_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id) {
    const char *export_name = __FUNCTION__;

    std::vector<SceUID> loaded_uids;
    {
        std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
        auto it = emuenv.kernel.loaded_sysmodules.find(module_id);
        if (it == emuenv.kernel.loaded_sysmodules.end())
            return RET_ERROR(SCE_SYSMODULE_ERROR_UNLOADED);
        loaded_uids = std::move(it->second);
        emuenv.kernel.loaded_sysmodules.erase(it);
    }

    // unload the modules in the reverse order they were loaded
    std::reverse(loaded_uids.begin(), loaded_uids.end());

    // first stop everything, then unload
    for (SceUID uid : loaded_uids) {
        const auto module = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
        if (!module)
            return RET_ERROR(SCE_SYSMODULE_ERROR_FATAL);

        stop_module(emuenv, module->info);
    }
    for (SceUID uid : loaded_uids) {
        int ret = unload_module(emuenv, uid);
        if (ret < 0)
            return RET_ERROR(SCE_SYSMODULE_ERROR_FATAL);
    }

    return 0;
}

bool load_sys_module_internal_with_arg(EmuEnvState &emuenv, SceUID thread_id, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, int *retcode) {
    LOG_INFO("Loading internal module ID: {}", log_hex(module_id));

    if (!sysmodule_internal_paths.contains(module_id))
        return false;

    const auto &module_paths = sysmodule_internal_paths.at(module_id);

    for (auto module_filename : module_paths) {
        std::string module_path = fmt::format("vs0:sys/external/{}.suprx", module_filename);
        auto loaded_module_uid = load_module(emuenv, module_path);

        if (loaded_module_uid < 0) {
            return false;
        }
        const auto module = lock_and_find(loaded_module_uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
        auto ret = start_module(emuenv, module->info, args, argp);
        if (retcode)
            *retcode = static_cast<int>(ret);
    }
    emuenv.kernel.loaded_internal_sysmodules.push_back(module_id);
    return true;
}

void init_libraries(EmuEnvState &emuenv) {
#define LIBRARY(name) import_library_init_##name(emuenv);
#include <modules/library_init_list.inc>
#undef LIBRARY
}
