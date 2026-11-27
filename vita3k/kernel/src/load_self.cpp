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

#include <kernel/load_self.h>
#include <kernel/relocation.h>
#include <kernel/state.h>
#include <kernel/types.h>

#include <cpu/arm.h>
#include <nids/functions.h>
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

static constexpr uint32_t NID_MODULE_STOP = 0x79F8E492;
static constexpr uint32_t NID_MODULE_EXIT = 0x913482A9;
static constexpr uint32_t NID_MODULE_START = 0x935CD196;
static constexpr uint32_t NID_MODULE_INFO = 0x6C2224BA;
static constexpr uint32_t NID_SYSLYB = 0x936c8a78;
static constexpr uint32_t NID_PROCESS_PARAM = 0x70FBA1E7;
static constexpr uint32_t NID_MODULE_START_THREAD_PARAM = 0x1A9822A4;
static constexpr uint32_t NID_MODULE_STOP_THREAD_PARAM = 0xD20886EB;

static constexpr bool LOG_MODULE_LOADING = false;

struct VarImportsHeader {
    uint32_t unk : 4; // Must be zero
    uint32_t reloc_data_size : 24; // Size of Relocation data in bytes, includes this header.
    uint32_t unk2 : 4; // Must be zero
};
static_assert(sizeof(VarImportsHeader) == sizeof(uint32_t));

// The fields of an import entry of either size
struct ImportEntry {
    uint32_t library_nid;
    Address library_name;
    uint16_t version;
    uint16_t attr;
    size_t func_count;
    const uint32_t *func_nids;
    const Ptr<uint32_t> *func_entries;
    size_t var_count;
    const uint32_t *var_nids;
    const Ptr<uint32_t> *var_entries;
};

static ImportEntry read_import(const MemState &mem, const sce_module_imports_raw &imports) {
    assert(imports.num_syms_tls_vars == 0);
    ImportEntry entry{ 0, 0, imports.version, imports.flags, imports.num_syms_funcs, nullptr, nullptr, imports.num_syms_vars, nullptr, nullptr };
    if (imports.size == 0x24) {
        const auto &short_imports = reinterpret_cast<const sce_module_imports_short_raw &>(imports);
        entry.library_nid = short_imports.library_nid;
        entry.library_name = short_imports.library_name;
        entry.func_nids = Ptr<const uint32_t>(short_imports.func_nid_table).get(mem);
        entry.func_entries = Ptr<Ptr<uint32_t>>(short_imports.func_entry_table).get(mem);
        entry.var_nids = Ptr<const uint32_t>(short_imports.var_nid_table).get(mem);
        entry.var_entries = Ptr<Ptr<uint32_t>>(short_imports.var_entry_table).get(mem);
    } else if (imports.size == 0x34) {
        entry.library_nid = imports.library_nid;
        entry.library_name = imports.library_name;
        entry.func_nids = Ptr<const uint32_t>(imports.func_nid_table).get(mem);
        entry.func_entries = Ptr<Ptr<uint32_t>>(imports.func_entry_table).get(mem);
        entry.var_nids = Ptr<const uint32_t>(imports.var_nid_table).get(mem);
        entry.var_entries = Ptr<Ptr<uint32_t>>(imports.var_entry_table).get(mem);
    } else
        entry.func_count = entry.var_count = 0;
    return entry;
}

static const sce_module_imports_raw *next_import(const sce_module_imports_raw *imports) {
    return reinterpret_cast<const sce_module_imports_raw *>(reinterpret_cast<const uint8_t *>(imports) + imports->size);
}

static const sce_module_info_raw &module_info_of(const MemState &mem, const KernelModule &module) {
    return *reinterpret_cast<const sce_module_info_raw *>(module.info_segment_address.get(mem) + module.info_offset);
}

// The reference table of a variable import, after its header
static void var_references(const MemState &mem, Ptr<uint32_t> entry, void *&references, uint32_t &size) {
    VarImportsHeader *const var_reloc_header = entry.cast<VarImportsHeader>().get(mem);
    references = static_cast<void *>(var_reloc_header + 1);
    size = (var_reloc_header->reloc_data_size > sizeof(VarImportsHeader)) ? (var_reloc_header->reloc_data_size - sizeof(VarImportsHeader)) : 0;
}

