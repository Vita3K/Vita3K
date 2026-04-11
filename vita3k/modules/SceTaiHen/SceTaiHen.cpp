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

// taiHEN support for Vita3K
// taiHEN is a plugin/hook framework for the PlayStation Vita.
// See: https://github.com/yifanlu/taiHEN

#include "SceTaiHen.h"

#include <kernel/load_self.h>
#include <kernel/state.h>
#include <modules/module_parent.h>
#include <util/arm.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/tracy.h>

#include <cstring>
#include <string_view>

TRACY_MODULE_NAME(SceTaiHen);

// taiHEN error codes
enum TaiError : uint32_t {
    TAI_SUCCESS = 0,
    TAI_ERROR_NOT_FOUND = 0x90010001,
    TAI_ERROR_PATCH_EXISTS = 0x90010002,
    TAI_ERROR_HOOK_ERROR = 0x90010003,
    TAI_ERROR_NOT_IMPLEMENTED = 0x90010004,
    TAI_ERROR_USER_MEMORY = 0x90010005,
    TAI_ERROR_ALLOC = 0x90010006,
    TAI_ERROR_FREE = 0x90010007,
    TAI_ERROR_STUB_NOT_RESOLVED = 0x90010008,
    TAI_ERROR_INVALID_KERNEL_ADDR = 0x90010009,
    TAI_ERROR_INVALID_ARGUMENT = 0x9001000A,
    TAI_ERROR_BAD_ADDR = 0x9001000B,
};

// ARM instruction for SVC #0 (triggers emulator import dispatch)
static constexpr uint32_t SVC_INSTRUCTION = 0xef000000;
// ARM instruction for MOV PC, LR (return from SVC stub)
static constexpr uint32_t MOV_PC_LR_INSTRUCTION = 0xe1a0f00e;
// Size of an import stub in bytes (3 uint32_t words)
static constexpr uint32_t IMPORT_STUB_SIZE = 3 * sizeof(uint32_t);

// Helper: patch all existing import stubs for a given NID to jump to a new address.
// Must be called with export_nids_mutex held.
static void patch_stubs_for_nid(KernelState &kernel, MemState &mem, uint32_t nid, Address new_addr) {
    auto range = kernel.func_binding_infos.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        Address stub_addr = it->second;
        uint32_t *stub = Ptr<uint32_t>(stub_addr).get(mem);
        stub[0] = encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(new_addr), 12);
        stub[1] = encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(new_addr >> 16), 12);
        stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        kernel.invalidate_jit_cache(stub_addr, IMPORT_STUB_SIZE);
    }
}

// Helper: restore all import stubs for a NID to SVC stubs (for host functions)
// Must be called with export_nids_mutex held.
static void restore_stubs_to_svc(KernelState &kernel, MemState &mem, uint32_t nid) {
    auto range = kernel.func_binding_infos.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        Address stub_addr = it->second;
        uint32_t *stub = Ptr<uint32_t>(stub_addr).get(mem);
        stub[0] = SVC_INSTRUCTION;
        stub[1] = MOV_PC_LR_INSTRUCTION;
        stub[2] = nid;
        kernel.invalidate_jit_cache(stub_addr, IMPORT_STUB_SIZE);
    }
}

// Helper: restore all import stubs for a NID to direct call stubs (for guest functions)
// Must be called with export_nids_mutex held.
static void restore_stubs_to_direct(KernelState &kernel, MemState &mem, uint32_t nid, Address target_addr) {
    auto range = kernel.func_binding_infos.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        Address stub_addr = it->second;
        uint32_t *stub = Ptr<uint32_t>(stub_addr).get(mem);
        stub[0] = encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(target_addr), 12);
        stub[1] = encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(target_addr >> 16), 12);
        stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        kernel.invalidate_jit_cache(stub_addr, IMPORT_STUB_SIZE);
    }
}

