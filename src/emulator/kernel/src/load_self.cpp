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

#include <kernel/load_self.h>
#include <kernel/relocation.h>
#include <kernel/state.h>
#include <kernel/types.h>

#include <nids/functions.h>
#include <util/log.h>

#include <elfio/elf_types.hpp>
#define SCE_ELF_DEFS_TARGET
#include <sce-elf-defs.h>
#undef SCE_ELF_DEFS_TARGET
#include <self.h>

#define ET_SCE_EXEC 0xFE00

#define NID_MODULE_STOP 0x79F8E492
#define NID_MODULE_EXIT 0x913482A9
#define NID_MODULE_START 0x935CD196
#define NID_MODULE_INFO 0x6C2224BA
#define NID_SYSLYB 0x936c8a78
#define NID_PROCESS_PARAM 0x70FBA1E7
#define NID_STACK_CHK_GUARD 0x93B8AA67

#define __stack_chk_guard 0xDEADBEEF

#include <miniz.h>

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace ELFIO;

static const bool LOG_IMPORTS = false;
static const bool LOG_EXPORTS = false;

static bool load_var_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, const MemState &mem) {
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (LOG_IMPORTS) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        uint32_t *const stub = entry.get(mem);

        if (nid == NID_STACK_CHK_GUARD) {
            stub[0] = __stack_chk_guard;
            continue;
        }

        const ExportNids::iterator export_address = kernel.export_nids.find(nid);
        if (export_address == kernel.export_nids.end()) {
            const char *const name = import_name(nid);
            LOG_ERROR("\tNID NOT FOUND {} ({}) at {}: {}", log_hex(nid), name, log_hex(entry.address()), log_hex(stub[0]));
            continue;
        }
        uint32_t *const export_ptr = Ptr<uint32_t>(export_address->second).get(mem);
        stub[0] = export_ptr[0];
    }

    return true;
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

static bool load_func_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, const MemState &mem) {
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (LOG_IMPORTS) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        const ExportNids::iterator export_address = kernel.export_nids.find(nid);
        if (export_address == kernel.export_nids.end()) {
            uint32_t *const stub = entry.get(mem);
            stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
            stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
            stub[2] = nid; // Our interrupt hook will read this.
        } else {
            Address func_address = export_address->second;
            uint32_t *const stub = entry.get(mem);
            stub[0] = encode_arm_inst(INSTRUCTION_MOVW, (uint16_t)func_address, 12);
            stub[1] = encode_arm_inst(INSTRUCTION_MOVT, (uint16_t)(func_address >> 16), 12);
            stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        }
    }

    return true;
}

static bool load_imports(const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, const MemState &mem) {
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_imports_raw *const imports_begin = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_top);
    const sce_module_imports_raw *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_end);

    for (const sce_module_imports_raw *imports = imports_begin; imports < imports_end; imports = reinterpret_cast<const sce_module_imports_raw *>(reinterpret_cast<const uint8_t *>(imports) + imports->size)) {
        assert(imports->num_syms_unk == 0);

        std::string lib_name;
        if (LOG_IMPORTS) {
            lib_name = Ptr<const char>(imports->module_name).get(mem);
            LOG_INFO("Loading func imports from {}", lib_name);
        }

        const uint32_t *const nids = Ptr<const uint32_t>(imports->func_nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(imports->func_entry_table).get(mem);
        if (!load_func_imports(nids, entries, imports->num_syms_funcs, kernel, mem)) {
            return false;
        }

        const uint32_t *const var_nids = Ptr<const uint32_t>(imports->var_nid_table).get(mem);
        const Ptr<uint32_t> *const var_entries = Ptr<Ptr<uint32_t>>(imports->var_entry_table).get(mem);
        const auto var_count = imports->num_syms_vars;

        if (LOG_IMPORTS && var_count > 0) {
            LOG_INFO("Loading var imports from {}", lib_name);
        }

        if (!load_var_imports(var_nids, var_entries, var_count, kernel, mem)) {
            return false;
        }
    }

    return true;
}