// The stub word every import of a variable that no library exports points at
static Address var_stub(KernelState &kernel, MemState &mem, uint32_t nid) {
    if (const auto it = kernel.var_stubs.find(nid); it != kernel.var_stubs.end())
        return it->second;
    constexpr uint32_t STUB_SYMVAL = 0xDEADBEEF;
    LOG_DEBUG("\tNID NOT FOUND {} ({}), setting to stub value {}", log_hex(nid), import_name(nid), log_hex(STUB_SYMVAL));
    const std::string alloc_name = fmt::format("Stub var import reloc symval, NID {} ({})", log_hex(nid), import_name(nid));
    const Ptr<uint32_t> stub(alloc(mem, 4, alloc_name.c_str()));
    *stub.get(mem) = STUB_SYMVAL;
    kernel.var_stubs.emplace(nid, stub.address());
    return stub.address();
}

static bool load_var_imports(const ImportEntry &import, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, const KernelModule &module) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < import.var_count; ++i) {
        const uint32_t nid = import.var_nids[i];
        const Ptr<uint32_t> entry = import.var_entries[i];

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}). entry: {}, *entry: {}", log_hex(nid), name, entry, log_hex(*entry.get(mem)));
        }

        void *references;
        uint32_t reloc_size;
        var_references(mem, entry, references, reloc_size);

        kernel.var_binding_infos.emplace(nid, VarBindingInfo{ references, reloc_size, &module });
        const ExportNids::iterator export_address_it = kernel.export_nids.find(nid);
        const Address export_address = export_address_it != kernel.export_nids.end() ? export_address_it->second : var_stub(kernel, mem, nid);

        if (reloc_size)
            if (!relocate(references, reloc_size, segments, mem, true, export_address))
                return false;
    }

    return true;
}

static void unload_var_imports(const ImportEntry &import, KernelState &kernel, const MemState &mem, const KernelModule &module) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    for (size_t i = 0; i < import.var_count; ++i) {
        void *references;
        uint32_t reloc_size;
        var_references(mem, import.var_entries[i], references, reloc_size);
        auto range = kernel.var_binding_infos.equal_range(import.var_nids[i]);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.entries == references && it->second.module == &module) {
                kernel.var_binding_infos.erase(it);
                break;
            }
        }
    }
}

// A stub that no library is linked to calls the HLE function of its NID
static bool is_unlinked(const uint32_t *stub) {
    return stub[0] == arm::ARM_SVC && stub[1] == arm::ARM_MOV_PC_LR;
}

// Points an import stub at a function, or at the HLE function of its NID for address 0
static void write_stub(uint32_t *stub, uint32_t nid, Address address) {
    if (!address) {
        stub[0] = arm::ARM_SVC; // Call our interrupt hook.
        stub[1] = arm::ARM_MOV_PC_LR; // Return to the caller.
        stub[2] = nid; // Our interrupt hook will read this.
    } else {
        stub[0] = arm::encode_arm_inst(arm::Instruction::MOVW, static_cast<uint16_t>(address), arm::REG_IP);
        stub[1] = arm::encode_arm_inst(arm::Instruction::MOVT, static_cast<uint16_t>(address >> 16), arm::REG_IP);
        stub[2] = arm::encode_arm_inst(arm::Instruction::BRANCH, 0, arm::REG_IP);
    }
}

static bool same_name(const MemState &mem, Address name, Address other) {
    if (!name || !other)
        return name == other;
    return strncmp(Ptr<const char>(name).get(mem), Ptr<const char>(other).get(mem), 256) == 0;
}

static const sce_module_exports_raw &entry_of(const MemState &mem, const ExportLibrary &library) {
    return *Ptr<const sce_module_exports_raw>(library.entry).get(mem);
}

// The library an import links to: the first library of the list with its NID and name that isn't
// NOLINK_EXPORT, if its version isn't below the import's
static const ExportLibrary *find_library(const KernelState &kernel, const MemState &mem, uint32_t library_nid, Address library_name, uint16_t version) {
    for (const ExportLibrary *library = kernel.libraries; library; library = library->next) {
        const sce_module_exports_raw &entry = entry_of(mem, *library);
        if ((entry.flags & SCE_KERNEL_EXPORT_ATTR_NOLINK_EXPORT) || entry.library_nid != library_nid || !same_name(mem, entry.library_name, library_name))
            continue;
        return entry.version >= version ? library : nullptr;
    }
    return nullptr;
}

// Links a stub to the library the lookup finds for its import, or to the HLE function. An import
// of no library in the list falls back to an export that no library owns.
static void link_stub(KernelState &kernel, MemState &mem, uint32_t nid, FuncBindingInfo &import) {
    import.library = find_library(kernel, mem, import.library_nid, import.library_name, import.version);
    Address address = 0;
    if (import.library) {
        const auto it = kernel.export_nids_by_lib.find(lib_export_key(import.library_nid, nid));
        address = it != kernel.export_nids_by_lib.end() ? it->second : 0;
    } else if (const auto it = kernel.export_nids.find(nid); it != kernel.export_nids.end() && !kernel.export_nid_owners.contains(nid))
        address = it->second;
    write_stub(Ptr<uint32_t>(import.entry_address).get(mem), nid, address);
    kernel.invalidate_jit_cache(import.entry_address, 3 * sizeof(uint32_t));
}