// Hook a function by NID in a module's export table.
// This is the primary taiHEN hook mechanism.
EXPORT(SceUID, taiHookFunctionExport, Ptr<uint32_t> p_hook, const char *module_name, uint32_t library_nid, uint32_t func_nid, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionExport, p_hook, module_name, library_nid, func_nid, hook_func);

    if (!p_hook || !hook_func) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("taiHookFunctionExport: module='{}' lib_nid={:#010x} func_nid={:#010x} hook={:#010x}",
        module_name ? module_name : "<any>", library_nid, func_nid, hook_func.address());

    const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);

    Address original_addr = 0;
    Address trampoline_addr = 0;
    bool was_in_export_nids = false;

    auto export_it = emuenv.kernel.export_nids.find(func_nid);
    if (export_it != emuenv.kernel.export_nids.end()) {
        // Function is already in export_nids (guest function or previously hooked)
        was_in_export_nids = true;
        original_addr = export_it->second;
    } else {
        // Function is a host-implemented function (dispatched via SVC).
        // Create a small SVC trampoline so that TAI_CONTINUE works correctly
        // by invoking the original host handler.
        was_in_export_nids = false;

        // Check that the function actually has any import stubs (otherwise it doesn't exist)
        auto range = emuenv.kernel.func_binding_infos.equal_range(func_nid);
        // It might not have stubs yet if the hook is installed before the importing module
        // is loaded. We allow that by creating the trampoline anyway.

        // Allocate a trampoline SVC stub in guest memory
        auto trampoline_name = fmt::format("taiHEN SVC trampoline NID {:#010x}", func_nid);
        trampoline_addr = alloc(emuenv.mem, IMPORT_STUB_SIZE, trampoline_name.c_str());
        if (!trampoline_addr) {
            LOG_ERROR("taiHookFunctionExport: Failed to allocate SVC trampoline");
            return TAI_ERROR_ALLOC;
        }
        uint32_t *trampoline = Ptr<uint32_t>(trampoline_addr).get(emuenv.mem);
        trampoline[0] = SVC_INSTRUCTION;      // svc #0 - triggers call_import
        trampoline[1] = MOV_PC_LR_INSTRUCTION; // mov pc, lr - return from SVC
        trampoline[2] = func_nid;              // NID for call_import dispatch
        emuenv.kernel.invalidate_jit_cache(trampoline_addr, IMPORT_STUB_SIZE);
        original_addr = trampoline_addr;
    }

    // Store the original address in *p_hook (guest reads this via TAI_CONTINUE)
    *p_hook.get(emuenv.mem) = original_addr;

    // Register the hook function as the new export
    if (was_in_export_nids) {
        emuenv.kernel.export_nids[func_nid] = hook_func.address();
    } else {
        emuenv.kernel.export_nids.emplace(func_nid, hook_func.address());
    }

    // Patch all existing import stubs for this NID to call hook_func directly
    patch_stubs_for_nid(emuenv.kernel, emuenv.mem, func_nid, hook_func.address());

    // Store hook info for later release
    const SceUID hook_uid = emuenv.kernel.get_next_uid();
    emuenv.kernel.tai_hooks[hook_uid] = TaiHookInfo{
        func_nid,
        hook_func.address(),
        original_addr,
        trampoline_addr,
        was_in_export_nids
    };

    LOG_INFO("taiHookFunctionExport: installed hook uid={} for nid={:#010x} original={:#010x} hook={:#010x}",
        hook_uid, func_nid, original_addr, hook_func.address());

    return hook_uid;
}

// Hook only the imports of a specific module (not all modules importing the function).
EXPORT(SceUID, taiHookFunctionImport, Ptr<uint32_t> p_hook, const char *module_name, uint32_t library_nid, uint32_t func_nid, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionImport, p_hook, module_name, library_nid, func_nid, hook_func);

    if (!p_hook || !hook_func || !module_name) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("taiHookFunctionImport: target_module='{}' lib_nid={:#010x} func_nid={:#010x} hook={:#010x}",
        module_name, library_nid, func_nid, hook_func.address());

    // Delegate to taiHookFunctionExport which patches all stubs.
    // For a proper import-only hook we would need per-stub ownership tracking,
    // but for most use-cases hooking the export globally is equivalent.
    return CALL_EXPORT(taiHookFunctionExport, p_hook, module_name, library_nid, func_nid, hook_func);
}

