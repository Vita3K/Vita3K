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
#include <nids/functions.h>
#include <util/arm.h>
#include <util/log.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(taiHEN);

// substitute library for instruction relocation (from comex/substitute, LGPLv2.1+)
#include <substitute_api.h>

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// taiHEN error codes (from taihen.h)
constexpr int TAI_ERROR_NOT_FOUND = 0x90010002;
constexpr int TAI_ERROR_HOOK_ERROR = 0x90010006;

// ==================== taiHEN State ====================

enum TaiHookType {
    TAI_HOOK_IMPORT, // Patched an import stub (12 bytes)
    TAI_HOOK_INLINE, // Patched function prologue (inline hook)
};

struct TaiHook {
    SceUID uid;
    SceUID pid;
    TaiHookType type;
    Address target_addr; // address of the patched stub/code (stripped of Thumb bit)
    Address hook_func; // ARM address of the hook function
    Address trampoline; // ARM address of trampoline to call original
    Address hook_user_addr; // address of _tai_hook_user in emulated memory
    uint32_t func_nid; // NID (for export hooks, used in release)
    int patch_size; // number of bytes patched at target (for inline hooks)
    bool is_thumb; // true if the hooked function was Thumb mode
    std::vector<uint8_t> saved_bytes; // original bytes at target
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
    std::map<SceUID, std::unique_ptr<TaiHook>> hooks;
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

// ==================== Hook Helper Functions ====================

// Layout of tai_hook_args_t in emulated memory (user side)
struct TaiHookArgs {
    uint32_t size;
    Address module_name; // Ptr<char>
    uint32_t library_nid;
    uint32_t func_nid;
    Address hook_func; // ARM address
};

// Layout of tai_offset_args_t in emulated memory
struct TaiOffsetArgs {
    uint32_t size;
    SceUID modid;
    int segidx;
    uint32_t offset;
    int thumb;
    Address hook_func; // ARM address
};

// Layout of _tai_hook_user in emulated memory (what TAI_CONTINUE reads)
struct TaiHookUser {
    uint32_t next; // Address of next hook_user (or 0)
    uint32_t func; // ARM address of hook function
    uint32_t old; // ARM address of original function (trampoline)
};

// Decode the target address from an LLE jump stub (MOVW+MOVT+BX r12)
static Address decode_lle_stub_target(const uint32_t *stub) {
    // MOVW r12, #imm16: encoded as (0xE30 << 20) | imm4:12 | reg << 12 | imm12
    uint32_t movw = stub[0];
    uint32_t movt = stub[1];
    uint16_t lo = ((movw >> 4) & 0xF000) | (movw & 0xFFF);
    uint16_t hi = ((movt >> 4) & 0xF000) | (movt & 0xFFF);
    return (static_cast<Address>(hi) << 16) | lo;
}

// Check if a stub is an SVC (HLE) stub: svc #0; mov pc, lr; NID
static bool is_svc_stub(const uint32_t *stub) {
    return stub[0] == 0xef000000 && stub[1] == 0xe1a0f00e;
}

// Find the import stub address for a given NID within a specific module
static Address find_import_stub_in_module(EmuEnvState &emuenv, const char *module_name, uint32_t func_nid) {
    // Find the module UID
    SceUID mod_uid = -1;
    for (const auto &[uid, mod] : emuenv.kernel.loaded_modules) {
        if (std::string(mod->info.module_name) == module_name) {
            mod_uid = uid;
            break;
        }
    }
    if (mod_uid < 0) {
        LOG_DEBUG("find_import_stub: module '{}' not found in loaded_modules", module_name);
        return 0;
    }

    const auto &mod = emuenv.kernel.loaded_modules[mod_uid];

    // Search func_binding_infos for stubs with this NID
    auto range = emuenv.kernel.func_binding_infos.equal_range(func_nid);
    size_t count = std::distance(range.first, range.second);
    if (count == 0) {
        LOG_DEBUG("find_import_stub: NID {} has 0 entries in func_binding_infos", log_hex(func_nid));
        return 0;
    }

    for (auto it = range.first; it != range.second; ++it) {
        Address addr = it->second;
        // Check if this stub address falls within the module's segments
        for (int seg = 0; seg < MODULE_INFO_NUM_SEGMENTS; seg++) {
            Address seg_start = mod->info.segments[seg].vaddr.address();
            uint32_t seg_size = mod->info.segments[seg].memsz;
            if (seg_size > 0 && addr >= seg_start && addr < seg_start + seg_size) {
                return addr;
            }
        }
        LOG_DEBUG("find_import_stub: NID {} stub at {} not in any segment of '{}'",
            log_hex(func_nid), log_hex(addr), module_name);
    }
    return 0;
}

// Create a hook: patch a stub at target_addr to jump to hook_func,
// create trampoline + hook_user struct, return UID
// This is for IMPORT hooks where target_addr points to a 12-byte import stub.
static SceUID create_hook(EmuEnvState &emuenv, TaihenState *state, SceUID thread_id,
    Address target_addr, Address hook_func) {
    uint32_t *stub = Ptr<uint32_t>(target_addr).get(emuenv.mem);

    // Determine what the "original function" address is
    Address original_func;
    if (is_svc_stub(stub)) {
        // HLE: the SVC stub IS the function entry. We copy it as trampoline.
        original_func = 0; // Will be set to trampoline_addr below
    } else {
        // LLE: decode the jump target from MOVW+MOVT
        original_func = decode_lle_stub_target(stub);
    }

    // Allocate trampoline: copy of original stub (12 bytes)
    Address trampoline_addr = alloc(emuenv.mem, 12, "taihen_trampoline");
    if (!trampoline_addr) {
        LOG_ERROR("taiHEN hook: failed to allocate trampoline");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    uint32_t *trampoline = Ptr<uint32_t>(trampoline_addr).get(emuenv.mem);
    trampoline[0] = stub[0];
    trampoline[1] = stub[1];
    trampoline[2] = stub[2];

    // For SVC stubs, the trampoline IS the original function
    if (is_svc_stub(stub)) {
        original_func = trampoline_addr;
    }

    // Allocate _tai_hook_user in emulated memory (12 bytes)
    Address hook_user_addr = alloc(emuenv.mem, sizeof(TaiHookUser), "taihen_hook_user");
    if (!hook_user_addr) {
        free(emuenv.mem, trampoline_addr);
        LOG_ERROR("taiHEN hook: failed to allocate hook_user");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    auto *hook_user = Ptr<TaiHookUser>(hook_user_addr).get(emuenv.mem);
    hook_user->next = 0; // No chain (single hook)
    hook_user->func = hook_func; // The hook function
    hook_user->old = original_func; // Original function / trampoline

    // Patch the target to jump to hook_func
    stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)hook_func, 12);
    stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(hook_func >> 16), 12);
    stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);

    // Invalidate JIT cache
    emuenv.kernel.invalidate_jit_cache(target_addr, 12);
    emuenv.kernel.invalidate_jit_cache(trampoline_addr, 12);

    // Store hook info for release
    auto hook = std::make_unique<TaiHook>();
    hook->uid = state->alloc_uid();
    hook->pid = thread_id;
    hook->type = TAI_HOOK_IMPORT;
    hook->target_addr = target_addr;
    hook->saved_bytes.assign(reinterpret_cast<uint8_t *>(trampoline),
        reinterpret_cast<uint8_t *>(trampoline) + 12);
    hook->hook_func = hook_func;
    hook->trampoline = trampoline_addr;
    hook->hook_user_addr = hook_user_addr;
    hook->func_nid = 0;
    hook->patch_size = 12;
    hook->is_thumb = false;
    const SceUID new_uid = hook->uid;
    state->hooks.emplace(new_uid, std::move(hook));

    return new_uid;
}