// Gives each function stub the HLE form until the start links it, and relocates the uses of the
// stubs' addresses
static bool load_func_imports(const ImportEntry &import, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem) {
    for (size_t i = 0; i < import.func_count; ++i) {
        const uint32_t nid = import.func_nids[i];
        const Ptr<uint32_t> entry = import.func_entries[i];

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        uint32_t *const stub = entry.get(mem);
        write_stub(stub, nid, 0);
        if (stub[3]) { // if function's associated reftable exists
            void *references;
            uint32_t reloc_size;
            var_references(mem, Ptr<uint32_t>(stub[3]), references, reloc_size);
            if (reloc_size && !relocate(references, reloc_size, segments, mem, true, entry.address()))
                return false;
        }
    }
    return true;
}

static bool load_imports(const KernelModule &kernel_module, const sce_module_info_raw &module, Ptr<const void> segment_address, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, bool is_unload = false) {
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const auto *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_end);
    for (const auto *imports = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_top); imports < imports_end; imports = next_import(imports)) {
        const ImportEntry import = read_import(mem, *imports);
        if (kernel.debugger.log_imports)
            LOG_INFO("Loading imports from {}", Ptr<const char>(import.library_name).get(mem));
        if (is_unload) {
            unload_var_imports(import, kernel, mem, kernel_module);
            continue;
        }
        if (!load_func_imports(import, segments, kernel, mem) || !load_var_imports(import, segments, kernel, mem, kernel_module))
            return false;
    }
    return true;
}

// Links each function stub of the module to the library the lookup finds for it, or unlinks it.
// The importers of the list are those of linked modules. The main module's imports are all weak.
static void link_imports(KernelState &kernel, MemState &mem, const KernelModule &module, bool link) {
    if (!module.info_segment_address)
        return;
    const sce_module_info_raw &info = module_info_of(mem, module);
    const uint8_t *const base = module.info_segment_address.get(mem);
    const auto *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + info.import_end);
    for (const auto *imports = reinterpret_cast<const sce_module_imports_raw *>(base + info.import_top); imports < imports_end; imports = next_import(imports)) {
        ImportEntry import = read_import(mem, *imports);
        if (module.flags & SCE_KERNEL_MODULE_FLAG_MAIN)
            import.attr |= SCE_KERNEL_IMPORT_ATTR_WEAK_IMPORT;
        for (size_t i = 0; i < import.func_count; ++i) {
            const uint32_t nid = import.func_nids[i];
            const Address stub_address = import.func_entries[i].address();
            if (link) {
                const auto it = kernel.func_binding_infos.emplace(nid, FuncBindingInfo{ stub_address, import.library_nid, import.library_name, import.version, import.attr, nullptr });
                link_stub(kernel, mem, nid, it->second);
                continue;
            }
            const auto range = kernel.func_binding_infos.equal_range(nid);
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second.entry_address == stub_address) {
                    kernel.func_binding_infos.erase(it);
                    break;
                }
            }
            write_stub(Ptr<uint32_t>(stub_address).get(mem), nid, 0);
            kernel.invalidate_jit_cache(stub_address, 3 * sizeof(uint32_t));
        }
    }
}

static bool is_entry_nid(uint32_t nid) {
    return nid == NID_MODULE_START || nid == NID_MODULE_STOP || nid == NID_MODULE_EXIT;
}

static bool is_parameter_nid(uint32_t nid) {
    return nid == NID_PROCESS_PARAM || nid == NID_MODULE_INFO || nid == NID_SYSLYB || nid == NID_MODULE_START_THREAD_PARAM || nid == NID_MODULE_STOP_THREAD_PARAM;
}

// Relocates the references of every module that imports a variable to a new address
static void relocate_var_imports(KernelState &kernel, MemState &mem, uint32_t nid, Address address) {
    auto range = kernel.var_binding_infos.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        const VarBindingInfo &var_binding_info = it->second;
        if (var_binding_info.size == 0)
            continue;

        SegmentInfosForReloc seg;
        for (int k = 0; k < MODULE_INFO_NUM_SEGMENTS; k++) {
            const auto &segment = var_binding_info.module->info.segments[k];
            if (segment.size > 0) {
                seg[k] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
            }
        }

        // Variables are taken as never imported into executable code, so no JIT cache is invalidated
        if (!seg.empty()) {
            if (!relocate(var_binding_info.entries, var_binding_info.size, seg, mem, true, address)) {
                LOG_ERROR("Failed to relocate late binding info");
            }
        }
    }
}