static bool load_func_exports(Ptr<const void> &entry_point, const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel) {
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (nid == NID_MODULE_START) {
            entry_point = entry;
            continue;
        }

        if (nid == NID_MODULE_STOP || nid == NID_MODULE_EXIT)
            continue;

        kernel.export_nids.emplace(nid, entry.address());

        if (LOG_EXPORTS) {
            const char *const name = import_name(nid);

            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }
    }

    return true;
}

static bool load_var_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel) {
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (nid == NID_PROCESS_PARAM) {
            kernel.process_param = entry;
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

        if (LOG_EXPORTS) {
            const char *const name = import_name(nid);

            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        kernel.export_nids.emplace(nid, entry.address());
    }

    return true;
}

static bool load_exports(Ptr<const void> &entry_point, const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, const MemState &mem) {
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_exports_raw *const exports_begin = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_top);
    const sce_module_exports_raw *const exports_end = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_end);

    for (const sce_module_exports_raw *exports = exports_begin; exports < exports_end; exports = reinterpret_cast<const sce_module_exports_raw *>(reinterpret_cast<const uint8_t *>(exports) + exports->size)) {
        const char *const lib_name = Ptr<const char>(exports->module_name).get(mem);

        if (LOG_EXPORTS) {
            LOG_INFO("Loading func exports from {}", lib_name);
        }

        const uint32_t *const nids = Ptr<const uint32_t>(exports->nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(exports->entry_table).get(mem);
        if (!load_func_exports(entry_point, nids, entries, exports->num_syms_funcs, kernel)) {
            return false;
        }

        const auto var_count = exports->num_syms_vars;

        if (LOG_EXPORTS && var_count > 0) {
            LOG_INFO("Loading var exports from {}", lib_name);
        }

        if (!load_var_exports(&nids[exports->num_syms_funcs], &entries[exports->num_syms_funcs], var_count, kernel)) {
            return false;
        }
    }

    return true;
}

/**
 * \return Negative on failure
 */