// Write ARM MOVW/MOVT/BX R12 (12 bytes) for import stub patching
static void write_arm_stub(uint32_t *dest, Address target) {
    dest[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)target, 12);
    dest[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(target >> 16), 12);
    dest[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
}

// Create an inline hook using substitute's transform-dis for instruction relocation.
// func_addr must include Thumb bit if target is Thumb.
// func_nid is optional (0 = none), used for export_nids management on release.
// Returns hook UID on success, TAI_ERROR_HOOK_ERROR if function can't be hooked.
static SceUID create_inline_hook(EmuEnvState &emuenv, TaihenState *state, SceUID thread_id,
    Address func_addr, Address hook_func, uint32_t func_nid = 0) {
    bool is_thumb = (func_addr & 1) != 0;
    Address code_addr = func_addr & ~1u;
    const void *code_host = Ptr<const void>(code_addr).get(emuenv.mem);

    // Set up arch context
    struct arch_dis_ctx arch;
    arch_dis_ctx_init(&arch);
    if (is_thumb)
        arch.pc_low_bit = true;

    // Determine jump patch size (substitute uses LDR PC, [PC, #-4] + literal = 8 or 10 bytes)
    int patch_size = jump_patch_size(code_addr, hook_func, arch, false);
    if (patch_size == -1) {
        LOG_ERROR("taiHEN inline hook: jump_patch_size failed at {}", log_hex(code_addr));
        return TAI_ERROR_HOOK_ERROR;
    }

    // Allocate trampoline (generous: rewritten prologue may be larger + final jump)
    int trampoline_alloc = TD_MAX_REWRITTEN_SIZE + MAX_JUMP_PATCH_SIZE + 16;
    Address trampoline_addr = alloc(emuenv.mem, trampoline_alloc, "taihen_trampoline");
    if (!trampoline_addr) {
        LOG_ERROR("taiHEN inline hook: failed to allocate trampoline");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    void *trampoline_host = Ptr<void>(trampoline_addr).get(emuenv.mem);
    void *rewritten_ptr = trampoline_host;

    uint_tptr pc_patch_start = code_addr;
    uint_tptr pc_patch_end = code_addr + patch_size;
    int offset_by_pcdiff[MAX_EXTENDED_PATCH_SIZE + 1];

    // Transform prologue: disassemble, relocate PC-relative instructions, write to trampoline
    int ret = transform_dis_main(code_host, &rewritten_ptr, pc_patch_start,
        &pc_patch_end, trampoline_addr, &arch, offset_by_pcdiff, 0);
    if (ret != SUBSTITUTE_OK) {
        LOG_DEBUG("taiHEN inline hook: transform_dis_main failed ({}) at {}", ret, log_hex(code_addr));
        free(emuenv.mem, trampoline_addr);
        return TAI_ERROR_HOOK_ERROR;
    }

    int copy_size = pc_patch_end - pc_patch_start;

    // Append jump back to rest of original function
    uint_tptr dpc = is_thumb ? (pc_patch_end | 1) : pc_patch_end;
    uint_tptr branch_pc = trampoline_addr + ((uint8_t *)rewritten_ptr - (uint8_t *)trampoline_host);
    make_jump_patch(&rewritten_ptr, branch_pc, dpc, arch);

    int trampoline_total = (uint8_t *)rewritten_ptr - (uint8_t *)trampoline_host;

    // For TAI_CONTINUE: trampoline address (with Thumb bit if needed)
    Address trampoline_entry = is_thumb ? (trampoline_addr | 1) : trampoline_addr;

    // Allocate hook_user
    Address hook_user_addr = alloc(emuenv.mem, sizeof(TaiHookUser), "taihen_hook_user");
    if (!hook_user_addr) {
        free(emuenv.mem, trampoline_addr);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    auto *hook_user = Ptr<TaiHookUser>(hook_user_addr).get(emuenv.mem);
    hook_user->next = 0;
    hook_user->func = hook_func;
    hook_user->old = trampoline_entry;

    // Save original bytes for release
    const uint8_t *code = Ptr<const uint8_t>(code_addr).get(emuenv.mem);
    std::vector<uint8_t> saved(code, code + copy_size);

    // Generate jump patch to hook_func
    // Note: MAX_JUMP_PATCH_SIZE=8 but Thumb unaligned needs 10 (NOP+LDR.W+literal)
    uint8_t jump_buf[MAX_JUMP_PATCH_SIZE + 4];
    void *jp = jump_buf;
    struct arch_dis_ctx jp_arch;
    arch_dis_ctx_init(&jp_arch);
    if (is_thumb)
        jp_arch.pc_low_bit = true;
    make_jump_patch(&jp, code_addr, hook_func, jp_arch);
    int jp_size = (uint8_t *)jp - jump_buf;

    // Write jump patch to function start
    uint8_t *target = Ptr<uint8_t>(code_addr).get(emuenv.mem);
    memcpy(target, jump_buf, jp_size);
    // NOP remaining bytes in patch region
    if (is_thumb) {
        for (int i = jp_size; i < copy_size; i += 2) {
            target[i] = 0x00;
            target[i + 1] = 0xBF; // Thumb NOP = 0xBF00
        }
    }

    emuenv.kernel.invalidate_jit_cache(code_addr, copy_size);
    emuenv.kernel.invalidate_jit_cache(trampoline_addr, trampoline_total);

    auto hook = std::make_unique<TaiHook>();
    hook->uid = state->alloc_uid();
    hook->pid = thread_id;
    hook->type = TAI_HOOK_INLINE;
    hook->target_addr = code_addr;
    hook->saved_bytes = std::move(saved);
    hook->hook_func = hook_func;
    hook->trampoline = trampoline_addr;
    hook->hook_user_addr = hook_user_addr;
    hook->func_nid = func_nid;
    hook->patch_size = copy_size;
    hook->is_thumb = is_thumb;
    const SceUID new_uid = hook->uid;
    state->hooks.emplace(new_uid, std::move(hook));

    LOG_DEBUG("taiHEN inline hook: {} bytes patched at {} ({})",
        copy_size, log_hex(code_addr), is_thumb ? "Thumb" : "ARM");
    return new_uid;
}

// Create an EXPORT hook using inline hooking (patch function prologue).
// Falls back to stub patching if prologue contains PC-relative instructions.
static SceUID create_export_hook(EmuEnvState &emuenv, TaihenState *state, SceUID thread_id,
    uint32_t func_nid, Address original_addr, Address hook_func) {
    // Try inline hook first
    SceUID uid = create_inline_hook(emuenv, state, thread_id, original_addr, hook_func, func_nid);
    if (uid >= 0) {
        // Update export_nids to point to hook
        emuenv.kernel.export_nids[func_nid] = hook_func;
        return uid;
    }

    // Fallback: patch all import stubs that point to the original function
    LOG_DEBUG("taiHEN export hook: PC-relative in prologue, falling back to stub patching");

    // Allocate trampoline: MOVW/MOVT/BX → original_addr (ARM stub)
    Address trampoline_addr = alloc(emuenv.mem, 12, "taihen_trampoline");
    if (!trampoline_addr) {
        LOG_ERROR("taiHEN export hook: failed to allocate trampoline");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    uint32_t *trampoline = Ptr<uint32_t>(trampoline_addr).get(emuenv.mem);
    write_arm_stub(trampoline, original_addr);

    Address hook_user_addr = alloc(emuenv.mem, sizeof(TaiHookUser), "taihen_hook_user");
    if (!hook_user_addr) {
        free(emuenv.mem, trampoline_addr);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    auto *hook_user = Ptr<TaiHookUser>(hook_user_addr).get(emuenv.mem);
    hook_user->next = 0;
    hook_user->func = hook_func;
    hook_user->old = trampoline_addr;

    // Update export_nids
    emuenv.kernel.export_nids[func_nid] = hook_func;

    // Patch all resolved import stubs for this NID
    int patched_count = 0;
    auto range = emuenv.kernel.func_binding_infos.equal_range(func_nid);
    for (auto it = range.first; it != range.second; ++it) {
        Address stub_addr = it->second;
        uint32_t *stub = Ptr<uint32_t>(stub_addr).get(emuenv.mem);
        if (!is_svc_stub(stub)) {
            Address current = decode_lle_stub_target(stub);
            if (current == original_addr) {
                write_arm_stub(stub, hook_func);
                emuenv.kernel.invalidate_jit_cache(stub_addr, 12);
                patched_count++;
            }
        }
    }

    emuenv.kernel.invalidate_jit_cache(trampoline_addr, 12);

    auto hook = std::make_unique<TaiHook>();
    hook->uid = state->alloc_uid();
    hook->pid = thread_id;
    hook->type = TAI_HOOK_IMPORT; // Fallback uses import-style patching
    hook->target_addr = 0; // No single target
    hook->saved_bytes.clear();
    hook->hook_func = hook_func;
    hook->trampoline = trampoline_addr;
    hook->hook_user_addr = hook_user_addr;
    hook->func_nid = func_nid;
    hook->patch_size = 0;
    hook->is_thumb = false;
    const SceUID new_uid = hook->uid;
    state->hooks.emplace(new_uid, std::move(hook));

    LOG_DEBUG("taiHEN export hook fallback: patched {} import stubs for NID {}",
        patched_count, log_hex(func_nid));
    return new_uid;
}

// ==================== taihen library (user exports) ====================

EXPORT(SceUID, taiHookFunctionExportForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    TRACY_FUNC(taiHookFunctionExportForUser, p_hook, args);
    if (!p_hook || !args)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    auto *hargs = reinterpret_cast<TaiHookArgs *>(args.get(emuenv.mem));
    const char *module_name = Ptr<const char>(hargs->module_name).get(emuenv.mem);
    uint32_t func_nid = hargs->func_nid;
    Address hook_func = hargs->hook_func;

    // Find the export address for this NID
    Address export_addr = 0;
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        auto it = emuenv.kernel.export_nids.find(func_nid);
        if (it != emuenv.kernel.export_nids.end()) {
            export_addr = it->second;
        }
    }

    if (!export_addr) {
        LOG_WARN("taiHookFunctionExportForUser: NID {} not found in exports (module '{}')", log_hex(func_nid), module_name);
        return TAI_ERROR_NOT_FOUND;
    }

    const std::lock_guard<std::mutex> lock(state->mutex);

    // Export hooks: patch all import stubs for this NID + update export_nids
    // Do NOT patch the function code (may be Thumb)
    SceUID uid;
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        uid = create_export_hook(emuenv, state, thread_id, func_nid, export_addr, hook_func);
    }
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionExportForUser: hooked NID {} (module '{}') at {} → {}, uid=0x{:X}",
        log_hex(func_nid), module_name, log_hex(export_addr), log_hex(hook_func), uid);
    return uid;
}

EXPORT(SceUID, taiHookFunctionImportForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    TRACY_FUNC(taiHookFunctionImportForUser, p_hook, args);
    if (!p_hook || !args)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    auto *hargs = reinterpret_cast<TaiHookArgs *>(args.get(emuenv.mem));
    const char *module_name = Ptr<const char>(hargs->module_name).get(emuenv.mem);
    uint32_t func_nid = hargs->func_nid;
    Address hook_func = hargs->hook_func;

    // Find the import stub for this NID in the specified module
    Address stub_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        stub_addr = find_import_stub_in_module(emuenv, module_name, func_nid);
    }

    if (!stub_addr) {
        LOG_WARN("taiHookFunctionImportForUser: no stub for NID {} in module '{}'", log_hex(func_nid), module_name);
        return TAI_ERROR_NOT_FOUND;
    }

    const std::lock_guard<std::mutex> lock(state->mutex);

    SceUID uid = create_hook(emuenv, state, thread_id, stub_addr, hook_func);
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionImportForUser: hooked NID {} in '{}' stub={} → {}, uid=0x{:X}",
        log_hex(func_nid), module_name, log_hex(stub_addr), log_hex(hook_func), uid);
    return uid;
}