// Writes a library's functions and variables to the export tables
static void publish_library(KernelState &kernel, MemState &mem, const sce_module_exports_raw &entry) {
    const uint32_t *const nids = Ptr<const uint32_t>(entry.nid_table).get(mem);
    const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem);
    if (kernel.debugger.log_exports)
        LOG_INFO("Loading exports from {}", entry.library_name ? Ptr<const char>(entry.library_name).get(mem) : "unknown");
    for (size_t i = 0; i < entry.num_syms_funcs + entry.num_syms_vars; ++i) {
        const uint32_t nid = nids[i];
        const Address address = entries[i].address();
        if (is_entry_nid(nid) || is_parameter_nid(nid))
            continue;
        if (kernel.debugger.log_exports)
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), import_name(nid), log_hex(address));

        if (i < entry.num_syms_funcs) {
            // Only the library that actually took the plain NID entry may drop it again.
            if (kernel.export_nids.emplace(nid, address).second)
                kernel.export_nid_owners.insert_or_assign(nid, entry.library_nid);
            kernel.export_nids_by_lib.insert_or_assign(lib_export_key(entry.library_nid, nid), address);
            continue;
        }

        // The library that took a variable first keeps it, and the export replaces the stub word
        if (!kernel.export_nid_owners.emplace(nid, entry.library_nid).second)
            continue;
        kernel.export_nids[nid] = address;
        relocate_var_imports(kernel, mem, nid, address);
        if (const auto it = kernel.var_stubs.find(nid); it != kernel.var_stubs.end()) {
            free(mem, it->second);
            kernel.var_stubs.erase(it);
        }
    }
}

// Takes a library's functions and variables out of the export tables, its variables become stub words
static void withdraw_library(KernelState &kernel, MemState &mem, const sce_module_exports_raw &entry) {
    const uint32_t *const nids = Ptr<const uint32_t>(entry.nid_table).get(mem);
    const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem);
    for (size_t i = 0; i < entry.num_syms_funcs + entry.num_syms_vars; ++i) {
        const uint32_t nid = nids[i];
        if (is_entry_nid(nid) || is_parameter_nid(nid))
            continue;

        if (i < entry.num_syms_funcs) {
            // taiHEN may have moved the library key to a hook since, which stays
            if (const auto it = kernel.export_nids_by_lib.find(lib_export_key(entry.library_nid, nid)); it != kernel.export_nids_by_lib.end() && it->second == entries[i].address())
                kernel.export_nids_by_lib.erase(it);
            // Redirects move the address but not the ownership, so only the owner may drop the plain entry.
            if (const auto it = kernel.export_nid_owners.find(nid); it != kernel.export_nid_owners.end() && it->second == entry.library_nid) {
                kernel.export_nids.erase(nid);
                kernel.export_nid_owners.erase(it);
            }
            continue;
        }

        const auto owner = kernel.export_nid_owners.find(nid);
        if (owner == kernel.export_nid_owners.end() || owner->second != entry.library_nid)
            continue;
        kernel.export_nid_owners.erase(owner);
        kernel.export_nids.erase(nid);
        relocate_var_imports(kernel, mem, nid, var_stub(kernel, mem, nid));
    }
}

// Links the unlinked stubs of a library's importers to it, those of a version up to the library's
static void link_unlinked_imports(KernelState &kernel, MemState &mem, const ExportLibrary &library) {
    const sce_module_exports_raw &entry = entry_of(mem, library);
    const uint32_t *const nids = Ptr<const uint32_t>(entry.nid_table).get(mem);
    const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem);
    for (size_t i = 0; i < entry.num_syms_funcs; ++i) {
        const auto range = kernel.func_binding_infos.equal_range(nids[i]);
        for (auto it = range.first; it != range.second; ++it) {
            FuncBindingInfo &import = it->second;
            uint32_t *const stub = Ptr<uint32_t>(import.entry_address).get(mem);
            if (import.library_nid != entry.library_nid || import.version > entry.version || !is_unlinked(stub) || !same_name(mem, import.library_name, entry.library_name))
                continue;
            import.library = &library;
            write_stub(stub, nids[i], entries[i].address());
            kernel.invalidate_jit_cache(import.entry_address, 3 * sizeof(uint32_t));
        }
    }
}

