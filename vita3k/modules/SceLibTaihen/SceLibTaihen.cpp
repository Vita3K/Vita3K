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

#include <module/module.h>

#include <io/device.h>
#include <io/vfs.h>
#include <kernel/state.h>
#include <mem/functions.h>
#include <modules/module_parent.h>
#include <util/log.h>

#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// ==================== taiHEN State ====================

struct TaiHook {
    SceUID uid;
    SceUID pid;
    Address target_addr; // address of the patched stub/code
    uint32_t saved_inst[3]; // original instructions
    Address hook_func; // ARM address of the hook function
    Address trampoline; // ARM address of trampoline to call original
    TaiHook *next; // next hook in chain
};

struct TaiInjection {
    SceUID uid;
    SceUID pid;
    Address dest;
    std::vector<uint8_t> saved_data;
};

struct TaihenState {
    std::mutex mutex;
    SceUID next_uid = 0x70000; // UIDs for taiHEN objects
    std::map<SceUID, TaiHook *> hooks;
    std::map<SceUID, TaiInjection> injections;
    std::map<std::string, std::vector<std::string>> config; // section → plugin paths
    bool config_loaded = false;
    bool kernel_plugins_loaded = false;

    SceUID alloc_uid() {
        return next_uid++;
    }
};

// ==================== Config Parser ====================

static std::map<std::string, std::vector<std::string>> parse_taihen_config(const std::string &content) {
    std::map<std::string, std::vector<std::string>> config;
    std::string current_section;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // trim trailing \r for Windows-style line endings
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);

        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '*') {
            current_section = line.substr(1);
            continue;
        }

        if (!current_section.empty()) {
            config[current_section].push_back(line);
        }
    }

    return config;
}

// ==================== taihen library (user exports) ====================

EXPORT(SceUID, taiHookFunctionExportForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    STUBBED("taiHookFunctionExportForUser");
    return 0;
}

EXPORT(SceUID, taiHookFunctionImportForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    STUBBED("taiHookFunctionImportForUser");
    return 0;
}

EXPORT(SceUID, taiHookFunctionOffsetForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    STUBBED("taiHookFunctionOffsetForUser");
    return 0;
}

EXPORT(int, taiGetModuleInfo, const char *module, Ptr<void> info) {
    if (!module || !info) {
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    }

    const auto &loaded_modules = emuenv.kernel.loaded_modules;
    for (const auto &[uid, mod] : loaded_modules) {
        if (std::string(mod->info.module_name) == module) {
            // tai_module_info_t layout:
            // uint32_t size (offset 0)
            // SceUID modid (offset 4)
            // uint32_t module_nid (offset 8)
            // char name[27] (offset 12)
            // ... exports_start, exports_end, imports_start, imports_end
            auto info_ptr = info.get(emuenv.mem);
            uint32_t user_size = *reinterpret_cast<uint32_t *>(info_ptr);
            if (user_size < 0x34) {
                return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
            }

            auto out = reinterpret_cast<uint8_t *>(info_ptr);
            *reinterpret_cast<uint32_t *>(out + 4) = uid; // modid
            *reinterpret_cast<uint32_t *>(out + 8) = 0; // module_nid (not tracked in Vita3K)
            std::strncpy(reinterpret_cast<char *>(out + 12), mod->info.module_name, 27);
            // segments info
            *reinterpret_cast<uint32_t *>(out + 0x28) = mod->info.segments[0].vaddr.address(); // exports_start (approx)
            *reinterpret_cast<uint32_t *>(out + 0x2C) = mod->info.segments[0].vaddr.address() + mod->info.segments[0].memsz; // exports_end (approx)

            LOG_DEBUG("taiGetModuleInfo: found module '{}' uid={}", module, uid);
            return 0;
        }
    }

    LOG_WARN("taiGetModuleInfo: module '{}' not found", module);
    return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
}

EXPORT(int, taiHookRelease, SceUID tai_uid, uint32_t hook_ref) {
    STUBBED("taiHookRelease");
    return 0;
}

