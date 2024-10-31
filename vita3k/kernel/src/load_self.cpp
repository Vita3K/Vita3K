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

#include <cpu/functions.h>
#include <kernel/load_self.h>
#include <kernel/relocation.h>
#include <kernel/state.h>
#include <kernel/types.h>

#include <nids/functions.h>
#include <util/arm.h>
#include <util/fs.h>
#include <util/log.h>

#include <util/elf.h>
// clang-format off
#define SCE_ELF_DEFS_TARGET
#include <sce-elf-defs.h>
#undef SCE_ELF_DEFS_TARGET
// clang-format on
#include <miniz.h>
#include <self.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#define NID_MODULE_STOP 0x79F8E492
#define NID_MODULE_EXIT 0x913482A9
#define NID_MODULE_START 0x935CD196
#define NID_MODULE_INFO 0x6C2224BA
#define NID_SYSLYB 0x936c8a78
#define NID_PROCESS_PARAM 0x70FBA1E7

static constexpr bool LOG_MODULE_LOADING = false;

struct VarImportsHeader {
    uint32_t unk : 4; // Must be zero
    uint32_t reloc_data_size : 24; // Size of Relocation data in bytes, includes this header.
    uint32_t unk2 : 4; // Must be zero
};
static_assert(sizeof(VarImportsHeader) == sizeof(uint32_t));

static bool load_var_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, uint32_t module_id) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}). entry: {}, *entry: {}", log_hex(nid), name, entry, log_hex(*entry.get(mem)));
        }

        VarImportsHeader *const var_reloc_header = reinterpret_cast<VarImportsHeader *>(entry.get(mem));
        const auto var_reloc_entries = static_cast<void *>(var_reloc_header + 1);
        const uint32_t reloc_size = (var_reloc_header->reloc_data_size > sizeof(VarImportsHeader)) ? (var_reloc_header->reloc_data_size - sizeof(VarImportsHeader)) : 0;

        const char *const name = import_name(nid);
        Address export_address;
        kernel.var_binding_infos.emplace(nid, VarBindingInfo{ var_reloc_entries, reloc_size, module_id });
        const ExportNids::iterator export_address_it = kernel.export_nids.find(nid);
        if (export_address_it != kernel.export_nids.end()) {
            export_address = export_address_it->second;
        } else {
            constexpr auto STUB_SYMVAL = 0xDEADBEEF;
            LOG_DEBUG("\tNID NOT FOUND {} ({}) at {}, setting to stub value {}", log_hex(nid), name, log_hex(entry.address()), log_hex(STUB_SYMVAL));

            auto alloc_name = fmt::format("Stub var import reloc symval, NID {} ({})", log_hex(nid), name);
            auto stub_symval_ptr = Ptr<uint32_t>(alloc(mem, 4, alloc_name.c_str()));
            *stub_symval_ptr.get(mem) = STUB_SYMVAL;

            export_address = stub_symval_ptr.address();

            // Use same stub for other var imports
            kernel.export_nids.emplace(nid, export_address);
        }

        if (reloc_size)
            // 8 is sizeof(EntryFormat1Alt)
            if (!relocate(var_reloc_entries, reloc_size, segments, mem, true, export_address))
                return false;
    }

    return true;
}

static bool unload_var_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, uint32_t module_id) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        VarImportsHeader *const var_reloc_header = reinterpret_cast<VarImportsHeader *>(entry.get(mem));
        const auto var_reloc_entries = static_cast<void *>(var_reloc_header + 1);
        const uint32_t reloc_size = (var_reloc_header->reloc_data_size > sizeof(VarImportsHeader)) ? (var_reloc_header->reloc_data_size - sizeof(VarImportsHeader)) : 0;

        // remove the binding info from the map
        VarBindingInfo binding_info{ var_reloc_entries, reloc_size, module_id };
        auto range = kernel.var_binding_infos.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            if (memcmp(&it->second, &binding_info, sizeof(VarBindingInfo)) == 0) {
                kernel.var_binding_infos.erase(it);
                break;
            }
        }
    }

    return true;
}