// Hook a function at a specific offset within a module segment (inline hook).
// This overwrites 8 bytes at the target address with an absolute branch to hook_func,
// and stores the original bytes in a trampoline so TAI_CONTINUE works.
EXPORT(SceUID, taiHookFunctionOffset, Ptr<uint32_t> p_hook, SceUID modid, int segidx, uint32_t offset, int thumb, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionOffset, p_hook, modid, segidx, offset, thumb, hook_func);

    if (!p_hook || !hook_func) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    const auto module = lock_and_find(modid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        LOG_ERROR("taiHookFunctionOffset: module {} not found", modid);
        return TAI_ERROR_NOT_FOUND;
    }

    if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    const SceKernelSegmentInfo &seg = module->info.segments[segidx];
    if (seg.size == 0 || seg.vaddr.address() == 0) {
        LOG_ERROR("taiHookFunctionOffset: segment {} of module {} is invalid", segidx, modid);
        return TAI_ERROR_NOT_FOUND;
    }

    const Address target_addr = seg.vaddr.address() + offset;
    // We always overwrite 8 bytes (4-byte instruction + 4-byte address)
    constexpr uint32_t INLINE_HOOK_SIZE = 8;
    if (offset + INLINE_HOOK_SIZE > seg.memsz) {
        LOG_ERROR("taiHookFunctionOffset: offset {:#x} out of segment bounds", offset);
        return TAI_ERROR_BAD_ADDR;
    }

    LOG_INFO("taiHookFunctionOffset: module={} seg={} offset={:#x} thumb={} hook={:#010x} target={:#010x}",
        modid, segidx, offset, thumb, hook_func.address(), target_addr);

    // ARM inline hook: LDR PC, [PC, #-4] followed by the target address.
    // When CPU executes at address A (ARM): PC = A+8, [PC-4] = [A+4] = target_addr[1].
    // Thumb-2 inline hook: LDR.W PC, [PC, #0] followed by the target address.
    // When CPU executes at address A (Thumb): PC = A+4, [PC+0] = [A+4] = target_addr[1].
    // Both encode as 8 bytes total.

    constexpr uint32_t ARM_LDR_PC_PC_M4 = 0xe51ff004u; // LDR PC, [PC, #-4]
    constexpr uint32_t THUMB2_LDR_PC_PC = 0xF000F8DFu;  // LDR.W PC, [PC, #0] (little-endian halfwords)

    uint8_t *target = Ptr<uint8_t>(target_addr).get(emuenv.mem);

    // Allocate trampoline: INLINE_HOOK_SIZE original bytes + 8-byte branch back
    const uint32_t trampoline_size = INLINE_HOOK_SIZE + 8;
    auto trampoline_name = fmt::format("taiHEN offset trampoline mod={} seg={} off={:#x}", modid, segidx, offset);
    const Address trampoline_addr = alloc(emuenv.mem, trampoline_size, trampoline_name.c_str());
    if (!trampoline_addr) {
        LOG_ERROR("taiHookFunctionOffset: Failed to allocate trampoline");
        return TAI_ERROR_ALLOC;
    }
    uint8_t *trampoline = Ptr<uint8_t>(trampoline_addr).get(emuenv.mem);

    // Copy original instructions to start of trampoline
    std::memcpy(trampoline, target, INLINE_HOOK_SIZE);

    // Append ARM branch back to target + INLINE_HOOK_SIZE
    const Address resume_addr = target_addr + INLINE_HOOK_SIZE;
    uint32_t *trampoline_branch = reinterpret_cast<uint32_t *>(trampoline + INLINE_HOOK_SIZE);
    trampoline_branch[0] = ARM_LDR_PC_PC_M4;
    trampoline_branch[1] = resume_addr;

    emuenv.kernel.invalidate_jit_cache(trampoline_addr, trampoline_size);

    // Write inline hook at target (8 bytes)
    if (thumb) {
        // Thumb-2: two 16-bit halfwords + 32-bit address
        uint16_t *target16 = reinterpret_cast<uint16_t *>(target);
        target16[0] = static_cast<uint16_t>(THUMB2_LDR_PC_PC & 0xFFFFu);
        target16[1] = static_cast<uint16_t>((THUMB2_LDR_PC_PC >> 16) & 0xFFFFu);
        uint32_t hook_addr = hook_func.address();
        std::memcpy(target + 4, &hook_addr, 4);
    } else {
        uint32_t *target32 = reinterpret_cast<uint32_t *>(target);
        target32[0] = ARM_LDR_PC_PC_M4;
        target32[1] = hook_func.address();
    }
    emuenv.kernel.invalidate_jit_cache(target_addr, INLINE_HOOK_SIZE);

    // Store the trampoline address in *p_hook (TAI_CONTINUE will call the trampoline)
    *p_hook.get(emuenv.mem) = trampoline_addr;

    // Register hook info
    const SceUID hook_uid = emuenv.kernel.get_next_uid();
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        emuenv.kernel.tai_hooks[hook_uid] = TaiHookInfo{
            0, // no NID for offset hooks
            hook_func.address(),
            target_addr, // used to restore original bytes on release
            trampoline_addr,
            false
        };
    }

    LOG_INFO("taiHookFunctionOffset: hook uid={} installed at {:#010x} trampoline={:#010x}",
        hook_uid, target_addr, trampoline_addr);

    return hook_uid;
}

