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
#include <host/import_var.h>
#include <host/load_self.h>
#include <host/state.h>
#include <io/device.h>
#include <io/vfs.h>
#include <kernel/functions.h>
#include <module/load_module.h>
#include <nids/functions.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <unordered_set>

static constexpr bool LOG_UNK_NIDS_ALWAYS = false;

#define VAR_NID(name, nid) extern const ImportVarFactory import_##name;
#define NID(name, nid) extern const ImportFn import_##name;
#include <nids/nids.h>
#undef NID
#undef VAR_NID

struct HostState;

static ImportFn resolve_import(uint32_t nid) {
    switch (nid) {
#define VAR_NID(name, nid)
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.h>
#undef NID
#undef VAR_NID
    }

    return ImportFn();
}

const std::array<VarExport, var_exports_size> &get_var_exports() {
    static std::array<VarExport, var_exports_size> var_exports = {
#define NID(name, nid)
#define VAR_NID(name, nid) \
    {                      \
        nid,               \
        import_##name,     \
        #name              \
    },
#include <nids/nids.h>
#undef VAR_NID
#undef NID
    };
    return var_exports;
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

static void log_import_call(char emulation_level, uint32_t nid, SceUID thread_id, const std::unordered_set<uint32_t> &nid_blacklist, Address lr) {
    if (nid_blacklist.find(nid) == nid_blacklist.end()) {
        const char *const name = import_name(nid);
        LOG_TRACE("[{}LE] TID: {:<3} FUNC: {} {} at {}", emulation_level, thread_id, log_hex(nid), name, log_hex(lr));
    }
}

// Encode code taken from https://github.com/yifanlu/UVLoader/blob/master/resolve.c

#define INSTRUCTION_UNKNOWN 0 ///< Unknown/unsupported instruction
#define INSTRUCTION_MOVW 1 ///< MOVW Rd, \#imm instruction
#define INSTRUCTION_MOVT 2 ///< MOVT Rd, \#imm instruction
#define INSTRUCTION_SYSCALL 3 ///< SVC \#imm instruction
#define INSTRUCTION_BRANCH 4 ///< BX Rn instruction

static uint32_t encode_arm_inst(uint8_t type, uint16_t immed, uint16_t reg) {
    switch (type) {
    case INSTRUCTION_MOVW:
        // 1110 0011 0000 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE30
        return ((uint32_t)0xE30 << 20) | ((uint32_t)(immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_MOVT:
        // 1110 0011 0100 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE34
        return ((uint32_t)0xE34 << 20) | ((uint32_t)(immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_SYSCALL:
        // Syscall does not have any immediate value, the number should
        // already be in R12
        return (uint32_t)0xEF000000;
    case INSTRUCTION_BRANCH:
        // 1110 0001 0010 111111111111 0001 YYYY
        // BX Rn has 0xE12FFF1 as top bytes
        return ((uint32_t)0xE12FFF1 << 4) | reg;
    case INSTRUCTION_UNKNOWN:
    default:
        return 0;
    }
}

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function
        if (is_returning(cpu))
            return;
        if (host.kernel.watch_import_calls) {
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
            fn(host, cpu, thread_id);
        } else if (host.missing_nids.count(nid) == 0 || LOG_UNK_NIDS_ALWAYS) {
            const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
            LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);

            if (!LOG_UNK_NIDS_ALWAYS)
                host.missing_nids.insert(nid);
        }
    } else {
        auto pc = read_pc(cpu);

        uint32_t *const stub = Ptr<uint32_t>(Address(pc)).get(host.mem);

        stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)export_pc, 12);
        stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(export_pc >> 16), 12);
        stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);

        // LLE - directly run ARM code imported from some loaded module
        if (is_returning(cpu)) {
            LOG_TRACE("[LLE] TID: {:<3} FUNC: {} returned {}", thread_id, import_name(nid), log_hex(read_reg(cpu, 0)));
            return;
        }

        const std::unordered_set<uint32_t> lle_nid_blacklist = {};
        log_import_call('L', nid, thread_id, lle_nid_blacklist, pc);
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}

/**
 * \return False on failure, true on success
 */
bool load_module(HostState &host, SceSysmoduleModuleId module_id) {
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
                auto inject = create_cpu_dep_inject(host);
                const SceUID module_thread_id = create_thread(lib_entry_point, host.kernel, host.mem, module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER,
                    static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_DEFAULT), inject, nullptr);
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

CPUDepInject create_cpu_dep_inject(HostState &host) {
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, cpu, nid, main_thread_id);
    };
    const ResolveNIDName resolve_nid_name = [&host](Address addr) {
        return ::resolve_nid_name(host.kernel, addr);
    };
    auto get_watch_memory_addr = [&host](Address addr) {
        return ::get_watch_memory_addr(host.kernel, addr);
    };

    CPUDepInject inject;
    inject.call_import = call_import;
    inject.resolve_nid_name = resolve_nid_name;
    inject.trace_stack = host.cfg.stack_traceback;
    inject.get_watch_memory_addr = get_watch_memory_addr;
    inject.module_regions = host.kernel.module_regions;
    return inject;
}