static bool load_func_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, const SegmentInfosForReloc &segments, KernelState &kernel, const MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        const ExportNids::iterator export_address = kernel.export_nids.find(nid);
        uint32_t *const stub = entry.get(mem);

        kernel.func_binding_infos.emplace(nid, entry.address());
        if (export_address == kernel.export_nids.end()) {
            stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
            stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
            stub[2] = nid; // Our interrupt hook will read this.
        } else {
            Address func_address = export_address->second;
            stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)func_address, 12);
            stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(func_address >> 16), 12);
            stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        }
        if (stub[3]) { // if function's associated reftable exists
            VarImportsHeader *const var_reloc_header = Ptr<VarImportsHeader>(stub[3]).get(mem);
            const auto var_reloc_entries = static_cast<void *>(var_reloc_header + 1);
            const uint32_t reloc_size = (var_reloc_header->reloc_data_size > sizeof(VarImportsHeader)) ? (var_reloc_header->reloc_data_size - sizeof(VarImportsHeader)) : 0;
            if (reloc_size) {
                if (!relocate(var_reloc_entries, reloc_size, segments, mem, true, entry.address())) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool unload_func_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, const SegmentInfosForReloc &segments, KernelState &kernel, const MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        // remove the stub from the table
        auto range = kernel.func_binding_infos.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == entry.address()) {
                kernel.func_binding_infos.erase(it);
                break;
            }
        }
    }
    return true;
}

static bool load_imports(const sce_module_info_raw &module, Ptr<const void> segment_address, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, bool is_unload = false) {
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_imports_raw *const imports_begin = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_top);
    const sce_module_imports_raw *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_end);

    for (const sce_module_imports_raw *imports = imports_begin; imports < imports_end; imports = reinterpret_cast<const sce_module_imports_raw *>(reinterpret_cast<const uint8_t *>(imports) + imports->size)) {
        assert(imports->num_syms_tls_vars == 0);

        Address library_name{};
        Address func_nid_table{};
        Address func_entry_table{};
        Address var_nid_table{};
        Address var_entry_table{};

        if (imports->size == 0x24) {
            auto short_imports = reinterpret_cast<const sce_module_imports_short_raw *>(imports);
            library_name = short_imports->library_name;
            func_nid_table = short_imports->func_nid_table;
            func_entry_table = short_imports->func_entry_table;
            var_nid_table = short_imports->var_nid_table;
            var_entry_table = short_imports->var_entry_table;
        } else if (imports->size == 0x34) {
            auto long_imports = imports;
            library_name = long_imports->library_name;
            func_nid_table = long_imports->func_nid_table;
            func_entry_table = long_imports->func_entry_table;
            var_nid_table = long_imports->var_nid_table;
            var_entry_table = long_imports->var_entry_table;
        }

        std::string lib_name;
        if (kernel.debugger.log_imports) {
            lib_name = Ptr<const char>(library_name).get(mem);
            LOG_INFO("Loading func imports from {}", lib_name);
        }

        const uint32_t *const nids = Ptr<const uint32_t>(func_nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(func_entry_table).get(mem);

        const size_t num_syms_funcs = imports->num_syms_funcs;
        if (!is_unload && !load_func_imports(nids, entries, num_syms_funcs, segments, kernel, mem))
            return false;
        if (is_unload && !unload_func_imports(nids, entries, num_syms_funcs, segments, kernel, mem))
            return false;

        const uint32_t *const var_nids = Ptr<const uint32_t>(var_nid_table).get(mem);
        const Ptr<uint32_t> *const var_entries = Ptr<Ptr<uint32_t>>(var_entry_table).get(mem);

        const auto var_count = imports->num_syms_vars;

        if (kernel.debugger.log_imports && var_count > 0)
            LOG_INFO("Loading var imports from {}", lib_name);

        if (!is_unload && !load_var_imports(var_nids, var_entries, var_count, segments, kernel, mem, module.module_nid))
            return false;
        if (is_unload && !unload_var_imports(var_nids, var_entries, var_count, segments, kernel, mem, module.module_nid))
            return false;
    }

    return true;
}