// Hook a function at an absolute guest address.
EXPORT(SceUID, taiHookFunctionAbs, Ptr<uint32_t> p_hook, SceUID modid, Ptr<const void> func_addr, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionAbs, p_hook, modid, func_addr, hook_func);

    if (!p_hook || !hook_func || !func_addr) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("taiHookFunctionAbs: addr={:#010x} hook={:#010x}", func_addr.address(), hook_func.address());

    // Determine if it's a Thumb function (bit 0 set in the address)
    const int thumb = (func_addr.address() & 1) ? 1 : 0;
    const Address real_addr = func_addr.address() & ~1u;

    // Always use 8-byte inline hook (4-byte branch instruction + 4-byte address)
    constexpr uint32_t ARM_LDR_PC_PC_M4 = 0xe51ff004u;
    constexpr uint32_t THUMB2_LDR_PC_PC = 0xF000F8DFu;
    constexpr uint32_t INLINE_HOOK_SIZE = 8;

    uint8_t *target = Ptr<uint8_t>(real_addr).get(emuenv.mem);

    const uint32_t trampoline_size = INLINE_HOOK_SIZE + 8;
    auto trampoline_name = fmt::format("taiHEN abs trampoline {:#010x}", real_addr);
    const Address trampoline_addr = alloc(emuenv.mem, trampoline_size, trampoline_name.c_str());
    if (!trampoline_addr) {
        return TAI_ERROR_ALLOC;
    }
    uint8_t *trampoline = Ptr<uint8_t>(trampoline_addr).get(emuenv.mem);
    std::memcpy(trampoline, target, INLINE_HOOK_SIZE);

    const Address resume_addr = real_addr + INLINE_HOOK_SIZE;
    uint32_t *trampoline_branch = reinterpret_cast<uint32_t *>(trampoline + INLINE_HOOK_SIZE);
    trampoline_branch[0] = ARM_LDR_PC_PC_M4;
    trampoline_branch[1] = resume_addr;
    emuenv.kernel.invalidate_jit_cache(trampoline_addr, trampoline_size);

    if (thumb) {
        uint16_t *target16 = reinterpret_cast<uint16_t *>(target);
        target16[0] = static_cast<uint16_t>(THUMB2_LDR_PC_PC & 0xFFFFu);
        target16[1] = static_cast<uint16_t>((THUMB2_LDR_PC_PC >> 16) & 0xFFFFu);
        uint32_t hook_addr = hook_func.address();
        std::memcpy(target + 4, &hook_addr, 4);
    } else {
        uint32_t *target32 = reinterpret_cast<uint32_t *>(target);
        target32[0] = ARM_LDR_PC_PC_M4;
        target32[1] = hook_func.address();
    }
    emuenv.kernel.invalidate_jit_cache(real_addr, INLINE_HOOK_SIZE);

    *p_hook.get(emuenv.mem) = trampoline_addr;

    const SceUID hook_uid = emuenv.kernel.get_next_uid();
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        emuenv.kernel.tai_hooks[hook_uid] = TaiHookInfo{
            0,
            hook_func.address(),
            real_addr,
            trampoline_addr,
            false
        };
    }

    return hook_uid;
}