EXPORT(SceUID, taiHookFunctionOffsetForUser, Ptr<uint32_t> p_hook, Ptr<void> args) {
    TRACY_FUNC(taiHookFunctionOffsetForUser, p_hook, args);
    if (!p_hook || !args)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    auto *oargs = reinterpret_cast<TaiOffsetArgs *>(args.get(emuenv.mem));
    SceUID modid = oargs->modid;
    int segidx = oargs->segidx;
    uint32_t offset = oargs->offset;
    int thumb = oargs->thumb;
    Address hook_func = oargs->hook_func;

    // Find the target address from module + segment + offset
    Address target_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
        if (mod_it == emuenv.kernel.loaded_modules.end()) {
            LOG_WARN("taiHookFunctionOffsetForUser: module {} not found", modid);
            return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
        }

        const auto &mod = mod_it->second->info;
        if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0) {
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }

        target_addr = mod.segments[segidx].vaddr.address() + offset;
    }
    // Add Thumb bit if the thumb flag is set
    if (thumb)
        target_addr |= 1;

    const std::lock_guard<std::mutex> lock(state->mutex);

    // Offset hooks use inline hooking (patch function prologue)
    // The offset may include the Thumb bit (bit 0)
    SceUID uid = create_inline_hook(emuenv, state, thread_id, target_addr, hook_func);
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionOffsetForUser: hooked mod={} seg={} off={} at {} → {}, uid=0x{:X}",
        modid, segidx, log_hex(offset), log_hex(target_addr), log_hex(hook_func), uid);
    return uid;
}