static bool load_func_exports(SceKernelModuleInfo *kernel_module_info, const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (nid == NID_MODULE_START) {
            kernel_module_info->start_entry = entry;
            continue;
        }

        if (nid == NID_MODULE_STOP) {
            kernel_module_info->stop_entry = entry;
            continue;
        }
        if (nid == NID_MODULE_EXIT) {
            kernel_module_info->exit_entry = entry;
            continue;
        }

        kernel.export_nids.emplace(nid, entry.address());
        // substitute supervisor calls to direct function calls in loaded modules
        auto range = kernel.func_binding_infos.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            auto address = it->second;
            uint32_t *const stub = Ptr<uint32_t>(address).get(mem);
            stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)entry.address(), 12);
            stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(entry.address() >> 16), 12);
            stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
            kernel.invalidate_jit_cache(address, 3 * sizeof(uint32_t));
        }

        if (kernel.debugger.log_exports) {
            const char *const name = import_name(nid);

            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }
    }

    return true;
}

static bool unload_func_exports(SceKernelModuleInfo *kernel_module_info, const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];

        if (nid == NID_MODULE_START || nid == NID_MODULE_STOP || nid == NID_MODULE_EXIT)
            continue;

        kernel.export_nids.erase(nid);
        // invalidate all lle nid calls
        auto range = kernel.func_binding_infos.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            Address entry = it->second;
            uint32_t *stub = Ptr<uint32_t>(entry).get(mem);

            stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
            stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
            stub[2] = nid; // Our interrupt hook will read this.
            kernel.invalidate_jit_cache(entry, 3 * sizeof(uint32_t));
        }
    }

    return true;
}

static bool load_var_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (nid == NID_PROCESS_PARAM) {
            if (!kernel.process_param)
                kernel.load_process_param(mem, entry);
            LOG_DEBUG("\tNID {} (SCE_PROC_PARAMS) at {}", log_hex(nid), log_hex(entry.address()));
            continue;
        }

        if (nid == NID_MODULE_INFO) {
            LOG_DEBUG("\tNID {} (NID_MODULE_INFO) at {}", log_hex(nid), log_hex(entry.address()));
            continue;
        }

        if (nid == NID_SYSLYB) {
            LOG_DEBUG("\tNID {} (SYSLYB) at {}", log_hex(nid), log_hex(entry.address()));
            continue;
        }

        if (kernel.debugger.log_exports) {
            const char *const name = import_name(nid);

            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        Address old_entry_address = 0;
        auto nid_it = kernel.export_nids.find(nid);
        if (nid_it != kernel.export_nids.end()) {
            LOG_DEBUG("Found previously not found variable. nid:{}, new_entry_point:{}", log_hex(nid), log_hex(entry.address()));
            old_entry_address = kernel.export_nids[nid];
        }
        kernel.export_nids[nid] = entry.address();

        auto range = kernel.var_binding_infos.equal_range(nid);
        for (auto j = range.first; j != range.second; ++j) {
            auto &var_binding_info = j->second;
            if (var_binding_info.size == 0)
                continue;

            SegmentInfosForReloc seg;
            const auto &module_info = kernel.loaded_modules[kernel.module_uid_by_nid[var_binding_info.module_nid]];
            if (!module_info) {
                LOG_ERROR("Module not found by nid: {} uid: {}", log_hex(var_binding_info.module_nid), kernel.module_uid_by_nid[var_binding_info.module_nid]);
            } else {
                for (int k = 0; k < MODULE_INFO_NUM_SEGMENTS; k++) {
                    const auto &segment = module_info->info.segments[k];
                    if (segment.size > 0) {
                        seg[k] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
                    }
                }
            }

            // Note: We make the assumption that variables are not imported into executable code (wouldn't make a lot of sense)
            // If this is not the case, uncomment the following
            /* if (!seg.empty()) {
                for (const auto &[key, value] : seg) {
                    kernel.invalidate_jit_cache(value.addr, value.size);
                }
            }*/

            if (!seg.empty()) {
                if (!relocate(var_binding_info.entries, var_binding_info.size, seg, mem, true, entry.address())) {
                    LOG_ERROR("Failed to relocate late binding info");
                }
            }
        }
        if (old_entry_address)
            free(mem, old_entry_address);
    }
    return true;
}