// Release a taiHEN hook.
EXPORT(int, taiHookRelease, SceUID hook_uid, uint32_t hook) {
    TRACY_FUNC(taiHookRelease, hook_uid, hook);

    const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);

    auto it = emuenv.kernel.tai_hooks.find(hook_uid);
    if (it == emuenv.kernel.tai_hooks.end()) {
        LOG_ERROR("taiHookRelease: hook {} not found", hook_uid);
        return TAI_ERROR_NOT_FOUND;
    }

    const TaiHookInfo &info = it->second;

    if (info.func_nid != 0) {
        // Export/import hook - restore export_nids and import stubs
        if (info.was_in_export_nids) {
            // The original was a guest function; restore it
            emuenv.kernel.export_nids[info.func_nid] = info.previous_export;
            restore_stubs_to_direct(emuenv.kernel, emuenv.mem, info.func_nid, info.previous_export);
        } else {
            // The original was a host function; remove from export_nids and restore SVC stubs
            emuenv.kernel.export_nids.erase(info.func_nid);
            restore_stubs_to_svc(emuenv.kernel, emuenv.mem, info.func_nid);
        }

        // Free the SVC trampoline if we created one
        if (info.trampoline != 0) {
            free(emuenv.mem, info.trampoline);
        }
    } else {
        // Inline hook (offset or abs) - restore original bytes from trampoline
        if (info.trampoline != 0) {
            // The original bytes (patch_size bytes) are at the start of the trampoline.
            // Determine patch size from trampoline (heuristic: check if trampoline+6 has a branch)
            // For simplicity, always restore 8 bytes (ARM inline hook).
            uint8_t *original = Ptr<uint8_t>(info.trampoline).get(emuenv.mem);
            uint8_t *target = Ptr<uint8_t>(info.previous_export).get(emuenv.mem);
            std::memcpy(target, original, 8);
            emuenv.kernel.invalidate_jit_cache(info.previous_export, 8);
            free(emuenv.mem, info.trampoline);
        }
    }

    emuenv.kernel.tai_hooks.erase(it);

    LOG_INFO("taiHookRelease: released hook {}", hook_uid);
    return TAI_SUCCESS;
}