EXPORT(SceUID, taiInjectAbs, Ptr<void> dest, Ptr<const void> src, uint32_t size) {
    if (!dest || !src || size == 0) {
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    }

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    const std::lock_guard<std::mutex> lock(state->mutex);

    TaiInjection inject;
    inject.uid = state->alloc_uid();
    inject.pid = thread_id; // approximate
    inject.dest = dest.address();

    auto dest_ptr = dest.get(emuenv.mem);
    auto src_ptr = src.get(emuenv.mem);

    // save original data
    inject.saved_data.resize(size);
    std::memcpy(inject.saved_data.data(), dest_ptr, size);

    // write injection
    std::memcpy(dest_ptr, src_ptr, size);

    // invalidate JIT cache
    emuenv.kernel.invalidate_jit_cache(dest.address(), size);

    SceUID uid = inject.uid;
    state->injections[uid] = std::move(inject);

    LOG_DEBUG("taiInjectAbs: injected {} bytes at {}, uid={}", size, log_hex(dest.address()), uid);
    return uid;
}

EXPORT(SceUID, taiInjectDataForUser, Ptr<void> args) {
    STUBBED("taiInjectDataForUser");
    return 0;
}

EXPORT(int, taiInjectRelease, SceUID tai_uid) {
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    const std::lock_guard<std::mutex> lock(state->mutex);

    auto it = state->injections.find(tai_uid);
    if (it == state->injections.end()) {
        return SCE_KERNEL_ERROR_INVALID_UID;
    }

    auto &inject = it->second;
    auto dest_ptr = Ptr<void>(inject.dest).get(emuenv.mem);
    std::memcpy(dest_ptr, inject.saved_data.data(), inject.saved_data.size());
    emuenv.kernel.invalidate_jit_cache(inject.dest, static_cast<size_t>(inject.saved_data.size()));

    state->injections.erase(it);

    LOG_DEBUG("taiInjectRelease: released injection uid={}", tai_uid);
    return 0;
}

EXPORT(int, taiGetModuleExportFunc, const char *modname, uint32_t libnid, uint32_t funcnid, Ptr<uint32_t> func) {
    STUBBED("taiGetModuleExportFunc");
    return 0;
}

// ==================== taihenUnsafe library ====================

EXPORT(SceUID, taiLoadKernelModule, const char *path, int flags, Ptr<void> opt) {
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadKernelModule: path={}", path);
    return load_module(emuenv, path);
}

EXPORT(int, taiStartKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    STUBBED("taiStartKernelModuleForUser");
    return 0;
}

EXPORT(SceUID, taiLoadStartKernelModuleForUser, const char *path, Ptr<void> args) {
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadStartKernelModuleForUser: path={}", path);
    SceUID uid = load_module(emuenv, path);
    if (uid < 0)
        return uid;

    const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
    if (mod_it != emuenv.kernel.loaded_modules.end()) {
        start_module(emuenv, mod_it->second->info);
    }
    return uid;
}

EXPORT(SceUID, taiLoadStartModuleForPidForUser, const char *path, Ptr<void> args) {
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadStartModuleForPidForUser: path={}", path);
    SceUID uid = load_module(emuenv, path);
    if (uid < 0)
        return uid;

    const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
    if (mod_it != emuenv.kernel.loaded_modules.end()) {
        start_module(emuenv, mod_it->second->info);
    }
    return uid;
}

EXPORT(int, taiStopKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    STUBBED("taiStopKernelModuleForUser");
    return 0;
}

EXPORT(int, taiUnloadKernelModule, SceUID modid, int flags, Ptr<void> opt) {
    STUBBED("taiUnloadKernelModule");
    return 0;
}

EXPORT(int, taiStopUnloadKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    STUBBED("taiStopUnloadKernelModuleForUser");
    return 0;
}

EXPORT(int, taiMemcpyUserToKernel, Ptr<void> kernel_dst, Ptr<const void> user_src, uint32_t len) {
    if (!kernel_dst || !user_src)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    // In emulator, user/kernel address space is the same
    std::memcpy(kernel_dst.get(emuenv.mem), user_src.get(emuenv.mem), len);
    return 0;
}

EXPORT(int, taiMemcpyKernelToUser, Ptr<void> user_dst, Ptr<const void> kernel_src, uint32_t len) {
    if (!user_dst || !kernel_src)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    std::memcpy(user_dst.get(emuenv.mem), kernel_src.get(emuenv.mem), len);
    return 0;
}