EXPORT(int, taiGetModuleInfo, const char *module, Ptr<void> info) {
    TRACY_FUNC(taiGetModuleInfo, module, info);
    if (!module || !info) {
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    }

    auto info_ptr = info.get(emuenv.mem);
    uint32_t user_size = *reinterpret_cast<uint32_t *>(info_ptr);
    if (user_size < 0x34) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    }

    auto fill_info = [&](SceUID uid, const char *name, uint32_t module_nid, Address seg0_addr, uint32_t seg0_size) {
        auto out = reinterpret_cast<uint8_t *>(info_ptr);
        *reinterpret_cast<uint32_t *>(out + 4) = uid; // modid
        *reinterpret_cast<uint32_t *>(out + 8) = module_nid;
        std::strncpy(reinterpret_cast<char *>(out + 12), name, 27);
        *reinterpret_cast<uint32_t *>(out + 0x28) = seg0_addr;
        *reinterpret_cast<uint32_t *>(out + 0x2C) = seg0_addr + seg0_size;
    };

    // Search LLE loaded modules first
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto &loaded_modules = emuenv.kernel.loaded_modules;
        for (const auto &[uid, mod] : loaded_modules) {
            if (std::string(mod->info.module_name) == module) {
                // Find module_nid via reverse lookup in module_uid_by_nid
                uint32_t mod_nid = 0;
                {
                    const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
                    for (const auto &[nid, mapped_uid] : emuenv.kernel.module_uid_by_nid) {
                        if (mapped_uid == uid) {
                            mod_nid = nid;
                            break;
                        }
                    }
                }
                fill_info(uid, mod->info.module_name, mod_nid,
                    mod->info.segments[0].vaddr.address(),
                    mod->info.segments[0].memsz);
                LOG_DEBUG("taiGetModuleInfo: found LLE module '{}' uid={} nid=0x{:08X}", module, uid, mod_nid);
                return 0;
            }
        }
    }

    // For HLE modules (SceSysmem etc), return a synthetic entry.
    // Plugins may look these up to patch them, but in the emulator HLE modules
    // have no ARM code. We provide a valid UID so callers don't get garbage.
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (state) {
        const std::lock_guard<std::mutex> lock(state->mutex);
        SceUID synthetic_uid = state->alloc_uid();
        fill_info(synthetic_uid, module, 0, 0, 0);
        LOG_WARN("taiGetModuleInfo: '{}' is HLE, returning synthetic uid={} (patches won't work)", module, synthetic_uid);
        return 0;
    }

    LOG_WARN("taiGetModuleInfo: module '{}' not found", module);
    return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
}