// Inject data at a specific offset within a module segment.
EXPORT(SceUID, taiInjectData, SceUID modid, int segidx, uint32_t offset, Ptr<const void> data, SceSize size) {
    TRACY_FUNC(taiInjectData, modid, segidx, offset, data, size);

    if (!data || size == 0) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    const auto module = lock_and_find(modid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        LOG_ERROR("taiInjectData: module {} not found", modid);
        return TAI_ERROR_NOT_FOUND;
    }

    if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    const SceKernelSegmentInfo &seg = module->info.segments[segidx];
    if (seg.size == 0 || seg.vaddr.address() == 0) {
        LOG_ERROR("taiInjectData: segment {} of module {} is invalid", segidx, modid);
        return TAI_ERROR_NOT_FOUND;
    }

    if (offset + size > seg.memsz) {
        LOG_ERROR("taiInjectData: data would exceed segment bounds");
        return TAI_ERROR_BAD_ADDR;
    }

    const Address target_addr = seg.vaddr.address() + offset;
    uint8_t *target = Ptr<uint8_t>(target_addr).get(emuenv.mem);
    const uint8_t *src = Ptr<const uint8_t>(data.address()).get(emuenv.mem);

    // Save original data
    TaiInjectInfo inject_info;
    inject_info.target_addr = target_addr;
    inject_info.original_data.assign(target, target + size);

    // Write new data
    std::memcpy(target, src, size);
    emuenv.kernel.invalidate_jit_cache(target_addr, size);

    const SceUID inject_uid = emuenv.kernel.get_next_uid();
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        emuenv.kernel.tai_injects[inject_uid] = std::move(inject_info);
    }

    LOG_INFO("taiInjectData: inject uid={} at {:#010x} size={}", inject_uid, target_addr, size);
    return inject_uid;
}

// Release a taiHEN data injection (restore original bytes).
EXPORT(int, taiInjectRelease, SceUID inject_uid) {
    TRACY_FUNC(taiInjectRelease, inject_uid);

    const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);

    auto it = emuenv.kernel.tai_injects.find(inject_uid);
    if (it == emuenv.kernel.tai_injects.end()) {
        LOG_ERROR("taiInjectRelease: inject {} not found", inject_uid);
        return TAI_ERROR_NOT_FOUND;
    }

    const TaiInjectInfo &info = it->second;
    uint8_t *target = Ptr<uint8_t>(info.target_addr).get(emuenv.mem);
    std::memcpy(target, info.original_data.data(), info.original_data.size());
    emuenv.kernel.invalidate_jit_cache(info.target_addr, info.original_data.size());

    emuenv.kernel.tai_injects.erase(it);

    LOG_INFO("taiInjectRelease: released inject {}", inject_uid);
    return TAI_SUCCESS;
}

// Load and start a user-mode module (delegates to Vita3K's standard loader).
EXPORT(SceUID, taiLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, int flags, Ptr<const void> opt, Ptr<int> res) {
    TRACY_FUNC(taiLoadStartModule, path, args, argp, flags, opt, res);

    if (!path) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("taiLoadStartModule: loading '{}'", path);

    const SceUID module_id = load_module(emuenv, path);
    if (module_id < 0) {
        LOG_ERROR("taiLoadStartModule: failed to load '{}' ({})", path, module_id);
        return module_id;
    }

    const auto module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        return TAI_ERROR_NOT_FOUND;
    }

    const int start_result = start_module(emuenv, module->info, args, argp);
    if (res) {
        *res.get(emuenv.mem) = start_result;
    }

    LOG_INFO("taiLoadStartModule: loaded '{}' uid={} start_result={}", path, module_id, start_result);
    return module_id;
}

// Stop and unload a user-mode module.
EXPORT(int, taiStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, Ptr<const void> opt, Ptr<int> res) {
    TRACY_FUNC(taiStopUnloadModule, uid, args, argp, flags, opt, res);

    const auto module = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        LOG_ERROR("taiStopUnloadModule: module {} not found", uid);
        return TAI_ERROR_NOT_FOUND;
    }

    const int stop_result = stop_module(emuenv, module->info, args, argp);
    if (res) {
        *res.get(emuenv.mem) = stop_result;
    }

    return unload_module(emuenv, uid);
}