SceUID load_self(Ptr<const void> &entry_point, KernelState &kernel, MemState &mem, const void *self, const std::string &path) {
    const uint8_t *const self_bytes = static_cast<const uint8_t *>(self);
    const SCE_header &self_header = *static_cast<const SCE_header *>(self);

    // assumes little endian host
    if (self_header.magic != 0x00454353) {
        LOG_CRITICAL("SELF {} is corrupt or encrypted. Decryption is not yet supported.", path);
        return -1;
    }

    if (self_header.version != 3) {
        LOG_CRITICAL("SELF {} version {} is not supported.", path, self_header.version);
        return -1;
    }

    if (self_header.header_type != 1) {
        LOG_CRITICAL("SELF {} header type {} is not supported.", path, self_header.header_type);
        return -1;
    }

    const uint8_t *const elf_bytes = self_bytes + self_header.elf_offset;
    const Elf32_Ehdr &elf = *reinterpret_cast<const Elf32_Ehdr *>(elf_bytes);
    const uint32_t module_info_offset = elf.e_entry & 0x3fffffff;
    const Elf32_Phdr *const segments = reinterpret_cast<const Elf32_Phdr *>(self_bytes + self_header.phdr_offset);

    const segment_info *const segment_infos = reinterpret_cast<const segment_info *>(self_bytes + self_header.section_info_offset);

    SegmentAddresses segment_addrs;
    for (Elf_Half segment_index = 0; segment_index < elf.e_phnum; ++segment_index) {
        const Elf32_Phdr &src = segments[segment_index];
        const uint8_t *const segment_bytes = self_bytes + self_header.header_len + src.p_offset;

        assert(segment_infos[segment_index].encryption == 2);
        if (src.p_type == PT_LOAD) {
            Address segment_address = 0;
            if (elf.e_type == ET_SCE_EXEC) {
                segment_address = alloc_at(mem, src.p_vaddr, src.p_memsz, path.c_str());
            } else {
                segment_address = alloc(mem, src.p_memsz, path.c_str());
            }
            const Ptr<void> address(segment_address);
            if (!address) {
                LOG_ERROR("Failed to allocate memory for segment.");
                return -1;
            }

            if (segment_infos[segment_index].compression == 2) {
                unsigned long dest_bytes = src.p_filesz;
                const uint8_t *const compressed_segment_bytes = self_bytes + segment_infos[segment_index].offset;

                int res = mz_uncompress(reinterpret_cast<uint8_t *>(address.get(mem)), &dest_bytes, compressed_segment_bytes, segment_infos[segment_index].length);
                assert(res == MZ_OK);
            } else {
                memcpy(address.get(mem), segment_bytes, src.p_filesz);
            }

            segment_addrs[segment_index] = address;
        } else if (src.p_type == PT_LOOS) {
            if (segment_infos[segment_index].compression == 2) {
                unsigned long dest_bytes = src.p_filesz;
                const uint8_t *const compressed_segment_bytes = self_bytes + segment_infos[segment_index].offset;
                std::unique_ptr<uint8_t> uncompressed(new uint8_t[dest_bytes]);

                int res = mz_uncompress(uncompressed.get(), &dest_bytes, compressed_segment_bytes, segment_infos[segment_index].length);
                assert(res == MZ_OK);
                if (!relocate(uncompressed.get(), src.p_filesz, segment_addrs, mem)) {
                    return -1;
                }
            } else {
                if (!relocate(segment_bytes, src.p_filesz, segment_addrs, mem)) {
                    return -1;
                }
            }
        }
    }

    const unsigned int module_info_segment_index = static_cast<unsigned int>(elf.e_entry >> 30);
    const Ptr<const uint8_t> module_info_segment_address = segment_addrs[module_info_segment_index].cast<const uint8_t>();
    const uint8_t *const module_info_segment_bytes = module_info_segment_address.get(mem);
    const sce_module_info_raw *const module_info = reinterpret_cast<const sce_module_info_raw *>(module_info_segment_bytes + module_info_offset);

    const SceKernelModuleInfoPtr sceKernelModuleInfo = std::make_shared<emu::SceKernelModuleInfo>();
    sceKernelModuleInfo->size = sizeof(*sceKernelModuleInfo);
    strncpy(sceKernelModuleInfo->module_name, module_info->name, 28);
    //unk28
    if (module_info->module_start != 0xffffffff && module_info->module_start != 0)
        entry_point = module_info_segment_address + module_info->module_start;
    else
        entry_point = Ptr<const void>(0);
    //unk30
    if (module_info->module_stop != 0xffffffff && module_info->module_stop != 0)
        sceKernelModuleInfo->module_stop = module_info_segment_address + module_info->module_stop;

    const Ptr<const void> exidx_top = Ptr<const void>(module_info->exidx_top);
    sceKernelModuleInfo->exidxTop = exidx_top;
    const Ptr<const void> exidx_btm = Ptr<const void>(module_info->exidx_end);
    sceKernelModuleInfo->exidxBtm = exidx_btm;

    //unk40
    //unk44
    sceKernelModuleInfo->tlsInit = Ptr<const void>(module_info_segment_address + module_info->field_38);
    sceKernelModuleInfo->tlsInitSize = module_info->field_3C;
    sceKernelModuleInfo->tlsAreaSize = module_info->field_40;
    //SceSize tlsInitSize;
    //SceSize tlsAreaSize;
    strncpy(sceKernelModuleInfo->path, path.c_str(), 255);

    for (Elf_Half segment_index = 0; segment_index < elf.e_phnum; ++segment_index) {
        sceKernelModuleInfo->segments[segment_index].size = sizeof(sceKernelModuleInfo->segments[segment_index]);
        sceKernelModuleInfo->segments[segment_index].vaddr = segment_addrs[segment_index];
        sceKernelModuleInfo->segments[segment_index].memsz = segments[segment_index].p_memsz;
    }

    sceKernelModuleInfo->type = module_info->type;

    LOG_INFO("Loading symbols for SELF: {}", path);

    if (!load_exports(entry_point, *module_info, module_info_segment_address, kernel, mem)) {
        return -1;
    }

    if (!load_imports(*module_info, module_info_segment_address, kernel, mem)) {
        return -1;
    }

    sceKernelModuleInfo->module_start = entry_point;
    // TODO: module_stop

    const std::lock_guard<std::mutex> lock(kernel.mutex);
    const SceUID uid = kernel.get_next_uid();
    sceKernelModuleInfo->handle = uid;
    kernel.loaded_modules.emplace(uid, sceKernelModuleInfo);

    return uid;
}