EXPORT(int, taiHookRelease, SceUID tai_uid, uint32_t hook_ref) {
    TRACY_FUNC(taiHookRelease, tai_uid, hook_ref);
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    const std::lock_guard<std::mutex> lock(state->mutex);

    auto it = state->hooks.find(tai_uid);
    if (it == state->hooks.end()) {
        LOG_WARN("taiHookRelease: hook uid=0x{:X} not found", tai_uid);
        return SCE_KERNEL_ERROR_INVALID_UID;
    }

    TaiHook *hook = it->second.get();

    if (hook->type == TAI_HOOK_INLINE) {
        // Inline hook: restore original prologue bytes
        uint8_t *target = Ptr<uint8_t>(hook->target_addr).get(emuenv.mem);
        memcpy(target, hook->saved_bytes.data(), hook->saved_bytes.size());
        emuenv.kernel.invalidate_jit_cache(hook->target_addr, hook->patch_size);

        // Restore export_nids if we have a NID
        if (hook->func_nid != 0) {
            const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
            Address original_addr = hook->target_addr | (hook->is_thumb ? 1 : 0);
            emuenv.kernel.export_nids[hook->func_nid] = original_addr;
        }
    } else if (hook->target_addr != 0) {
        // Import hook: restore original bytes at the patched stub
        uint8_t *target = Ptr<uint8_t>(hook->target_addr).get(emuenv.mem);
        memcpy(target, hook->saved_bytes.data(), hook->saved_bytes.size());
        emuenv.kernel.invalidate_jit_cache(hook->target_addr, hook->saved_bytes.size());
    } else {
        // Fallback export hook (stub patching): restore export_nids + import stubs
        if (hook->func_nid != 0) {
            const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
            // Find the original address from the trampoline (it branches there)
            uint32_t *tramp = Ptr<uint32_t>(hook->trampoline).get(emuenv.mem);
            Address original_addr = decode_lle_stub_target(tramp);
            emuenv.kernel.export_nids[hook->func_nid] = original_addr;

            // Re-patch import stubs back to original
            auto range = emuenv.kernel.func_binding_infos.equal_range(hook->func_nid);
            for (auto bi = range.first; bi != range.second; ++bi) {
                uint32_t *stub = Ptr<uint32_t>(bi->second).get(emuenv.mem);
                if (!is_svc_stub(stub)) {
                    Address current = decode_lle_stub_target(stub);
                    if (current == hook->hook_func) {
                        write_arm_stub(stub, original_addr);
                        emuenv.kernel.invalidate_jit_cache(bi->second, 12);
                    }
                }
            }
        }
    }

    // Free emulated memory
    free(emuenv.mem, hook->trampoline);
    free(emuenv.mem, hook->hook_user_addr);

    state->hooks.erase(it);

    LOG_DEBUG("taiHookRelease: released hook uid=0x{:X}", tai_uid);
    return 0;
}

EXPORT(SceUID, taiInjectAbs, Ptr<void> dest, Ptr<const void> src, uint32_t size) {
    TRACY_FUNC(taiInjectAbs, dest, src, size);
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

// Layout of tai_offset_args_t for taiInjectData in emulated memory
// Must match: size(4) modid(4) segidx(4) offset(4) thumb(4) source(4) source_size(4) = 28 bytes
struct TaiInjectArgs {
    uint32_t size;
    SceUID modid;
    int segidx;
    uint32_t offset;
    int thumb; // unused for inject, but present in struct
    Address source; // Ptr in guest memory
    uint32_t source_size;
};

EXPORT(SceUID, taiInjectDataForUser, Ptr<void> args) {
    TRACY_FUNC(taiInjectDataForUser, args);
    if (!args)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *iargs = reinterpret_cast<TaiInjectArgs *>(args.get(emuenv.mem));
    SceUID modid = iargs->modid;
    int segidx = iargs->segidx;
    uint32_t offset = iargs->offset;
    uint32_t size = iargs->source_size;
    Address source_addr = iargs->source;

    if (!source_addr || size == 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    // Resolve module + segment + offset to address
    Address dest_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
        if (mod_it == emuenv.kernel.loaded_modules.end()) {
            LOG_WARN("taiInjectDataForUser: module {} not found", modid);
            return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
        }

        const auto &mod = mod_it->second->info;
        if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0) {
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }

        dest_addr = mod.segments[segidx].vaddr.address() + offset;
    }
    Ptr<void> dest(dest_addr);

    // Delegate to taiInjectAbs logic
    return export_taiInjectAbs(emuenv, thread_id, export_name, dest, Ptr<const void>(source_addr), size);
}