EXPORT(int, taiReloadConfig) {
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    // Force reload on next access
    state->config_loaded = false;
    LOG_DEBUG("taiReloadConfig: config will be reloaded on next access");
    return 0;
}

// ==================== taihenForKernel library ====================

EXPORT(SceUID, taiHookFunctionExportForKernel, SceUID pid, Ptr<uint32_t> p_hook,
    const char *module, uint32_t library_nid, uint32_t func_nid, Ptr<const void> hook_func) {
    STUBBED("taiHookFunctionExportForKernel");
    return 0;
}

EXPORT(SceUID, taiHookFunctionImportForKernel, SceUID pid, Ptr<uint32_t> p_hook,
    const char *module, uint32_t import_library_nid, uint32_t import_func_nid, Ptr<const void> hook_func) {
    STUBBED("taiHookFunctionImportForKernel");
    return 0;
}

EXPORT(SceUID, taiHookFunctionOffsetForKernel, SceUID pid, Ptr<uint32_t> p_hook,
    SceUID modid, int segidx, uint32_t offset, int thumb, Ptr<const void> hook_func) {
    STUBBED("taiHookFunctionOffsetForKernel");
    return 0;
}

EXPORT(int, taiGetModuleInfoForKernel, SceUID pid, const char *module, Ptr<void> info) {
    // Delegate to the user version — in emulator there's no pid separation
    return export_taiGetModuleInfo(emuenv, thread_id, export_name, module, info);
}

EXPORT(int, taiHookReleaseForKernel, SceUID tai_uid, uint32_t hook_ref) {
    STUBBED("taiHookReleaseForKernel");
    return 0;
}

EXPORT(SceUID, taiInjectAbsForKernel, SceUID pid, Ptr<void> dest, Ptr<const void> src, uint32_t size) {
    // Delegate — no pid separation in emulator
    return export_taiInjectAbs(emuenv, thread_id, export_name, dest, src, size);
}

EXPORT(SceUID, taiInjectDataForKernel, SceUID pid, SceUID modid, int segidx, uint32_t offset,
    Ptr<const void> data, uint32_t size) {
    STUBBED("taiInjectDataForKernel");
    return 0;
}

EXPORT(int, taiInjectReleaseForKernel, SceUID tai_uid) {
    return export_taiInjectRelease(emuenv, thread_id, export_name, tai_uid);
}

EXPORT(int, taiLoadPluginsForTitleForKernel, SceUID pid, const char *titleid, int flags) {
    STUBBED("taiLoadPluginsForTitleForKernel");
    return 0;
}

EXPORT(int, taiReloadConfigForKernel, int schedule, int load_kernel) {
    return export_taiReloadConfig(emuenv, thread_id, export_name);
}

// ==================== taihenModuleUtils library ====================

EXPORT(int, module_get_by_name_nid, SceUID pid, const char *name, uint32_t nid, Ptr<uint32_t> info) {
    STUBBED("module_get_by_name_nid");
    return 0;
}

EXPORT(int, module_get_offset, SceUID pid, SceUID modid, int segidx, uint32_t offset, Ptr<uint32_t> addr) {
    if (!addr)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
    if (mod_it == emuenv.kernel.loaded_modules.end()) {
        LOG_WARN("module_get_offset: module {} not found", modid);
        return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
    }

    const auto &mod = mod_it->second->info;
    if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    Address result = mod.segments[segidx].vaddr.address() + offset;
    *addr.get(emuenv.mem) = result;

    LOG_DEBUG("module_get_offset: mod={} seg={} off={} -> {}", modid, segidx, log_hex(offset), log_hex(result));
    return 0;
}