// Adds a library to the list. A library of the same NID and name gets hidden by a NOLINK_EXPORT
// library if it is one too, or else by a library of a higher version.
static int register_library(KernelState &kernel, MemState &mem, ExportLibrary &library) {
    if (library.registered)
        return SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB;
    const sce_module_exports_raw &entry = entry_of(mem, library);
    if ((entry.flags & SCE_KERNEL_EXPORT_ATTR_SYSLIB) || !(entry.flags & SCE_KERNEL_EXPORT_ATTR_AUTO_EXPORT))
        return 0;

    ExportLibrary **link = &kernel.libraries;
    while (*link && !(entry_of(mem, **link).library_nid == entry.library_nid && same_name(mem, entry_of(mem, **link).library_name, entry.library_name)))
        link = &(*link)->next;
    if (*link) {
        const sce_module_exports_raw &old = entry_of(mem, **link);
        const bool nolink = entry.flags & SCE_KERNEL_EXPORT_ATTR_NOLINK_EXPORT;
        if (nolink != bool(old.flags & SCE_KERNEL_EXPORT_ATTR_NOLINK_EXPORT) || (!nolink && entry.version <= old.version))
            return SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB;
        // The importers linked to the older library stay linked to it
        withdraw_library(kernel, mem, old);
        library.older = *link;
        *link = (*link)->next;
    }
    library.next = kernel.libraries;
    kernel.libraries = &library;
    library.registered = true;
    publish_library(kernel, mem, entry);
    link_unlinked_imports(kernel, mem, library);
    return 0;
}

// Takes a library out of the list, or out of the older libraries one of the list hides. Returns
// whether it was in the list itself.
static bool remove_library(KernelState &kernel, ExportLibrary &library) {
    for (ExportLibrary **link = &kernel.libraries; *link; link = &(*link)->next) {
        if (*link == &library) {
            if (library.older) {
                library.older->next = library.next;
                *link = library.older;
            } else
                *link = library.next;
            return true;
        }
        for (ExportLibrary **hidden = &(*link)->older; *hidden; hidden = &(*hidden)->older) {
            if (*hidden == &library) {
                *hidden = library.older;
                return false;
            }
        }
    }
    return false;
}

// Removes a library from the list, an older one it hid comes back. Each stub linked to it calls the
// HLE function until the lookup finds another library for it. A forced removal skips the check for
// strong imports.
static int unregister_library(KernelState &kernel, MemState &mem, ExportLibrary &library, bool forced) {
    const sce_module_exports_raw &entry = entry_of(mem, library);
    if (!library.registered)
        return (entry.flags & SCE_KERNEL_EXPORT_ATTR_SYSLIB) ? 0 : SCE_KERNEL_ERROR_LIBRARYDB_NO_LIB;
    // An import linked to the library counts even for a function the library doesn't export
    for (const auto &[nid, import] : kernel.func_binding_infos) {
        if (!forced && import.library == &library && !(import.attr & SCE_KERNEL_IMPORT_ATTR_WEAK_IMPORT))
            return SCE_KERNEL_ERROR_MODULEMGR_IN_USE;
    }

    ExportLibrary *const older = library.older;
    if (remove_library(kernel, library)) {
        withdraw_library(kernel, mem, entry);
        if (older)
            publish_library(kernel, mem, entry_of(mem, *older));
    }
    library.next = nullptr;
    library.older = nullptr;
    library.registered = false;

    for (auto &[nid, import] : kernel.func_binding_infos) {
        if (import.library == &library)
            link_stub(kernel, mem, nid, import);
    }
    return 0;
}

int link_module(KernelState &kernel, MemState &mem, KernelModule &module) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    link_imports(kernel, mem, module, true);
    for (ExportLibrary &library : module.libraries) {
        if (const int error = register_library(kernel, mem, library); error < 0) {
            link_imports(kernel, mem, module, false);
            return error;
        }
    }
    return 0;
}

int unlink_module(KernelState &kernel, MemState &mem, KernelModule &module, bool forced) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    link_imports(kernel, mem, module, false);
    for (ExportLibrary &library : module.libraries) {
        if (const int error = unregister_library(kernel, mem, library, forced); error < 0) {
            if (!forced)
                link_imports(kernel, mem, module, true);
            return error;
        }
    }
    return 0;
}

ExportFunctions export_functions(const MemState &mem, const ExportLibrary &library) {
    const sce_module_exports_raw &entry = entry_of(mem, library);
    return { Ptr<const uint32_t>(entry.nid_table).get(mem), Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem), entry.num_syms_funcs };
}

uint32_t module_nid(const MemState &mem, const KernelModule &module) {
    return module_info_of(mem, module).module_nid;
}

