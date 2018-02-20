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

#include "load_self.h"

#include "relocation.h"

#include <nids/functions.h>
#include <util/log.h>
#include <util/types.h>

#include <elfio/elf_types.hpp>
#define SCE_ELF_DEFS_TARGET
#include <sce-elf-defs.h>
#undef SCE_ELF_DEFS_TARGET
#include <self.h>

#include <miniz.h>

#include <assert.h>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace ELFIO;

static const bool LOG_IMPORTS = false;

static bool load_func_imports(const u32 *nids, const Ptr<u32> *entries, size_t count, const MemState &mem) {
    for (size_t i = 0; i < count; ++i) {
        const u32 nid = nids[i];
        const Ptr<u32> entry = entries[i];

        if (LOG_IMPORTS) {
            const char *const name = import_name(nid);
			LOG_DEBUG( "\tNID {:#08x} ({}) at {:#x}", nid, name, entry.address());
        }

        u32 *const stub = entry.get(mem);
        stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
        stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
        stub[2] = nid; // Our interrupt hook will read this.
    }

    return true;
}

static bool load_imports(const sce_module_info_raw &module, Ptr<const void> segment_address, const MemState &mem) {
    const u8 *const base = segment_address.cast<const u8>().get(mem);
    const sce_module_imports_raw *const imports_begin = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_top);
    const sce_module_imports_raw *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_end);

    for (const sce_module_imports_raw *imports = imports_begin; imports < imports_end; imports = reinterpret_cast<const sce_module_imports_raw *>(reinterpret_cast<const u8 *>(imports) + imports->size)) {
        if (LOG_IMPORTS) {
            const char *const lib_name = Ptr<const char>(imports->module_name).get(mem);
            LOG_INFO("Loading imports from {}", lib_name);
        }

        assert(imports->version == 1);
        assert(imports->num_syms_vars == 0);
        assert(imports->num_syms_unk == 0);

        const u32 *const nids = Ptr<const u32>(imports->func_nid_table).get(mem);
        const Ptr<u32> *const entries = Ptr<Ptr<u32>>(imports->func_entry_table).get(mem);
        if (!load_func_imports(nids, entries, imports->num_syms_funcs, mem)) {
            return false;
        }
    }

    return true;
}

bool load_self(Ptr<const void> &entry_point, MemState &mem, const void *self) {
    const u8 *const self_bytes = static_cast<const u8 *>(self);
    const SCE_header &self_header = *static_cast<const SCE_header *>(self);

    // assumes little endian host
    // TODO: do it in a better way, perhaps with user-defined literals that do the conversion automatically (magic != "SCE\0"_u32)
    if (!memcmp(&self_header.magic, "\0ECS", 4))
    {
        LOG_CRITICAL("(S)ELF is corrupt or encrypted. Decryption not yet supported.");
        return false;
    }

    const u8 *const elf_bytes = self_bytes + self_header.elf_offset;
    const Elf32_Ehdr &elf = *reinterpret_cast<const Elf32_Ehdr *>(elf_bytes);
    const u32 module_info_segment_index = static_cast<u32>(elf.e_entry >> 30);
    const u32 module_info_offset = elf.e_entry & 0x3fffffff;
    const Elf32_Phdr *const segments = reinterpret_cast<const Elf32_Phdr *>(self_bytes + self_header.phdr_offset);
    const Elf32_Phdr &module_info_segment = segments[module_info_segment_index];

    const segment_info *const segment_infos = reinterpret_cast<const segment_info *>(self_bytes + self_header.section_info_offset);

    SegmentAddresses segment_addrs;
    for (Elf_Half segment_index = 0; segment_index < elf.e_phnum; ++segment_index) {
        const Elf32_Phdr &src = segments[segment_index];
        const u8 *const segment_bytes = self_bytes + self_header.header_len + src.p_offset;

        assert(segment_infos[segment_index].encryption==2);
        if (src.p_type == PT_LOAD) {
            const Ptr<void> address(alloc(mem, src.p_memsz, "segment"));
            if (!address) {
                LOG_ERROR("Failed to allocate memory for segment.");
                return false;
            }

            if(segment_infos[segment_index].compression==2) {
                ulong dest_bytes = src.p_filesz;
                const u8 *const compressed_segment_bytes = self_bytes + segment_infos[segment_index].offset;

                s32 res = mz_uncompress(reinterpret_cast<u8 *>(address.get(mem)), &dest_bytes, compressed_segment_bytes, segment_infos[segment_index].length);
                assert(res == MZ_OK);
            } else {
                memcpy(address.get(mem), segment_bytes, src.p_filesz);
            }

            segment_addrs[segment_index] = address;
        } else if (src.p_type == PT_LOOS) {
            if(segment_infos[segment_index].compression==2) {
                ulong dest_bytes = src.p_filesz;
                const u8 *const compressed_segment_bytes = self_bytes + segment_infos[segment_index].offset;
                std::unique_ptr<u8> uncompressed(new u8[dest_bytes]);

                s32 res = mz_uncompress(uncompressed.get(), &dest_bytes, compressed_segment_bytes, segment_infos[segment_index].length);
                assert(res == MZ_OK);
                if (!relocate(uncompressed.get(), src.p_filesz, segment_addrs, mem)) {
                    return false;
                }
            } else {
                if (!relocate(segment_bytes, src.p_filesz, segment_addrs, mem)) {
                    return false;
                }
            }
        }
    }

    const Ptr<const u8> module_info_segment_address = segment_addrs[module_info_segment_index].cast<const u8>();
    const u8 *const module_info_segment_bytes = module_info_segment_address.get(mem);
    const sce_module_info_raw *const module_info = reinterpret_cast<const sce_module_info_raw *>(module_info_segment_bytes + module_info_offset);

    entry_point = module_info_segment_address + module_info->module_start;

    if (!load_imports(*module_info, module_info_segment_address, mem)) {
        return false;
    }

    return true;
}