EXPORT(int, module_get_export_func, SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, Ptr<uint32_t> func) {
    if (!func)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    // First, check runtime export_nids (LLE exports and previously created dynamic stubs)
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        auto it = emuenv.kernel.export_nids.find(funcnid);
        if (it != emuenv.kernel.export_nids.end()) {
            *func.get(emuenv.mem) = it->second;
            LOG_DEBUG("module_get_export_func: found NID {} at {} (export_nids)", log_hex(funcnid), log_hex(it->second));
            return 0;
        }
    }

    // Not found in export_nids — try to create a dynamic SVC stub for HLE functions
    // This is needed for HLE modules that don't have ARM code addresses
    // Check if the NID is known to our HLE system via resolve_import
    // We create a stub: svc #0; mov pc, lr; NID
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);

        // Allocate 12 bytes for the stub in emulated memory
        Address stub_addr = alloc(emuenv.mem, 12, "taihen_dynamic_stub");
        if (!stub_addr) {
            LOG_ERROR("module_get_export_func: failed to allocate dynamic stub for NID {}", log_hex(funcnid));
            return SCE_KERNEL_ERROR_NO_MEMORY;
        }

        uint32_t *stub = Ptr<uint32_t>(stub_addr).get(emuenv.mem);
        stub[0] = 0xef000000; // svc #0
        stub[1] = 0xe1a0f00e; // mov pc, lr
        stub[2] = funcnid; // NID for dispatch

        // Register in export_nids so future lookups find it directly
        emuenv.kernel.export_nids.emplace(funcnid, stub_addr);
        emuenv.kernel.invalidate_jit_cache(stub_addr, 12);

        *func.get(emuenv.mem) = stub_addr;
        LOG_DEBUG("module_get_export_func: created dynamic SVC stub for NID {} at {}", log_hex(funcnid), log_hex(stub_addr));
        return 0;
    }
}

EXPORT(int, module_get_import_func, SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, Ptr<uint32_t> func) {
    STUBBED("module_get_import_func");
    return 0;
}

// ==================== Plugin Loading ====================

void load_taihen_config(EmuEnvState &emuenv) {
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state || state->config_loaded)
        return;

    // Try ux0:tai/config.txt first, then ur0:tai/config.txt
    vfs::FileBuffer buf;
    if (!vfs::read_file(VitaIoDevice::ux0, buf, emuenv.pref_path, "tai/config.txt")) {
        vfs::read_file(VitaIoDevice::ur0, buf, emuenv.pref_path, "tai/config.txt");
    }

    if (buf.empty()) {
        LOG_DEBUG("taiHEN: no config.txt found");
        state->config_loaded = true;
        return;
    }

    std::string content(buf.begin(), buf.end());
    state->config = parse_taihen_config(content);
    state->config_loaded = true;

    LOG_INFO("taiHEN: loaded config.txt with {} sections", state->config.size());
    for (const auto &[section, plugins] : state->config) {
        LOG_INFO("  [{}]: {} plugins", section, plugins.size());
        for (const auto &p : plugins) {
            LOG_INFO("    {}", p);
        }
    }
}

void load_taihen_plugins_for_title(EmuEnvState &emuenv, const std::string &titleid) {
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return;

    load_taihen_config(emuenv);

    // Load KERNEL plugins (once)
    if (!state->kernel_plugins_loaded) {
        auto kernel_it = state->config.find("KERNEL");
        if (kernel_it != state->config.end()) {
            for (const auto &plugin_path : kernel_it->second) {
                LOG_INFO("taiHEN: loading kernel plugin: {}", plugin_path);
                SceUID uid = load_module(emuenv, plugin_path);
                if (uid >= 0) {
                    const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
                    if (mod_it != emuenv.kernel.loaded_modules.end()) {
                        start_module(emuenv, mod_it->second->info);
                    }
                } else {
                    LOG_ERROR("taiHEN: failed to load kernel plugin: {} (error {})", plugin_path, uid);
                }
            }
        }
        state->kernel_plugins_loaded = true;
    }

    // Load title-specific plugins
    auto title_it = state->config.find(titleid);
    if (title_it != state->config.end()) {
        for (const auto &plugin_path : title_it->second) {
            LOG_INFO("taiHEN: loading plugin for {}: {}", titleid, plugin_path);
            SceUID uid = load_module(emuenv, plugin_path);
            if (uid >= 0) {
                const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
                if (mod_it != emuenv.kernel.loaded_modules.end()) {
                    start_module(emuenv, mod_it->second->info);
                }
            } else {
                LOG_ERROR("taiHEN: failed to load plugin: {} (error {})", plugin_path, uid);
            }
        }
    }
}

// ==================== Library Init ====================

LIBRARY_INIT(SceLibTaihen) {
    emuenv.kernel.obj_store.create<TaihenState>();
    LOG_INFO("taiHEN: HLE module initialized");
}