Address find_module_export(const MemState &mem, const KernelModule &module, uint32_t nid) {
    for (const ExportLibrary &library : module.libraries) {
        const sce_module_exports_raw &entry = entry_of(mem, library);
        const uint32_t *const nids = Ptr<const uint32_t>(entry.nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem);
        for (size_t i = 0; i < entry.num_syms_funcs + entry.num_syms_vars; ++i) {
            if (nids[i] == nid)
                return entries[i].address();
        }
    }
    return 0;
}

// Keeps a record of each export library, which the start registers, and reads the entry points and
// parameters from them
static void load_exports(KernelModule &kernel_module, const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, MemState &mem) {
    for (Address address = segment_address.address() + module.export_top; address < segment_address.address() + module.export_end; address += entry_of(mem, kernel_module.libraries.back()).size) {
        kernel_module.libraries.push_back({ address, nullptr, nullptr, false });
        const sce_module_exports_raw &entry = entry_of(mem, kernel_module.libraries.back());
        const uint32_t *const nids = Ptr<const uint32_t>(entry.nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(entry.entry_table).get(mem);
        for (size_t i = 0; i < entry.num_syms_funcs + entry.num_syms_vars; ++i) {
            const uint32_t nid = nids[i];
            const Ptr<uint32_t> export_entry = entries[i];
            if (nid == NID_MODULE_START)
                kernel_module.info.start_entry = export_entry;
            else if (nid == NID_MODULE_STOP)
                kernel_module.info.stop_entry = export_entry;
            else if (nid == NID_MODULE_EXIT)
                kernel_module.info.exit_entry = export_entry;
            else if (nid == NID_MODULE_START_THREAD_PARAM)
                kernel_module.start_thread_param = export_entry.address();
            else if (nid == NID_MODULE_STOP_THREAD_PARAM)
                kernel_module.stop_thread_param = export_entry.address();
            else if (nid == NID_PROCESS_PARAM) {
                // The process parameters also add the libc parameter export
                const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
                if (!kernel.process_param)
                    kernel.load_process_param(mem, export_entry);
            }
        }
    }
}

/**
 * \return Negative on failure
 */
SceUID load_self(KernelState &kernel, MemState &mem, const void *self, const std::string &self_path, const fs::path &dump_path, uint16_t flags) {
    const uint8_t *const image_bytes = static_cast<const uint8_t *>(self);
    const SCE_header &self_header = *static_cast<const SCE_header *>(self);

    constexpr uint32_t SCE_MAGIC = 0x00454353; // "SCE\0"
    const bool is_self = (self_header.magic == SCE_MAGIC);

    if (is_self) {
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
    }

    const uint8_t *const elf_bytes = is_self ? (image_bytes + self_header.elf_offset) : image_bytes;
    const Elf32_Ehdr &elf = *reinterpret_cast<const Elf32_Ehdr *>(elf_bytes);
    const uint32_t module_info_offset = elf.e_entry & 0x3fffffff;
    const Elf32_Phdr *const segments = is_self
        ? reinterpret_cast<const Elf32_Phdr *>(image_bytes + self_header.phdr_offset)
        : reinterpret_cast<const Elf32_Phdr *>(elf_bytes + elf.e_phoff);
    const segment_info *const seg_infos = is_self
        ? reinterpret_cast<const segment_info *>(image_bytes + self_header.section_info_offset)
        : nullptr;

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

    // log elf header
    LOG_TRACE("ELF Header: e_type: {}, e_machine: {}, e_version: {}, e_entry: {}, e_phoff: {}, e_shoff: {}, e_flags: {}, e_ehsize: {}, e_phentsize: {}, e_phnum: {}, e_shentsize: {}, e_shnum: {}, e_shstrndx: {}",
        log_hex(elf.e_type), log_hex(elf.e_machine), log_hex(elf.e_version), log_hex(elf.e_entry), log_hex(elf.e_phoff), log_hex(elf.e_shoff), log_hex(elf.e_flags), log_hex(elf.e_ehsize), log_hex(elf.e_phentsize), log_hex(elf.e_phnum), log_hex(elf.e_shentsize), log_hex(elf.e_shnum), log_hex(elf.e_shstrndx));

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

    if (is_self) {
        LOG_DEBUG_IF(LOG_MODULE_LOADING, "Loading SELF at {}... (ELF type: {}, self_filesize: {}, self_offset: {}, module_info_offset: {})", self_path, log_hex(elf.e_type), log_hex(self_header.self_filesize), log_hex(self_header.self_offset), log_hex(module_info_offset));
    } else {
        LOG_DEBUG_IF(LOG_MODULE_LOADING, "Loading ELF at {}... (ELF type: {}, module_info_offset: {})", self_path, log_hex(elf.e_type), log_hex(module_info_offset));
    }

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
        // A SELF says where each segment is in the segment info, and the
        // program header's p_offset describes the ELF the SELF was made from
        // rather than the SELF itself. The two agree only when the whole ELF
        // was embedded at header_len, which is what fake SELFs used to do and
        // no real one does.
        const uint8_t *const seg_bytes = is_self
            ? (image_bytes + seg_infos[seg_index].offset)
            : (elf_bytes + seg_header.p_offset);

        const auto uncompress_segment = [&](void *dst) {
            unsigned long dest_bytes = seg_header.p_filesz;
            const uint8_t *const compressed_segment_bytes = image_bytes + seg_infos[seg_index].offset;
            int res = mz_uncompress(static_cast<unsigned char *>(dst), &dest_bytes, compressed_segment_bytes, static_cast<mz_ulong>(seg_infos[seg_index].length));
            assert(res == MZ_OK);
        };

        LOG_DEBUG_IF(LOG_MODULE_LOADING, "    [{}] (p_type: {}): p_offset: {}, p_vaddr: {}, p_paddr: {}, p_filesz: {}, p_memsz: {}, p_flags: {}, p_align: {}", get_seg_header_string(seg_header.p_type), log_hex(seg_header.p_type), log_hex(seg_header.p_offset), log_hex(seg_header.p_vaddr), log_hex(seg_header.p_paddr), log_hex(seg_header.p_filesz), log_hex(seg_header.p_memsz), log_hex(seg_header.p_flags), log_hex(seg_header.p_align));

        if (is_self && seg_infos[seg_index].encryption != 2) { // 0 should also be valid?
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

                segment_address = try_alloc_at(mem, seg_header.p_vaddr, seg_header.p_memsz, alloc_name.c_str());

                if (!segment_address) {
                    if (isRelocatable) { // Try allocating somewhere else
                        segment_address = alloc(mem, seg_header.p_memsz, alloc_name.c_str());
                    }

                    if (!segment_address) {
                        LOG_CRITICAL("Loading {} ELF {} failed: Could not allocate {} bytes @ {} for segment {}.", (isRelocatable) ? "relocatable" : "fixed", self_path, log_hex(seg_header.p_memsz), log_hex(seg_header.p_vaddr), seg_index);
                        free_all_segments(mem, segment_reloc_info);
                        return SCE_KERNEL_ERROR_NO_MEMORY; // TODO is this correct?
                    }
                }

                const Ptr<uint8_t> seg_ptr(segment_address);
                if (is_self && seg_infos[seg_index].compression == 2) {
                    uncompress_segment(seg_ptr.get(mem));
                } else {
                    memcpy(seg_ptr.get(mem), seg_bytes, seg_header.p_filesz);
                }

                segment_reloc_info[seg_index] = { segment_address, seg_header.p_vaddr, seg_header.p_memsz };
            }
        } else if (seg_header.p_type == PT_SCE_RELA) {
            const void *reloc_data = seg_bytes;
            std::unique_ptr<uint8_t[]> uncompressed;

            if (is_self && seg_infos[seg_index].compression == 2) {
                uncompressed = std::make_unique<uint8_t[]>(seg_header.p_filesz);
                uncompress_segment(uncompressed.get());
                reloc_data = uncompressed.get();
            }

            if (!relocate(reloc_data, seg_header.p_filesz, segment_reloc_info, mem)) {
                return -1;
            }
        } else if ((seg_header.p_type == PT_SCE_COMMENT) || (seg_header.p_type == PT_SCE_VERSION)
            || (seg_header.p_type == PT_ARM_EXIDX) /* TODO: this may be important and require being loaded */) {
            LOG_INFO("{}: Skipping special segment {}...", self_path, log_hex(seg_header.p_type));
        } else {
            LOG_CRITICAL("{}: Skipping segment with unknown p_type {}!", self_path, log_hex(seg_header.p_type));
        }
    }

    if (kernel.debugger.dump_elfs) {
        const uint8_t *dump_begin = is_self ? (image_bytes + self_header.header_len) : elf_bytes;
        const uint8_t *dump_end;

        if (is_self) {
            dump_end = image_bytes + self_header.self_filesize;
        } else {
            size_t elf_size = 0;
            auto dump_segments = reinterpret_cast<const Elf32_Phdr *>(elf_bytes + elf.e_phoff);
            for (const auto &[seg_index, segment] : segment_reloc_info) {
                uint8_t *seg_bytes = Ptr<uint8_t>(segment.addr).get(mem);
                elf_size = std::max(elf_size, static_cast<size_t>(dump_segments[seg_index].p_offset) + static_cast<size_t>(dump_segments[seg_index].p_filesz));
            }
            dump_end = elf_bytes + elf_size;
        }

        std::vector<uint8_t> dump_elf(dump_begin, dump_end);
        if (is_self) {
            dump_elf.resize(self_header.elf_filesize);
        }

        Elf32_Phdr *dump_segments = reinterpret_cast<Elf32_Phdr *>(dump_elf.data() + elf.e_phoff);
        uint16_t last_index = 0;
        for (const auto &[seg_index, segment] : segment_reloc_info) {
            uint8_t *seg_bytes = Ptr<uint8_t>(segment.addr).get(mem);
            memcpy(dump_elf.data() + dump_segments[seg_index].p_offset, seg_bytes, dump_segments[seg_index].p_filesz);
            dump_segments[seg_index].p_vaddr = segment.addr;
            last_index = std::max(seg_index, last_index);
        }
        fs::create_directories(dump_path);
        const auto start = dump_segments[0].p_vaddr;
        const auto end = dump_segments[last_index].p_vaddr + dump_segments[last_index].p_filesz;
        const auto elf_name = fs::path(self_path).filename().stem().string();
        const auto filename = dump_path / fmt::format("{}-{}_{}.elf", log_hex_full(start), log_hex_full(end), elf_name);
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

    kernelModuleInfo->info_segment_address = module_info_segment_address;
    kernelModuleInfo->info_offset = module_info_offset;
    kernelModuleInfo->flags = flags;
    kernelModuleInfo->set_state(ModuleState::loaded);
    // The app info starts with the program authority ID
    if (is_self)
        memcpy(&kernelModuleInfo->auth_id, image_bytes + self_header.appinfo_offset, sizeof(kernelModuleInfo->auth_id));

    auto *sceKernelModuleInfo = &kernelModuleInfo->info;
    sceKernelModuleInfo->size = sizeof(*sceKernelModuleInfo);
    strncpy(sceKernelModuleInfo->module_name, module_info->name, 28);
    // unk28
    if (module_info->module_start != 0xffffffff && module_info->module_start != 0)
        sceKernelModuleInfo->start_entry = module_info_segment_address + module_info->module_start;
    // unk30
    if (module_info->module_stop != 0xffffffff && module_info->module_stop != 0)
        sceKernelModuleInfo->stop_entry = module_info_segment_address + module_info->module_stop;

    const auto seg_ptr = [&](uint32_t offset) { return Ptr<const void>(offset ? module_info_segment_address.address() + offset : 0); };
    sceKernelModuleInfo->exidx_top = seg_ptr(module_info->exidx_top);
    sceKernelModuleInfo->exidx_btm = seg_ptr(module_info->exidx_end);
    sceKernelModuleInfo->extab_top = seg_ptr(module_info->extab_top);
    sceKernelModuleInfo->extab_btm = seg_ptr(module_info->extab_end);

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
        segment.perms = segments[segment_index].p_flags & 0x7; // PF_R | PF_W | PF_X
        segment.vaddr = it->second.addr;
        segment.memsz = segments[segment_index].p_memsz;
        segment.filesz = segments[segment_index].p_filesz;
    }

    LOG_INFO("Linking {} {}...", is_self ? "SELF" : "ELF", self_path);
    if (self_path.contains("eboot.bin"))
        LOG_INFO("eboot.bin module NID: {}", log_hex(module_info->module_nid));

    load_exports(*kernelModuleInfo, *module_info, module_info_segment_address, kernel, mem);

    if (!load_imports(*kernelModuleInfo, *module_info, module_info_segment_address, segment_reloc_info, kernel, mem)) {
        load_imports(*kernelModuleInfo, *module_info, module_info_segment_address, segment_reloc_info, kernel, mem, true);
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
    // A start or stop that failed in the middle of the libraries leaves the module partly linked,
    // which goes with it so that nothing points into freed memory
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        link_imports(kernel, mem, module, false);
        for (ExportLibrary &library : module.libraries) {
            if (library.registered)
                unregister_library(kernel, mem, library, true);
        }
    }

    SegmentInfosForReloc segment_reloc_info;
    for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
        const auto &segment = module.info.segments[i];
        if (segment.size == 0)
            continue;

        segment_reloc_info[i] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
    }

    if (!load_imports(module, *module_info, module.info_segment_address, segment_reloc_info, kernel, mem, true)) {
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
        // Another copy of the file may have the NID now
        if (const auto it = kernel.module_uid_by_nid.find(mod_nid); it != kernel.module_uid_by_nid.end() && it->second == static_cast<uint32_t>(module.info.modid))
            kernel.module_uid_by_nid.erase(it);
    }

    return 0;
}