static bool unload_var_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];

        if (nid == NID_PROCESS_PARAM || nid == NID_MODULE_INFO || nid == NID_SYSLYB)
            continue;

        // replace again the nid by a stub
        constexpr auto STUB_SYMVAL = 0xDEADBEEF;

        auto alloc_name = fmt::format("Stub var import reloc symval, NID {} ({})", log_hex(nid), import_name(nid));
        auto stub_symval_ptr = Ptr<uint32_t>(alloc(mem, 4, alloc_name.c_str()));
        *stub_symval_ptr.get(mem) = STUB_SYMVAL;

        const Ptr<uint32_t> entry = stub_symval_ptr;

        // Use same stub for other var imports
        kernel.export_nids[nid] = entry.address();

        auto range = kernel.var_binding_infos.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            auto &var_binding_info = it->second;
            if (var_binding_info.size == 0)
                continue;

            SegmentInfosForReloc seg;
            const auto &module_info = kernel.loaded_modules[kernel.module_uid_by_nid[var_binding_info.module_nid]];
            if (!module_info) {
                LOG_ERROR("Module not found by nid: {} uid: {}", log_hex(var_binding_info.module_nid), kernel.module_uid_by_nid[var_binding_info.module_nid]);
            } else {
                for (int k = 0; k < MODULE_INFO_NUM_SEGMENTS; k++) {
                    const auto &segment = module_info->info.segments[k];
                    if (segment.size > 0) {
                        seg[k] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
                    }
                }
            }

            if (!seg.empty()) {
                if (!relocate(var_binding_info.entries, var_binding_info.size, seg, mem, true, entry.address())) {
                    LOG_ERROR("Failed to relocate late binding info");
                }
            }
        }
    }
    return true;
}

static bool load_exports(SceKernelModuleInfo *kernel_module_info, const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, MemState &mem, bool is_unload = false) {
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_exports_raw *const exports_begin = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_top);
    const sce_module_exports_raw *const exports_end = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_end);

    for (const sce_module_exports_raw *exports = exports_begin; exports < exports_end; exports = reinterpret_cast<const sce_module_exports_raw *>(reinterpret_cast<const uint8_t *>(exports) + exports->size)) {
        const char *const lib_name = Ptr<const char>(exports->library_name).get(mem);

        if (kernel.debugger.log_exports)
            LOG_INFO("Loading func exports from {}", lib_name ? lib_name : "unknown");

        const uint32_t *const nids = Ptr<const uint32_t>(exports->nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(exports->entry_table).get(mem);
        if (!is_unload && !load_func_exports(kernel_module_info, nids, entries, exports->num_syms_funcs, kernel, mem))
            return false;
        if (is_unload && !unload_func_exports(kernel_module_info, nids, entries, exports->num_syms_funcs, kernel, mem))
            return false;

        const auto var_count = exports->num_syms_vars;

        if (kernel.debugger.log_exports && var_count > 0) {
            LOG_INFO("Loading var exports from {}", lib_name ? lib_name : "unknown");
        }

        if (!is_unload && !load_var_exports(&nids[exports->num_syms_funcs], &entries[exports->num_syms_funcs], var_count, kernel, mem))
            return false;
        if (is_unload && !unload_var_exports(&nids[exports->num_syms_funcs], &entries[exports->num_syms_funcs], var_count, kernel, mem))
            return false;
    }

    return true;
}

/**
 * \return Negative on failure
 */