EXPORT(int, taiInjectRelease, SceUID tai_uid) {
    TRACY_FUNC(taiInjectRelease, tai_uid);
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
    TRACY_FUNC(taiGetModuleExportFunc, modname, libnid, funcnid, func);
    if (!func)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    // Search runtime export_nids
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        auto it = emuenv.kernel.export_nids.find(funcnid);
        if (it != emuenv.kernel.export_nids.end()) {
            *func.get(emuenv.mem) = it->second;
            return 0;
        }
    }

    LOG_WARN("taiGetModuleExportFunc: NID {} not found (module '{}')", log_hex(funcnid), modname ? modname : "<null>");
    return TAI_ERROR_NOT_FOUND;
}

// ==================== taihenUnsafe library ====================

EXPORT(SceUID, taiLoadKernelModule, const char *path, int flags, Ptr<void> opt) {
    TRACY_FUNC(taiLoadKernelModule, path, flags, opt);
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadKernelModule: path={}", path);
    return load_module(emuenv, path);
}

EXPORT(int, taiStartKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    TRACY_FUNC(taiStartKernelModuleForUser, modid, args, opt, res);
    STUBBED("taiStartKernelModuleForUser");
    return 0;
}

EXPORT(SceUID, taiLoadStartKernelModuleForUser, const char *path, Ptr<void> args) {
    TRACY_FUNC(taiLoadStartKernelModuleForUser, path, args);
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadStartKernelModuleForUser: path={}", path);
    SceUID uid = load_module(emuenv, path);
    if (uid < 0)
        return uid;

    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
        if (mod_it != emuenv.kernel.loaded_modules.end()) {
            start_module(emuenv, mod_it->second->info);
        }
    }
    return uid;
}

EXPORT(SceUID, taiLoadStartModuleForPidForUser, const char *path, Ptr<void> args) {
    TRACY_FUNC(taiLoadStartModuleForPidForUser, path, args);
    if (!path)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    LOG_DEBUG("taiLoadStartModuleForPidForUser: path={}", path);
    SceUID uid = load_module(emuenv, path);
    if (uid < 0)
        return uid;

    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(uid);
        if (mod_it != emuenv.kernel.loaded_modules.end()) {
            start_module(emuenv, mod_it->second->info);
        }
    }
    return uid;
}

EXPORT(int, taiStopKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    TRACY_FUNC(taiStopKernelModuleForUser, modid, args, opt, res);
    STUBBED("taiStopKernelModuleForUser");
    return 0;
}

EXPORT(int, taiUnloadKernelModule, SceUID modid, int flags, Ptr<void> opt) {
    TRACY_FUNC(taiUnloadKernelModule, modid, flags, opt);
    STUBBED("taiUnloadKernelModule");
    return 0;
}

EXPORT(int, taiStopUnloadKernelModuleForUser, SceUID modid, Ptr<void> args, Ptr<void> opt, Ptr<int> res) {
    TRACY_FUNC(taiStopUnloadKernelModuleForUser, modid, args, opt, res);
    STUBBED("taiStopUnloadKernelModuleForUser");
    return 0;
}

EXPORT(int, taiMemcpyUserToKernel, Ptr<void> kernel_dst, Ptr<const void> user_src, uint32_t len) {
    TRACY_FUNC(taiMemcpyUserToKernel, kernel_dst, user_src, len);
    if (!kernel_dst || !user_src)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    // In emulator, user/kernel address space is the same
    std::memcpy(kernel_dst.get(emuenv.mem), user_src.get(emuenv.mem), len);
    return 0;
}

EXPORT(int, taiMemcpyKernelToUser, Ptr<void> user_dst, Ptr<const void> kernel_src, uint32_t len) {
    TRACY_FUNC(taiMemcpyKernelToUser, user_dst, kernel_src, len);
    if (!user_dst || !kernel_src)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    std::memcpy(user_dst.get(emuenv.mem), kernel_src.get(emuenv.mem), len);
    return 0;
}

EXPORT(int, taiReloadConfig) {
    TRACY_FUNC(taiReloadConfig);
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
    TRACY_FUNC(taiHookFunctionExportForKernel, pid, p_hook);
    if (!p_hook || !module)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    // For kernel hooks, we receive arguments directly (not via args struct)
    // Find the export and hook it
    Address export_addr = 0;
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        auto it = emuenv.kernel.export_nids.find(func_nid);
        if (it != emuenv.kernel.export_nids.end()) {
            export_addr = it->second;
        }
    }

    if (!export_addr) {
        LOG_WARN("taiHookFunctionExportForKernel: NID {} not found (module '{}')", log_hex(func_nid), module);
        return TAI_ERROR_NOT_FOUND;
    }

    const std::lock_guard<std::mutex> lock(state->mutex);
    SceUID uid;
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        uid = create_export_hook(emuenv, state, thread_id, func_nid, export_addr, hook_func.address());
    }
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionExportForKernel: hooked NID {} (module '{}') → {}, uid=0x{:X}",
        log_hex(func_nid), module, log_hex(hook_func.address()), uid);
    return uid;
}

EXPORT(SceUID, taiHookFunctionImportForKernel, SceUID pid, Ptr<uint32_t> p_hook,
    const char *module, uint32_t import_library_nid, uint32_t import_func_nid, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionImportForKernel, pid, p_hook);
    if (!p_hook || !module)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    Address stub_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        stub_addr = find_import_stub_in_module(emuenv, module, import_func_nid);
    }

    if (!stub_addr) {
        LOG_WARN("taiHookFunctionImportForKernel: no stub for NID {} in '{}'", log_hex(import_func_nid), module);
        return TAI_ERROR_NOT_FOUND;
    }

    const std::lock_guard<std::mutex> lock(state->mutex);
    SceUID uid = create_hook(emuenv, state, thread_id, stub_addr, hook_func.address());
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionImportForKernel: hooked NID {} in '{}' → {}, uid=0x{:X}",
        log_hex(import_func_nid), module, log_hex(hook_func.address()), uid);
    return uid;
}