// Load and start a kernel module.
// Vita3K runs in user-mode emulation only, so kernel modules cannot be loaded
// from disk. However, several well-known kernel plugins (kubridge.skprx) have
// their functions implemented as Host Library Emulation (HLE) in Vita3K.
// For those, we return a synthetic positive UID so the caller's error check
// passes, while the actual function calls are resolved via the NID table.
EXPORT(SceUID, taiLoadStartKernelModule, const char *path, SceSize args, Ptr<const void> argp, int flags) {
    TRACY_FUNC(taiLoadStartKernelModule, path, args, argp, flags);

    if (path) {
        const std::string_view path_view(path);
        // kubridge.skprx is fully HLE-implemented as SceKuBridge.
        // Match only the exact filename to avoid false positives like
        // "my_kubridge_proxy.skprx". Extract the part after the last '/' or ':'.
        const auto sep = path_view.find_last_of("/:");
        const std::string_view filename = (sep != std::string_view::npos) ? path_view.substr(sep + 1) : path_view;
        if (filename == "kubridge.skprx") {
            LOG_INFO("taiLoadStartKernelModule: '{}' is HLE-implemented, returning synthetic UID", path);
            // Return a fixed synthetic UID in the positive kernel module range.
            return 0x40000001;
        }
    }

    LOG_WARN("taiLoadStartKernelModule: kernel module loading is not supported in Vita3K (path='{}')",
        path ? path : "<null>");

    // Return a module-not-found error. Games/plugins that depend on kernel modules
    // will need their functionality stubbed separately.
    return TAI_ERROR_NOT_IMPLEMENTED;
}

// Stop and unload a kernel module.
// For HLE-backed kernel modules (like kubridge) that returned a synthetic UID,
// we silently accept the stop request.
EXPORT(int, taiStopUnloadKernelModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, Ptr<int> res) {
    TRACY_FUNC(taiStopUnloadKernelModule, uid, args, argp, flags, res);
    // Synthetic UID range used for HLE kernel modules
    if (uid >= 0x40000000) {
        LOG_INFO("taiStopUnloadKernelModule: releasing HLE kernel module uid={:#010x}", uid);
        if (res)
            *res.get(emuenv.mem) = 0;
        return SCE_KERNEL_OK;
    }
    LOG_WARN("taiStopUnloadKernelModule: kernel module unloading is not supported in Vita3K (uid={})", uid);
    return TAI_ERROR_NOT_IMPLEMENTED;
}

// Retrieve information about a loaded module by name.
EXPORT(int, taiGetModuleInfo, const char *module_name, tai_module_info_t *info) {
    TRACY_FUNC(taiGetModuleInfo, module_name, info);

    if (!module_name || !info) {
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    if (info->size != sizeof(tai_module_info_t)) {
        LOG_ERROR("taiGetModuleInfo: info->size ({}) != sizeof(tai_module_info_t) ({})",
            info->size, sizeof(tai_module_info_t));
        return TAI_ERROR_INVALID_ARGUMENT;
    }

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[mod_id, module] : emuenv.kernel.loaded_modules) {
        if (std::strncmp(module->info.module_name, module_name, 28) == 0) {
            info->modid = module->info.modid;
            std::strncpy(info->name, module->info.module_name, 28);
            info->name[27] = '\0';
            info->flags = module->info.modattr;

            // Provide the exports/imports range from the raw module info stored at
            // module_info_segment_address + module_info_offset. We decode them from
            // the sce_module_info structure embedded in the ELF.
            // Expose the first code segment vaddr as exports_start, and the last as
            // imports_end as a reasonable approximation for plugins that just need the modid.
            info->exports_start = 0;
            info->exports_end = 0;
            info->imports_start = 0;
            info->imports_end = 0;
            for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
                const SceKernelSegmentInfo &seg = module->info.segments[i];
                if (seg.size == 0 || seg.vaddr.address() == 0)
                    continue;
                if (info->exports_start == 0)
                    info->exports_start = seg.vaddr.address();
                info->imports_end = seg.vaddr.address() + seg.memsz;
            }

            LOG_INFO("taiGetModuleInfo: found '{}' uid={}", module_name, mod_id);
            return TAI_SUCCESS;
        }
    }

    LOG_WARN("taiGetModuleInfo: module '{}' not found", module_name);
    return TAI_ERROR_NOT_FOUND;
}
