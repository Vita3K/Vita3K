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

#include <modules/module_parent.h>

#include <cpu/functions.h>
#include <host/import_fn.h>
#include <host/load_self.h>
#include <host/state.h>
#include <io/device.h>
#include <io/vfs.h>
#include <module/load_module.h>
#include <nids/functions.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <unordered_set>

static constexpr bool LOG_UNK_NIDS_ALWAYS = false;

#define NID(name, nid) extern const ImportFn import_##name;
#include <nids/nids.h>
#undef NID

static ImportFn resolve_import(uint32_t nid) {
    switch (nid) {
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.h>
#undef NID
    }

    return ImportFn();
}

/**
 * \brief Resolves a function imported from a loaded module.
 * \param kernel Kernel state struct
 * \param nid NID to resolve
 * \return Resolved address, 0 if not found
 */
Address resolve_export(KernelState &kernel, uint32_t nid) {
    const ExportNids::iterator export_address = kernel.export_nids.find(nid);
    if (export_address == kernel.export_nids.end()) {
        return 0;
    }

    return export_address->second;
}

uint32_t resolve_nid(KernelState &kernel, Address addr) {
    auto nid = kernel.nid_from_export.find(addr);
    if (nid == kernel.nid_from_export.end()) {
        // resolve the thumbs address
        addr = addr | 1;
        nid = kernel.nid_from_export.find(addr);
        if (nid == kernel.nid_from_export.end()) {
            return 0;
        }
    }

    return nid->second;
}

std::string resolve_nid_name(KernelState &kernel, Address addr) {
    auto nid = resolve_nid(kernel, addr);
    if (nid == 0) {
        return "";
    }
    return import_name(nid);
}

static void log_import_call(char emulation_level, uint32_t nid, SceUID thread_id, const std::unordered_set<uint32_t> &nid_blacklist, Address pc) {
    if (nid_blacklist.find(nid) == nid_blacklist.end()) {
        const char *const name = import_name(nid);
        LOG_TRACE("[{}LE] TID: {:<3} FUNC: {} {} at {}", emulation_level, thread_id, log_hex(nid), name, log_hex(pc));
    }
}

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function

        if (host.kernel.watch_import_calls) {
            const std::unordered_set<uint32_t> hle_nid_blacklist = {
                0xB295EB61, // sceKernelGetTLSAddr
                0x46E7BE7B, // sceKernelLockLwMutex
                0x91FA6614, // sceKernelUnlockLwMutex
            };
            auto pc = read_pc(cpu);
            log_import_call('H', nid, thread_id, hle_nid_blacklist, pc);
        }
        const ImportFn fn = resolve_import(nid);
        if (fn) {
            fn(host, cpu, thread_id);
        } else if (host.missing_nids.count(nid) == 0 || LOG_UNK_NIDS_ALWAYS) {
            const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
            LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);

            if (!LOG_UNK_NIDS_ALWAYS)
                host.missing_nids.insert(nid);
        }
    } else {
        // LLE - directly run ARM code imported from some loaded module

        if (host.kernel.watch_import_calls) {
            const std::unordered_set<uint32_t> lle_nid_blacklist = {};
            auto pc = read_pc(cpu);
            log_import_call('L', nid, thread_id, lle_nid_blacklist, pc);
        }
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}

/**
 * \return False on failure, true on success
 */
bool load_module(HostState &host, SceSysmoduleModuleId module_id) {
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, cpu, nid, main_thread_id);
    };
    const ResolveNIDName resolve_nid_name = [&host](Address addr) {
        return ::resolve_nid_name(host.kernel, addr);
    };

    LOG_INFO("Loading module ID: {}", log_hex(module_id));

    const auto module_paths = sysmodule_paths[module_id];

    for (std::string module_path : module_paths) {
        module_path = "sys/external/" + module_path + ".suprx";

        vfs::FileBuffer module_buffer;
        Ptr<const void> lib_entry_point;

        if (vfs::read_file(VitaIoDevice::vs0, module_buffer, host.pref_path, module_path)) {
            SceUID loaded_module_uid = load_self(lib_entry_point, host.kernel, host.mem, module_buffer.data(), module_path, host.cfg);
            const auto module = host.kernel.loaded_modules[loaded_module_uid];
            const auto module_name = module->module_name;

            if (loaded_module_uid >= 0) {
                LOG_INFO("Module {} (at \"{}\") loaded", module_name, module_path);
            } else {
                LOG_ERROR("Error when loading module at \"{}\"", module_path);
                return false;
            }

            if (lib_entry_point) {
                LOG_DEBUG("Running module_start of module: {}", module_name);

                Ptr<void> argp = Ptr<void>();
                const SceUID module_thread_id = create_thread(lib_entry_point, host.kernel, host.mem, module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER,
                    static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_DEFAULT), call_import, resolve_nid_name, nullptr);
                const ThreadStatePtr module_thread = util::find(module_thread_id, host.kernel.threads);
                const auto ret = run_on_current(*module_thread, lib_entry_point, 0, argp);

                module_thread->to_do = ThreadToDo::exit;
                module_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?

                const std::lock_guard<std::mutex> lock(host.kernel.mutex);
                host.kernel.running_threads.erase(module_thread_id);
                host.kernel.threads.erase(module_thread_id);
                LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(ret));
            }

        } else {
            LOG_ERROR("Module at \"{}\" not present", module_path);
            // ignore and assume it was loaded
        }
    }

    host.kernel.loaded_sysmodules.push_back(module_id);
    return true;
}