EXPORT(SceUID, taiHookFunctionOffsetForKernel, SceUID pid, Ptr<uint32_t> p_hook,
    SceUID modid, int segidx, uint32_t offset, int thumb, Ptr<const void> hook_func) {
    TRACY_FUNC(taiHookFunctionOffsetForKernel, pid, p_hook);
    if (!p_hook)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return SCE_KERNEL_ERROR_ERROR;

    Address target_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
        if (mod_it == emuenv.kernel.loaded_modules.end()) {
            LOG_WARN("taiHookFunctionOffsetForKernel: module {} not found", modid);
            return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
        }

        const auto &mod = mod_it->second->info;
        if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

        target_addr = mod.segments[segidx].vaddr.address() + offset;
    }
    target_addr &= ~1u; // Clear thumb bit

    const std::lock_guard<std::mutex> lock(state->mutex);
    SceUID uid = create_hook(emuenv, state, thread_id, target_addr, hook_func.address());
    if (uid < 0)
        return uid;

    *p_hook.get(emuenv.mem) = state->hooks[uid]->hook_user_addr;

    LOG_DEBUG("taiHookFunctionOffsetForKernel: hooked mod={} seg={} off={} → {}, uid=0x{:X}",
        modid, segidx, log_hex(offset), log_hex(hook_func.address()), uid);
    return uid;
}

EXPORT(int, taiGetModuleInfoForKernel, SceUID pid, const char *module, Ptr<void> info) {
    TRACY_FUNC(taiGetModuleInfoForKernel, pid, module, info);
    // Delegate to the user version — in emulator there's no pid separation
    return export_taiGetModuleInfo(emuenv, thread_id, export_name, module, info);
}

EXPORT(int, taiHookReleaseForKernel, SceUID tai_uid, uint32_t hook_ref) {
    TRACY_FUNC(taiHookReleaseForKernel, tai_uid, hook_ref);
    // Delegate to user version — no pid separation in emulator
    return export_taiHookRelease(emuenv, thread_id, export_name, tai_uid, hook_ref);
}

EXPORT(SceUID, taiInjectAbsForKernel, SceUID pid, Ptr<void> dest, Ptr<const void> src, uint32_t size) {
    TRACY_FUNC(taiInjectAbsForKernel, pid, dest, src, size);
    // Delegate — no pid separation in emulator
    return export_taiInjectAbs(emuenv, thread_id, export_name, dest, src, size);
}

EXPORT(SceUID, taiInjectDataForKernel, SceUID pid, SceUID modid, int segidx, uint32_t offset,
    Ptr<const void> data, uint32_t size) {
    TRACY_FUNC(taiInjectDataForKernel, pid, modid, segidx, offset);
    if (!data || size == 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    Address dest_addr;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
        if (mod_it == emuenv.kernel.loaded_modules.end()) {
            LOG_WARN("taiInjectDataForKernel: module {} not found", modid);
            return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
        }

        const auto &mod = mod_it->second->info;
        if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

        dest_addr = mod.segments[segidx].vaddr.address() + offset;
    }
    Ptr<void> dest(dest_addr);

    return export_taiInjectAbs(emuenv, thread_id, export_name, dest, Ptr<const void>(data.address()), size);
}

EXPORT(int, taiInjectReleaseForKernel, SceUID tai_uid) {
    TRACY_FUNC(taiInjectReleaseForKernel, tai_uid);
    return export_taiInjectRelease(emuenv, thread_id, export_name, tai_uid);
}

EXPORT(int, taiLoadPluginsForTitleForKernel, SceUID pid, const char *titleid, int flags) {
    TRACY_FUNC(taiLoadPluginsForTitleForKernel, pid, titleid, flags);
    STUBBED("taiLoadPluginsForTitleForKernel");
    return 0;
}

EXPORT(int, taiReloadConfigForKernel, int schedule, int load_kernel) {
    TRACY_FUNC(taiReloadConfigForKernel, schedule, load_kernel);
    return export_taiReloadConfig(emuenv, thread_id, export_name);
}

// ==================== taihenModuleUtils library ====================

EXPORT(int, module_get_by_name_nid, SceUID pid, const char *name, uint32_t nid, Ptr<uint32_t> info) {
    TRACY_FUNC(module_get_by_name_nid, pid, name, nid, info);
    STUBBED("module_get_by_name_nid");
    return 0;
}

EXPORT(int, module_get_offset, SceUID pid, SceUID modid, int segidx, uint32_t offset, Ptr<uint32_t> addr) {
    TRACY_FUNC(module_get_offset, pid, modid, segidx, offset, addr);
    if (!addr)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    Address result;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        const auto mod_it = emuenv.kernel.loaded_modules.find(modid);
        if (mod_it == emuenv.kernel.loaded_modules.end()) {
            LOG_DEBUG("module_get_offset: module {} not found (may be HLE)", modid);
            return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
        }

        const auto &mod = mod_it->second->info;
        if (segidx < 0 || segidx >= MODULE_INFO_NUM_SEGMENTS || mod.segments[segidx].memsz == 0) {
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }

        result = mod.segments[segidx].vaddr.address() + offset;
    }
    *addr.get(emuenv.mem) = result;

    return 0;
}