SceUID load_self(KernelState &kernel, MemState &mem, const void *self, const std::string &self_path, const fs::path &log_path, const std::vector<Patch> &patches) {
    // TODO: use raw I/O from path when io becomes less bad
    const uint8_t *const self_bytes = static_cast<const uint8_t *>(self);
    const SCE_header &self_header = *static_cast<const SCE_header *>(self);

    // assumes little endian host
    if (self_header.version != 3) {
        LOG_CRITICAL("SELF {} version {} is not supported.", self_path, self_header.version);
        return -1;
    }

    if (self_header.header_type != 1) {
        LOG_CRITICAL("SELF {} header type {} is not supported.", self_path, self_header.header_type);
        return -1;
    }

    if (self_path == "app0:sce_module/steroid.suprx") {
        LOG_CRITICAL("You're trying to load a vitamin dump. It is not supported.");
        return -1;
    }

    const uint8_t *const elf_bytes = self_bytes + self_header.elf_offset;
    const Elf32_Ehdr &elf = *reinterpret_cast<const Elf32_Ehdr *>(elf_bytes);
    const uint32_t module_info_offset = elf.e_entry & 0x3fffffff;
    const Elf32_Phdr *const segments = reinterpret_cast<const Elf32_Phdr *>(self_bytes + self_header.phdr_offset);

    // Verify ELF header is correct
    if (!EHDR_HAS_VALID_MAGIC(elf)) {
        LOG_CRITICAL("Cannot load file {}: invalid ELF magic.", self_path);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    if (elf.e_ident[EI_CLASS] != ELFCLASS32) {
        LOG_CRITICAL("Cannot load ELF {}: unexpected EI_CLASS {}.", self_path, elf.e_ident[EI_CLASS]);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    if (elf.e_ident[EI_DATA] != ELFDATA2LSB) {
        LOG_CRITICAL("Cannot load ELF {}: unexpected EI_DATA {}.", self_path, elf.e_ident[EI_DATA]);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    if (elf.e_ident[EI_VERSION] != EV_CURRENT) {
        LOG_CRITICAL("Cannot load ELF {}: invalid EI_VERSION {}.", self_path, elf.e_ident[EI_VERSION]);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    if (elf.e_machine != EM_ARM) {
        LOG_CRITICAL("Cannot load ELF {}: unexpected e_machine {}.", self_path, elf.e_machine);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    bool isRelocatable;
    if (elf.e_type == ET_SCE_EXEC) {
        isRelocatable = false;
    } else if (elf.e_type == ET_SCE_RELEXEC) {
        isRelocatable = true;
    } else if (elf.e_type == ET_SCE_PSP2RELEXEC) {
        LOG_CRITICAL("Cannot load ELF {}: ET_SCE_PSP2RELEXEC is not supported.", self_path);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    } else {
        LOG_CRITICAL("Cannot load ELF {}: unexpected e_type {}.", self_path, elf.e_type);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }

    // TODO: is OSABI always 0?
    // TODO: is ABI_VERSION always 0?

    const segment_info *const seg_infos = reinterpret_cast<const segment_info *>(self_bytes + self_header.section_info_offset);

    LOG_DEBUG_IF(LOG_MODULE_LOADING, "Loading SELF at {}... (ELF type: {}, self_filesize: {}, self_offset: {}, module_info_offset: {})", self_path, log_hex(elf.e_type), log_hex(self_header.self_filesize), log_hex(self_header.self_offset), log_hex(module_info_offset));

    auto get_seg_header_string = [](uint32_t p_type) {
        if (p_type == PT_NULL) {
            return "NULL";
        } else if (p_type == PT_LOAD) {
            return "LOAD";
        } else if (p_type == PT_SCE_COMMENT) {
            return "SCE Comment";
        } else if (p_type == PT_SCE_VERSION) {
            return "SCE Version";
        } else if ((PT_LOOS <= p_type) && (p_type <= PT_HIOS)) {
            return "OS-specific";
        } else if ((PT_LOPROC <= p_type) && (p_type <= PT_HIPROC)) {
            return "Processor-specific";
        } else {
            return "Unknown";
        }
    };

    SegmentInfosForReloc segment_reloc_info;

    auto free_all_segments = [](MemState &mem, SegmentInfosForReloc &segs_info) {
        for (auto &[_, segment] : segs_info) {
            free(mem, segment.addr);
        }
    };

    for (Elf_Half seg_index = 0; seg_index < elf.e_phnum; ++seg_index) {
        const Elf32_Phdr &seg_header = segments[seg_index];
        const uint8_t *const seg_bytes = self_bytes + self_header.header_len + seg_header.p_offset;

        LOG_DEBUG_IF(LOG_MODULE_LOADING, "    [{}] (p_type: {}): p_offset: {}, p_vaddr: {}, p_paddr: {}, p_filesz: {}, p_memsz: {}, p_flags: {}, p_align: {}", get_seg_header_string(seg_header.p_type), log_hex(seg_header.p_type), log_hex(seg_header.p_offset), log_hex(seg_header.p_vaddr), log_hex(seg_header.p_paddr), log_hex(seg_header.p_filesz), log_hex(seg_header.p_memsz), log_hex(seg_header.p_flags), log_hex(seg_header.p_align));

        if (seg_infos[seg_index].encryption != 2) { // 0 should also be valid?
            LOG_ERROR("Cannot load ELF {}: invalid segment encryption status {}.", self_path, seg_infos[seg_index].encryption);
            free_all_segments(mem, segment_reloc_info);
            return -1;
        }

        if (seg_header.p_type == PT_NULL) {
            // Nothing to do.
        } else if (seg_header.p_type == PT_LOAD) {
            if (seg_header.p_memsz != 0) {
                Address segment_address = 0;
                auto alloc_name = fmt::format("{}:seg{}", self_path, seg_index);

                // TODO: when the virtual process bringup is fixed, uncomment this
                // Try allocating at image base for RELEXEC to avoid having to relocate the main module
                /*
                segment_address = try_alloc_at(mem, seg_header.p_vaddr, seg_header.p_memsz, alloc_name.c_str());

                if (!segment_address) {
                    if (isRelocatable) { //Try allocating somewhere else
                        segment_address = alloc(mem, seg_header.p_memsz, alloc_name.c_str());
                    }

                    if (!isRelocatable || !segment_address) {
                        LOG_CRITICAL("Loading {} ELF {} failed: Could not allocate {} bytes @ {} for segment {}.", (isRelocatable) ? "relocatable" : "fixed", self_path, log_hex(seg_header.p_memsz), log_hex(seg_header.p_vaddr), seg_index);
                        free_all_segments(mem, segment_reloc_info);
                        return SCE_KERNEL_ERROR_NO_MEMORY; //TODO is this correct?
                    }
                }
                */

                if (isRelocatable) {
                    segment_address = alloc(mem, seg_header.p_memsz, alloc_name.c_str());
                } else {
                    segment_address = alloc_at(mem, seg_header.p_vaddr, seg_header.p_memsz, alloc_name.c_str());
                }

                if (!segment_address) {
                    LOG_CRITICAL("Loading {} ELF {} failed: Could not allocate {} bytes @ {} for segment {}.", (isRelocatable) ? "relocatable" : "fixed", self_path, log_hex(seg_header.p_memsz), log_hex(seg_header.p_vaddr), seg_index);
                    free_all_segments(mem, segment_reloc_info);
                    return SCE_KERNEL_ERROR_NO_MEMORY; // TODO is this correct?
                }

                const Ptr<uint8_t> seg_ptr(segment_address);
                if (seg_infos[seg_index].compression == 2) {
                    unsigned long dest_bytes = seg_header.p_filesz;
                    const uint8_t *const compressed_segment_bytes = self_bytes + seg_infos[seg_index].offset;

                    int res = MZ_OK;
                    if (seg_infos[seg_index].length > 0)
                        res = mz_uncompress(seg_ptr.get(mem), &dest_bytes, compressed_segment_bytes, static_cast<mz_ulong>(seg_infos[seg_index].length));
                } else {
                    memcpy(seg_ptr.get(mem), seg_bytes, seg_header.p_filesz);
                }

                for (auto &patch : patches) {
                    // TODO patches should maybe be able to specify the path/file to patch?
                    if (seg_index == patch.seg && self_path.find("eboot.bin") != std::string::npos) {
                        LOG_INFO("Patching segment {} at offset 0x{:X} with {} values", seg_index, patch.offset, patch.values.size());
                        memcpy(seg_ptr.get(mem) + patch.offset, patch.values.data(), patch.values.size());
                    }
                }

                segment_reloc_info[seg_index] = { segment_address, seg_header.p_vaddr, seg_header.p_memsz };
            }
        } else if (seg_header.p_type == PT_SCE_RELA) {
            if (seg_infos[seg_index].compression == 2) {
                unsigned long dest_bytes = seg_header.p_filesz;
                const uint8_t *const compressed_segment_bytes = self_bytes + seg_infos[seg_index].offset;
                auto uncompressed = std::make_unique<uint8_t[]>(dest_bytes);

                int res = mz_uncompress(uncompressed.get(), &dest_bytes, compressed_segment_bytes, static_cast<mz_ulong>(seg_infos[seg_index].length));
                assert(res == MZ_OK);
                if (!relocate(uncompressed.get(), seg_header.p_filesz, segment_reloc_info, mem)) {
                    return -1;
                }

            } else {
                if (!relocate(seg_bytes, seg_header.p_filesz, segment_reloc_info, mem)) {
                    return -1;
                }
            }
        } else if ((seg_header.p_type == PT_SCE_COMMENT) || (seg_header.p_type == PT_SCE_VERSION)
            || (seg_header.p_type == PT_ARM_EXIDX) /* TODO: this may be important and require being loaded */) {
            LOG_INFO("{}: Skipping special segment {}...", self_path, log_hex(seg_header.p_type));
        } else {
            LOG_CRITICAL("{}: Skipping segment with unknown p_type {}!", self_path, log_hex(seg_header.p_type));
        }
    }

    if (kernel.debugger.dump_elfs) {
        // Dump elf
        std::vector<uint8_t> dump_elf(self_bytes + self_header.header_len, self_bytes + self_header.self_filesize);
        dump_elf.resize(self_header.elf_filesize);
        Elf32_Phdr *dump_segments = reinterpret_cast<Elf32_Phdr *>(dump_elf.data() + elf.e_phoff);
        uint16_t last_index = 0;
        for (const auto &[seg_index, segment] : segment_reloc_info) {
            uint8_t *seg_bytes = Ptr<uint8_t>(segment.addr).get(mem);
            memcpy(dump_elf.data() + dump_segments[seg_index].p_offset, seg_bytes, dump_segments[seg_index].p_filesz);
            dump_segments[seg_index].p_vaddr = segment.addr;
            last_index = std::max(seg_index, last_index);
        }
        fs::path dump_dir = log_path / "elfdumps";
        fs::create_directories(dump_dir);
        const auto start = dump_segments[0].p_vaddr;
        const auto end = dump_segments[last_index].p_vaddr + dump_segments[last_index].p_filesz;
        const auto elf_name = fs::path(self_path).filename().stem().string();
        const auto filename = dump_dir / fmt::format("{}-{}_{}.elf", log_hex_full(start), log_hex_full(end), elf_name);
        fs_utils::dump_data(filename, dump_elf.data(), dump_elf.size());
    }

    const unsigned int module_info_segment_index = elf.e_entry >> 30;
    const Ptr<const uint8_t> module_info_segment_address = Ptr<const uint8_t>(segment_reloc_info[module_info_segment_index].addr);
    const uint8_t *const module_info_segment_bytes = module_info_segment_address.get(mem);
    const sce_module_info_raw *const module_info = reinterpret_cast<const sce_module_info_raw *>(module_info_segment_bytes + module_info_offset);

    for (const auto &[seg, infos] : segment_reloc_info) {
        LOG_INFO("Loaded module segment {} @ [0x{:08X} - 0x{:08X} / 0x{:08X}] (size: 0x{:08X}) of module {}", seg, infos.addr, infos.addr + infos.size, infos.p_vaddr, infos.size, self_path);
    }

    const SceKernelModulePtr kernelModuleInfo = std::make_shared<KernelModule>();
    memset(kernelModuleInfo.get(), 0, sizeof(KernelModule));

    kernelModuleInfo->info_segment_address = module_info_segment_address;
    kernelModuleInfo->info_offset = module_info_offset;

    auto *sceKernelModuleInfo = &kernelModuleInfo->info;
    sceKernelModuleInfo->size = sizeof(*sceKernelModuleInfo);
    strncpy(sceKernelModuleInfo->module_name, module_info->name, 28);
    // unk28
    if (module_info->module_start != 0xffffffff && module_info->module_start != 0)
        sceKernelModuleInfo->start_entry = module_info_segment_address + module_info->module_start;
    // unk30
    if (module_info->module_stop != 0xffffffff && module_info->module_stop != 0)
        sceKernelModuleInfo->stop_entry = module_info_segment_address + module_info->module_stop;

    sceKernelModuleInfo->exidx_top = Ptr<const void>(module_info->exidx_top);
    sceKernelModuleInfo->exidx_btm = Ptr<const void>(module_info->exidx_end);
    sceKernelModuleInfo->extab_top = Ptr<const void>(module_info->extab_top);
    sceKernelModuleInfo->extab_btm = Ptr<const void>(module_info->extab_end);

    sceKernelModuleInfo->tlsInit = Ptr<const void>(!module_info->tls_start ? 0 : (module_info_segment_address.address() + module_info->tls_start));
    sceKernelModuleInfo->tlsInitSize = module_info->tls_filesz;
    sceKernelModuleInfo->tlsAreaSize = module_info->tls_memsz;

    if (sceKernelModuleInfo->tlsInit) {
        kernel.tls_address = sceKernelModuleInfo->tlsInit;
        kernel.tls_psize = sceKernelModuleInfo->tlsInitSize;
        kernel.tls_msize = sceKernelModuleInfo->tlsAreaSize;
    }

    strncpy(sceKernelModuleInfo->path, self_path.c_str(), 255);

    for (Elf_Half segment_index = 0; segment_index < elf.e_phnum; ++segment_index) {
        // Skip non-loadable segments
        auto it = segment_reloc_info.find(segment_index);
        if (it == segment_reloc_info.end())
            continue;

        if (segment_index >= MODULE_INFO_NUM_SEGMENTS) {
            LOG_ERROR("Segment {} should not be loadable", segment_index);
            continue;
        }

        SceKernelSegmentInfo &segment = sceKernelModuleInfo->segments[segment_index];
        segment.size = sizeof(segment);
        segment.vaddr = it->second.addr;
        segment.memsz = segments[segment_index].p_memsz;
        segment.filesz = segments[segment_index].p_filesz;
    }

    sceKernelModuleInfo->state = module_info->type;

    LOG_INFO("Linking SELF {}...", self_path);
    if (!load_exports(sceKernelModuleInfo, *module_info, module_info_segment_address, kernel, mem)) {
        return -1;
    }

    if (!load_imports(*module_info, module_info_segment_address, segment_reloc_info, kernel, mem)) {
        return -1;
    }
    const SceUID uid = kernel.get_next_uid();
    sceKernelModuleInfo->modid = uid;
    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        kernel.loaded_modules[uid] = kernelModuleInfo;
    }
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        kernel.module_uid_by_nid[module_info->module_nid] = uid;
    }

    return uid;
}

int unload_self(KernelState &kernel, MemState &mem, KernelModule &module) {
    LOG_INFO("Unlinking self...");
    const sce_module_info_raw *const module_info = reinterpret_cast<const sce_module_info_raw *>(module.info_segment_address.get(mem) + module.info_offset);
    if (!load_exports(&module.info, *module_info, module.info_segment_address, kernel, mem, true)) {
        return -1;
    }

    SegmentInfosForReloc segment_reloc_info;
    for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
        const auto &segment = module.info.segments[i];
        if (segment.size == 0)
            continue;

        segment_reloc_info[i] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
    }

    if (!load_imports(*module_info, module.info_segment_address, segment_reloc_info, kernel, mem, true)) {
        return -1;
    }

    SceUID mod_nid = module_info->module_nid;

    // last step: free the memory
    for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
        const auto &segment = module.info.segments[i];
        if (segment.size == 0)
            continue;

        kernel.invalidate_jit_cache(segment.vaddr.address(), segment.memsz);
        free(mem, module.info.segments[i].vaddr.address());
    }

    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        kernel.loaded_modules.erase(module.info.modid);
    }
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        kernel.module_uid_by_nid.erase(mod_nid);
    }

    return 0;
}