EXPORT(int, module_get_export_func, SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, Ptr<uint32_t> func) {
    TRACY_FUNC(module_get_export_func, pid, modname, libnid, funcnid, func);
    if (!func)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    // First, check runtime export_nids (LLE exports and previously created dynamic stubs)
    {
        const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
        auto it = emuenv.kernel.export_nids.find(funcnid);
        if (it != emuenv.kernel.export_nids.end()) {
            *func.get(emuenv.mem) = it->second;
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
        LOG_INFO("module_get_export_func: created dynamic SVC stub for NID {} ({})", log_hex(funcnid), modname ? modname : "?");
        return 0;
    }
}

EXPORT(int, module_get_import_func, SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, Ptr<uint32_t> func) {
    TRACY_FUNC(module_get_import_func, pid, modname, libnid, funcnid, func);
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
    if (!vfs::read_file(VitaIoDevice::ux0, buf, emuenv.vita_fs_path, "tai/config.txt")) {
        vfs::read_file(VitaIoDevice::ur0, buf, emuenv.vita_fs_path, "tai/config.txt");
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

// Register an HLE override for a kubridge export NID.
// Creates an SVC stub in ARM memory, overwrites export_nids, and re-patches
// any existing func_binding_infos so that callers dispatch to HLE C++ instead
// of kubridge's ARM code (which uses firmware-specific offsets and MMU ops).
static void register_hle_override(EmuEnvState &emuenv, uint32_t nid) {
    auto &kernel = emuenv.kernel;
    auto &mem = emuenv.mem;
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);

    // Allocate 12 bytes for an SVC stub: svc #0; mov pc, lr; NID
    Address stub_addr = alloc(mem, 12, "kubridge_hle_override");
    if (!stub_addr) {
        LOG_ERROR("register_hle_override: failed to allocate stub for NID {}", log_hex(nid));
        return;
    }

    uint32_t *stub = Ptr<uint32_t>(stub_addr).get(mem);
    stub[0] = 0xef000000; // svc #0
    stub[1] = 0xe1a0f00e; // mov pc, lr
    stub[2] = nid;

    // Overwrite (or insert) the export_nids entry
    kernel.export_nids[nid] = stub_addr;

    // Re-patch any existing import stubs that already point to kubridge's ARM code
    auto range = kernel.func_binding_infos.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        auto address = it->second;
        uint32_t *caller_stub = Ptr<uint32_t>(address).get(mem);
        caller_stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)stub_addr, 12);
        caller_stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(stub_addr >> 16), 12);
        caller_stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        kernel.invalidate_jit_cache(address, 3 * sizeof(uint32_t));
    }

    const char *name = import_name(nid);
    LOG_INFO("taiHEN: registered HLE override for NID {} ({}) at {}", log_hex(nid), name, log_hex(stub_addr));
}

// Apply HLE overrides for plugin exports that have HLE implementations.
// When a plugin (e.g. kubridge) exports functions that can't run as LLE
// (firmware-specific offsets, ARM MMU manipulation), we register them in
// nids.inc and this function dynamically overrides the LLE exports with
// SVC stubs that dispatch to our C++ implementations.
static void apply_plugin_hle_overrides(EmuEnvState &emuenv, const std::set<uint32_t> &pre_plugin_nids) {
    auto &kernel = emuenv.kernel;
    int count = 0;

    // Snapshot current export_nids to iterate (we'll modify it)
    std::vector<std::pair<uint32_t, Address>> exports_snapshot;
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        exports_snapshot.assign(kernel.export_nids.begin(), kernel.export_nids.end());
    }

    for (const auto &[nid, addr] : exports_snapshot) {
        if (addr == 0)
            continue;
        // Only override NIDs that were added by plugins (not pre-existing system exports)
        if (pre_plugin_nids.count(nid))
            continue;
        // Check if the exported NID's current address is an LLE (ARM) function
        // AND we have an HLE implementation for it
        uint32_t *stub = Ptr<uint32_t>(addr).get(emuenv.mem);
        if (is_svc_stub(stub)) {
            // Already an SVC stub (HLE), no need to override
            continue;
        }
        if (has_hle_implementation(nid)) {
            register_hle_override(emuenv, nid);
            count++;
        }
    }

    if (count > 0) {
        LOG_INFO("taiHEN: applied {} HLE overrides for plugin exports", count);
    }
}

void load_taihen_plugins_for_title(EmuEnvState &emuenv, const std::string &titleid) {
    auto *state = emuenv.kernel.obj_store.get<TaihenState>();
    if (!state)
        return;

    load_taihen_config(emuenv);

    // Load KERNEL plugins (once)
    // Note: we only load_module here. start_module is called by run_app()
    // which iterates all loaded_modules and starts them.
    if (!state->kernel_plugins_loaded) {
        // Snapshot NIDs before loading plugins so we only override plugin exports
        std::set<uint32_t> pre_plugin_nids;
        {
            const std::lock_guard<std::mutex> guard(emuenv.kernel.export_nids_mutex);
            for (const auto &[nid, addr] : emuenv.kernel.export_nids)
                pre_plugin_nids.insert(nid);
        }

        auto kernel_it = state->config.find("KERNEL");
        if (kernel_it != state->config.end()) {
            for (const auto &plugin_path : kernel_it->second) {
                LOG_INFO("taiHEN: loading kernel plugin: {}", plugin_path);
                SceUID uid = load_module(emuenv, plugin_path);
                if (uid < 0) {
                    LOG_ERROR("taiHEN: failed to load kernel plugin: {} (error {})", plugin_path, uid);
                }
            }
        }
        state->kernel_plugins_loaded = true;

        // Apply HLE overrides for plugin exports that have C++ implementations
        apply_plugin_hle_overrides(emuenv, pre_plugin_nids);
    }

    // Load title-specific plugins
    auto title_it = state->config.find(titleid);
    if (title_it != state->config.end()) {
        for (const auto &plugin_path : title_it->second) {
            LOG_INFO("taiHEN: loading plugin for {}: {}", titleid, plugin_path);
            SceUID uid = load_module(emuenv, plugin_path);
            if (uid < 0) {
                LOG_ERROR("taiHEN: failed to load plugin: {} (error {})", plugin_path, uid);
            }
        }
    }
}

// ==================== Library Init ====================

LIBRARY_INIT(taihen) {
    emuenv.kernel.obj_store.create<TaihenState>();
    LOG_INFO("taiHEN: HLE module initialized");
}
