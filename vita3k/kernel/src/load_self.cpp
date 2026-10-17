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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>

static constexpr uint32_t NID_MODULE_STOP = 0x79F8E492;
static constexpr uint32_t NID_MODULE_EXIT = 0x913482A9;
static constexpr uint32_t NID_MODULE_START = 0x935CD196;
static constexpr uint32_t NID_MODULE_INFO = 0x6C2224BA;
static constexpr uint32_t NID_SYSLYB = 0x936c8a78;
static constexpr uint32_t NID_PROCESS_PARAM = 0x70FBA1E7;

static constexpr bool LOG_MODULE_LOADING = false;

struct VarImportsHeader {
    uint32_t unk : 4; // Must be zero
    uint32_t reloc_data_size : 24; // Size of Relocation data in bytes, includes this header.
    uint32_t unk2 : 4; // Must be zero
};
static_assert(sizeof(VarImportsHeader) == sizeof(uint32_t));

static Address allocate_variable_stub(MemState &mem, uint32_t nid) {
    const auto name = fmt::format("Stub var import reloc symval, NID {} ({})", log_hex(nid), import_name(nid));
    const Address address = alloc(mem, sizeof(uint32_t), name.c_str());
    if (!address) {
        LOG_ERROR("Failed to allocate variable import stub for NID {}", log_hex(nid));
        return 0;
    }
    *Ptr<uint32_t>(address).get(mem) = 0xDEADBEEF;
    return address;
}

static void release_unused_variable_stub(const ModuleBindingView &bindings, MemState &mem, uint32_t nid) {
    const auto stub = bindings.variable_stubs.find(nid);
    const auto exported = bindings.exports.find(nid);
    if (bindings.variable_imports.count(nid) == 0 && bindings.function_imports.count(nid) == 0 && stub != bindings.variable_stubs.end()
        && exported != bindings.exports.end() && exported->second == stub->second) {
        free(mem, stub->second);
        bindings.variable_stubs.erase(stub);
        bindings.exports.erase(exported);
    }
}

struct ImportLinkJournal {
    std::vector<FuncBindingInfos::iterator> functions;
    std::vector<VarBindingInfos::iterator> variables;
    std::vector<uint32_t> allocated_stubs;
};

struct VariableStubPool {
    explicit VariableStubPool(MemState &memory)
        : mem(memory) {}
    ~VariableStubPool() {
        for (const auto &[nid, address] : stubs)
            free(mem, address);
    }
    bool reserve(uint32_t nid) {
        if (stubs.contains(nid))
            return true;
        const Address address = allocate_variable_stub(mem, nid);
        if (!address)
            return false;
        stubs.emplace(nid, address);
        return true;
    }
    Address take(uint32_t nid) {
        const auto it = stubs.find(nid);
        if (it == stubs.end()) {
            LOG_ERROR("No reserved variable import stub for NID {}", log_hex(nid));
            return 0;
        }
        const Address address = it->second;
        stubs.erase(it);
        return address;
    }
    MemState &mem;
    std::map<uint32_t, Address> stubs;
};

static void rollback_imports(KernelState &kernel, MemState &mem, const ImportLinkJournal &journal) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (const auto it : journal.functions)
        bindings.function_imports.erase(it);
    for (const auto it : journal.variables)
        bindings.variable_imports.erase(it);
    for (const uint32_t nid : journal.allocated_stubs)
        release_unused_variable_stub(bindings, mem, nid);
}

static bool segment_contains(const SegmentInfosForReloc &segments, uint64_t address, uint64_t size);
static bool validate_import_relocations(Address address, const SegmentInfosForReloc &segments, const MemState &mem);

static bool load_var_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, uint32_t module_id, ImportLinkJournal &journal,
    const std::vector<std::vector<uint32_t>> *snapshots = nullptr, VariableStubPool *stub_pool = nullptr) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (!snapshots && !validate_import_relocations(entry.address(), segments, mem))
            return false;

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}). entry: {}, *entry: {}", log_hex(nid), name, entry, log_hex(*entry.get(mem)));
        }

        VarImportsHeader *const var_reloc_header = entry.cast<VarImportsHeader>().get(mem);
        const auto var_reloc_entries = static_cast<void *>(var_reloc_header + 1);
        const uint32_t reloc_size = snapshots                                ? static_cast<uint32_t>((*snapshots)[i].size() * sizeof(uint32_t))
            : (var_reloc_header->reloc_data_size > sizeof(VarImportsHeader)) ? (var_reloc_header->reloc_data_size - sizeof(VarImportsHeader))
                                                                             : 0;
        if (reloc_size % sizeof(uint32_t) != 0
            || !segment_contains(segments, static_cast<uint64_t>(entry.address()) + sizeof(VarImportsHeader), reloc_size)) {
            LOG_ERROR("Invalid variable import relocation size at {}", log_hex(entry.address()));
            return false;
        }

        const char *const name = import_name(nid);
        Address export_address;
        const ExportNids::iterator export_address_it = bindings.exports.find(nid);
        if (export_address_it != bindings.exports.end()) {
            export_address = export_address_it->second;
        } else {
            constexpr auto STUB_SYMVAL = 0xDEADBEEF;
            LOG_DEBUG("\tNID NOT FOUND {} ({}) at {}, setting to stub value {}", log_hex(nid), name, log_hex(entry.address()), log_hex(STUB_SYMVAL));

            export_address = stub_pool ? stub_pool->take(nid) : allocate_variable_stub(mem, nid);
            if (!export_address)
                return false;

            // Use same stub for other var imports
            bindings.exports.emplace(nid, export_address);
            bindings.variable_stubs[nid] = export_address;
            journal.allocated_stubs.push_back(nid);
        }

        std::vector<uint32_t> relocation_entries = snapshots ? (*snapshots)[i] : std::vector<uint32_t>(reloc_size / sizeof(uint32_t));
        if (!snapshots && reloc_size)
            std::memcpy(relocation_entries.data(), var_reloc_entries, reloc_size);
        const auto binding = bindings.variable_imports.emplace(nid, VarBindingInfo{ var_reloc_entries, reloc_size, module_id, segments, std::move(relocation_entries) });
        journal.variables.push_back(binding);
        if (reloc_size)
            if (!relocate(binding->second.relocation_entries.data(), reloc_size, segments, mem, true, export_address))
                return false;
    }

    return true;
}

static bool unload_var_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem, uint32_t module_id) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        VarImportsHeader *const var_reloc_header = reinterpret_cast<VarImportsHeader *>(entry.get(mem));
        const auto var_reloc_entries = static_cast<void *>(var_reloc_header + 1);
        // remove the binding info from the map
        auto range = bindings.variable_imports.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.entries == var_reloc_entries && it->second.module_nid == module_id) {
                bindings.variable_imports.erase(it);
                break;
            }
        }
        release_unused_variable_stub(bindings, mem, nid);
    }

    return true;
}

static bool load_func_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, uint32_t library_nid, const SegmentInfosForReloc &segments, KernelState &kernel, const MemState &mem, ImportLinkJournal &journal,
    const std::vector<std::vector<uint32_t>> *snapshots = nullptr) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (kernel.debugger.log_imports) {
            const char *const name = import_name(nid);
            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }

        // Fall back to the plain NID for exports registered without library information.
        Address func_address = 0;
        if (const auto it = bindings.library_exports.find(lib_export_key(library_nid, nid)); it != bindings.library_exports.end()) {
            func_address = it->second;
        } else if (const auto nid_it = bindings.exports.find(nid); nid_it != bindings.exports.end()) {
            func_address = nid_it->second;
        }
        uint32_t *const stub = entry.get(mem);

        journal.functions.push_back(bindings.function_imports.emplace(nid, FuncBindingInfo{ entry.address(), library_nid }));
        if (!func_address) {
            stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
            stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
            stub[2] = nid; // Our interrupt hook will read this.
        } else {
            stub[0] = encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(func_address), 12);
            stub[1] = encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(func_address >> 16), 12);
            stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        }
        if (snapshots) {
            const auto &relocations = (*snapshots)[i];
            if (!relocations.empty() && !relocate(relocations.data(), static_cast<uint32_t>(relocations.size() * sizeof(uint32_t)), segments, mem, true, entry.address()))
                return false;
        } else if (stub[3]) { // if function's associated reftable exists
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

static bool unload_func_imports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        // remove the stub from the table
        auto range = bindings.function_imports.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.entry_address == entry.address()) {
                bindings.function_imports.erase(it);
                break;
            }
        }
        release_unused_variable_stub(bindings, mem, nid);
    }
    return true;
}

static bool segment_contains(const SegmentInfosForReloc &segments, uint64_t address, uint64_t size) {
    for (const auto &[index, segment] : segments) {
        if (address >= segment.addr && address - segment.addr <= segment.size && size <= segment.size - (address - segment.addr))
            return true;
    }
    return false;
}

static bool validate_exports(const sce_module_info_raw &module, Ptr<const void> segment_address, const SegmentInfosForReloc &segments, const MemState &mem) {
    if (module.export_top == module.export_end)
        return true;
    const uint64_t table_address = static_cast<uint64_t>(segment_address.address()) + module.export_top;
    if (module.export_top > module.export_end
        || !segment_contains(segments, table_address, module.export_end - module.export_top)) {
        LOG_ERROR("Module export table is outside the module segments");
        return false;
    }

    const auto *base = segment_address.cast<const uint8_t>().get(mem);
    uint32_t offset = module.export_top;
    while (offset < module.export_end) {
        const uint32_t remaining = module.export_end - offset;
        if (remaining < sizeof(sce_module_exports_raw)) {
            LOG_ERROR("Truncated module export record");
            return false;
        }
        sce_module_exports_raw exports;
        std::memcpy(&exports, base + offset, sizeof(exports));
        if (exports.size < sizeof(exports) || exports.size > remaining) {
            LOG_ERROR("Invalid or truncated module export record");
            return false;
        }
        const uint64_t count = static_cast<uint64_t>(exports.num_syms_funcs) + exports.num_syms_vars;
        const uint64_t table_size = count * sizeof(uint32_t);
        if (table_size && (!segment_contains(segments, exports.nid_table, table_size) || !segment_contains(segments, exports.entry_table, table_size))) {
            LOG_ERROR("Export NID or entry table is outside the module segments");
            return false;
        }
        const auto *nids = Ptr<const uint32_t>(exports.nid_table).get(mem);
        const auto *entries = Ptr<const uint32_t>(exports.entry_table).get(mem);
        for (size_t i = exports.num_syms_funcs; i < count; ++i) {
            if (nids[i] == NID_PROCESS_PARAM && !segment_contains(segments, entries[i], sizeof(SceProcessParam))) {
                LOG_ERROR("Process parameters are outside the module segments");
                return false;
            }
        }
        offset += exports.size;
    }
    return true;
}

static bool validate_late_variable_bindings(const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, const MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    const auto *base = segment_address.cast<const uint8_t>().get(mem);
    for (uint32_t offset = module.export_top; offset < module.export_end;) {
        sce_module_exports_raw exports;
        std::memcpy(&exports, base + offset, sizeof(exports));
        const auto *nids = Ptr<const uint32_t>(exports.nid_table).get(mem);
        for (size_t i = 0; i < exports.num_syms_vars; ++i) {
            const uint32_t nid = nids[exports.num_syms_funcs + i];
            if (nid == NID_PROCESS_PARAM || nid == NID_MODULE_INFO || nid == NID_SYSLYB)
                continue;
            const auto range = bindings.variable_imports.equal_range(nid);
            for (auto it = range.first; it != range.second; ++it) {
                const auto &binding = it->second;
                if (binding.size && !binding.segments.empty()
                    && (binding.relocation_entries.size() * sizeof(uint32_t) != binding.size
                        || !validate_variable_relocations(binding.relocation_entries.data(), binding.size, binding.segments))) {
                    LOG_ERROR("Invalid late variable binding for NID {}", log_hex(nid));
                    return false;
                }
            }
        }
        offset += exports.size;
    }
    return true;
}

static bool validate_import_relocations(Address address, const SegmentInfosForReloc &segments, const MemState &mem) {
    if (!segment_contains(segments, address, sizeof(VarImportsHeader))) {
        LOG_ERROR("Import relocation header {} is outside the module segments", log_hex(address));
        return false;
    }
    const auto *header = Ptr<const VarImportsHeader>(address).get(mem);
    const uint32_t size = header->reloc_data_size > sizeof(VarImportsHeader) ? header->reloc_data_size - sizeof(VarImportsHeader) : 0;
    if (!segment_contains(segments, static_cast<uint64_t>(address) + sizeof(VarImportsHeader), size)) {
        LOG_ERROR("Import relocations at {} extend outside the module segments", log_hex(address));
        return false;
    }
    return validate_variable_relocations(header + 1, size, segments);
}

static bool load_imports(const sce_module_info_raw &module, Ptr<const void> segment_address, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, ImportLinkJournal &journal, bool is_unload = false, bool validate_only = false) {
    if (module.import_top == module.import_end)
        return true;
    if (validate_only && (module.import_top > module.import_end || !segment_contains(segments, static_cast<uint64_t>(segment_address.address()) + module.import_top, module.import_end - module.import_top))) {
        LOG_ERROR("Module import table is outside the module segments");
        return false;
    }
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_imports_raw *const imports_begin = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_top);
    const sce_module_imports_raw *const imports_end = reinterpret_cast<const sce_module_imports_raw *>(base + module.import_end);

    for (const sce_module_imports_raw *imports = imports_begin; imports < imports_end; imports = reinterpret_cast<const sce_module_imports_raw *>(reinterpret_cast<const uint8_t *>(imports) + imports->size)) {
        if (validate_only) {
            const auto remaining = reinterpret_cast<const uint8_t *>(imports_end) - reinterpret_cast<const uint8_t *>(imports);
            if (remaining < sizeof(uint16_t)) {
                LOG_ERROR("Truncated module import record");
                return false;
            }
            uint16_t size;
            std::memcpy(&size, imports, sizeof(size));
            if ((size != 0x24 && size != 0x34) || remaining < size) {
                LOG_ERROR("Invalid or truncated module import record");
                return false;
            }
            if (imports->num_syms_tls_vars != 0) {
                LOG_ERROR("TLS variable imports are not supported");
                return false;
            }
        }
        assert(imports->num_syms_tls_vars == 0);

        Address library_name{};
        uint32_t library_nid{};
        Address func_nid_table{};
        Address func_entry_table{};
        Address var_nid_table{};
        Address var_entry_table{};

        if (imports->size == 0x24) {
            auto short_imports = reinterpret_cast<const sce_module_imports_short_raw *>(imports);
            library_nid = short_imports->library_nid;
            library_name = short_imports->library_name;
            func_nid_table = short_imports->func_nid_table;
            func_entry_table = short_imports->func_entry_table;
            var_nid_table = short_imports->var_nid_table;
            var_entry_table = short_imports->var_entry_table;
        } else if (imports->size == 0x34) {
            auto long_imports = imports;
            library_nid = long_imports->library_nid;
            library_name = long_imports->library_name;
            func_nid_table = long_imports->func_nid_table;
            func_entry_table = long_imports->func_entry_table;
            var_nid_table = long_imports->var_nid_table;
            var_entry_table = long_imports->var_entry_table;
        }

        if (validate_only) {
            const uint64_t func_size = static_cast<uint64_t>(imports->num_syms_funcs) * sizeof(uint32_t);
            const uint64_t var_size = static_cast<uint64_t>(imports->num_syms_vars) * sizeof(uint32_t);
            if ((func_size && (!segment_contains(segments, func_nid_table, func_size) || !segment_contains(segments, func_entry_table, func_size)))
                || (var_size && (!segment_contains(segments, var_nid_table, var_size) || !segment_contains(segments, var_entry_table, var_size)))) {
                LOG_ERROR("Import NID or entry table is outside the module segments");
                return false;
            }
            const auto *func_entries = Ptr<const Ptr<uint32_t>>(func_entry_table).get(mem);
            for (size_t i = 0; i < imports->num_syms_funcs; ++i) {
                const Address entry = func_entries[i].address();
                if (!segment_contains(segments, entry, 4 * sizeof(uint32_t))) {
                    LOG_ERROR("Function import stub {} is outside the module segments", log_hex(entry));
                    return false;
                }
                const Address relocations = Ptr<const uint32_t>(entry).get(mem)[3];
                if (relocations && !validate_import_relocations(relocations, segments, mem))
                    return false;
            }
            const auto *var_entries = Ptr<const Ptr<uint32_t>>(var_entry_table).get(mem);
            for (size_t i = 0; i < imports->num_syms_vars; ++i) {
                if (!validate_import_relocations(var_entries[i].address(), segments, mem))
                    return false;
            }
            continue;
        }

        std::string lib_name;
        if (kernel.debugger.log_imports) {
            lib_name = Ptr<const char>(library_name).get(mem);
            LOG_INFO("Loading func imports from {}", lib_name);
        }

        const uint32_t *const nids = Ptr<const uint32_t>(func_nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(func_entry_table).get(mem);

        const size_t num_syms_funcs = imports->num_syms_funcs;
        if (!is_unload && !load_func_imports(nids, entries, num_syms_funcs, library_nid, segments, kernel, mem, journal))
            return false;
        if (is_unload && !unload_func_imports(nids, entries, num_syms_funcs, kernel, mem))
            return false;

        const uint32_t *const var_nids = Ptr<const uint32_t>(var_nid_table).get(mem);
        const Ptr<uint32_t> *const var_entries = Ptr<Ptr<uint32_t>>(var_entry_table).get(mem);

        const auto var_count = imports->num_syms_vars;

        if (kernel.debugger.log_imports && var_count > 0)
            LOG_INFO("Loading var imports from {}", lib_name);

        if (!is_unload && !load_var_imports(var_nids, var_entries, var_count, segments, kernel, mem, module.module_nid, journal))
            return false;
        if (is_unload && !unload_var_imports(var_nids, var_entries, var_count, kernel, mem, module.module_nid))
            return false;
    }

    return true;
}

static bool load_func_exports(SceKernelModuleInfo *kernel_module_info, const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, uint32_t library_nid, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
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

        // Only the library that actually took the plain NID entry may drop it again.
        if (bindings.exports.emplace(nid, entry.address()).second)
            bindings.export_owners.insert_or_assign(nid, library_nid);
        bindings.library_exports.insert_or_assign(lib_export_key(library_nid, nid), entry.address());
        // substitute supervisor calls to direct function calls in loaded modules
        auto range = bindings.function_imports.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            // A same-named export from an unrelated library must not hijack these importers.
            if (it->second.library_nid != library_nid)
                continue;
            auto address = it->second.entry_address;
            uint32_t *const stub = Ptr<uint32_t>(address).get(mem);
            stub[0] = encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(entry.address()), 12);
            stub[1] = encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(entry.address() >> 16), 12);
            stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
            kernel.invalidate_jit_cache(mem, address, 3 * sizeof(uint32_t));
        }

        if (kernel.debugger.log_exports) {
            const char *const name = import_name(nid);

            LOG_DEBUG("\tNID {} ({}) at {}", log_hex(nid), name, log_hex(entry.address()));
        }
    }

    return true;
}

// An import stub resolved to an export branches through MOVW/MOVT of its address.
static bool stub_targets(const uint32_t *stub, Address address) {
    return stub[0] == encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(address), 12)
        && stub[1] == encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(address >> 16), 12);
}

static void rebind_function_imports_target(KernelState &kernel, MemState &mem, const ModuleBindingView &bindings, uint32_t nid, Address from, Address to) {
    const auto range = bindings.function_imports.equal_range(nid);
    for (auto it = range.first; it != range.second; ++it) {
        const Address address = it->second.entry_address;
        uint32_t *stub = Ptr<uint32_t>(address).get(mem);
        if (!stub_targets(stub, from))
            continue;
        stub[0] = encode_arm_inst(INSTRUCTION_MOVW, static_cast<uint16_t>(to), 12);
        stub[1] = encode_arm_inst(INSTRUCTION_MOVT, static_cast<uint16_t>(to >> 16), 12);
        stub[2] = encode_arm_inst(INSTRUCTION_BRANCH, 0, 12);
        kernel.invalidate_jit_cache(mem, address, 3 * sizeof(uint32_t));
    }
}

static bool unload_func_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, uint32_t library_nid, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];

        if (nid == NID_MODULE_START || nid == NID_MODULE_STOP || nid == NID_MODULE_EXIT)
            continue;

        const Address unloaded_address = entries[i].address();
        // Another module may have taken this library key since, and its export is still live.
        if (const auto lib_it = bindings.library_exports.find(lib_export_key(library_nid, nid)); lib_it != bindings.library_exports.end() && lib_it->second == unloaded_address)
            bindings.library_exports.erase(lib_it);
        // Redirects move the address but not the ownership, so only the owner may drop the plain entry.
        if (const auto owner_it = bindings.export_owners.find(nid); owner_it != bindings.export_owners.end() && owner_it->second == library_nid) {
            bindings.exports.erase(nid);
            bindings.export_owners.erase(owner_it);
        }
        // invalidate all lle nid calls
        auto range = bindings.function_imports.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            Address entry = it->second.entry_address;
            uint32_t *stub = Ptr<uint32_t>(entry).get(mem);
            // A stub bound through the plain NID fallback names another library but still branches here.
            if (it->second.library_nid != library_nid && !stub_targets(stub, unloaded_address))
                continue;

            stub[0] = 0xef000000; // svc #0 - Call our interrupt hook.
            stub[1] = 0xe1a0f00e; // mov pc, lr - Return to the caller.
            stub[2] = nid; // Our interrupt hook will read this.
            kernel.invalidate_jit_cache(mem, entry, 3 * sizeof(uint32_t));
        }
    }

    return true;
}

static bool load_var_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, Address module_segment, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];
        const Ptr<uint32_t> entry = entries[i];

        if (nid == NID_PROCESS_PARAM) {
            if (!kernel.get_process_param(mem))
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
        auto nid_it = bindings.exports.find(nid);
        const bool had_previous_export = nid_it != bindings.exports.end();
        if (had_previous_export) {
            LOG_DEBUG("Found previously not found variable. nid:{}, new_entry_point:{}", log_hex(nid), log_hex(entry.address()));
            old_entry_address = nid_it->second;
        }
        const auto stub = bindings.variable_stubs.find(nid);
        const bool old_is_stub = stub != bindings.variable_stubs.end() && stub->second == old_entry_address;
        auto [stack, inserted] = bindings.variable_exports.try_emplace(nid);
        if (inserted && had_previous_export && !old_is_stub)
            stack->second.fallback = old_entry_address;
        stack->second.layers.push_back({ module_segment, entry.address() });
        bindings.exports[nid] = entry.address();

        auto range = bindings.variable_imports.equal_range(nid);
        for (auto j = range.first; j != range.second; ++j) {
            auto &var_binding_info = j->second;
            if (var_binding_info.size == 0)
                continue;

            const auto &seg = var_binding_info.segments;

            // Note: We make the assumption that variables are not imported into executable code (wouldn't make a lot of sense)
            // If this is not the case, uncomment the following
            /* if (!seg.empty()) {
                for (const auto &[key, value] : seg) {
                     kernel.invalidate_jit_cache(mem, value.addr, value.size);
                }
            }*/

            if (!seg.empty()) {
                if (!relocate(var_binding_info.relocation_entries.data(), var_binding_info.size, seg, mem, true, entry.address())) {
                    LOG_ERROR("Failed to relocate late binding info");
                }
            }
        }
        if (had_previous_export)
            rebind_function_imports_target(kernel, mem, bindings, nid, old_entry_address, entry.address());
        if (old_is_stub && old_entry_address != entry.address()) {
            free(mem, old_entry_address);
            bindings.variable_stubs.erase(stub);
        }
    }
    return true;
}

static bool unload_var_exports(const uint32_t *nids, const Ptr<uint32_t> *entries, size_t count, Address module_segment, KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t nid = nids[i];

        if (nid == NID_PROCESS_PARAM || nid == NID_MODULE_INFO || nid == NID_SYSLYB)
            continue;

        const auto stack = bindings.variable_exports.find(nid);
        if (stack == bindings.variable_exports.end()) {
            LOG_ERROR("No variable export stack for NID {}", log_hex(nid));
            return false;
        }
        auto &layers = stack->second.layers;
        const auto layer = std::find_if(layers.begin(), layers.end(), [&](const VariableExportLayer &candidate) {
            return candidate.module_segment == module_segment && candidate.address == entries[i].address();
        });
        if (layer == layers.end()) {
            LOG_ERROR("Variable export NID {} is not owned by module segment {}", log_hex(nid), log_hex(module_segment));
            return false;
        }
        if (std::next(layer) != layers.end()) {
            layers.erase(layer);
            continue;
        }

        std::optional<Address> replacement;
        if (layers.size() > 1) {
            replacement = layers[layers.size() - 2].address;
        } else if (stack->second.fallback) {
            replacement = *stack->second.fallback;
        } else if (bindings.variable_imports.count(nid) != 0 || bindings.function_imports.count(nid) != 0) {
            const Address stub_address = allocate_variable_stub(mem, nid);
            if (!stub_address)
                return false;
            replacement = stub_address;
            bindings.variable_stubs[nid] = stub_address;
        }
        layers.erase(layer);
        if (layers.empty())
            bindings.variable_exports.erase(stack);

        if (!replacement) {
            bindings.exports.erase(nid);
            continue;
        }
        bindings.exports[nid] = *replacement;
        rebind_function_imports_target(kernel, mem, bindings, nid, entries[i].address(), *replacement);

        auto range = bindings.variable_imports.equal_range(nid);
        for (auto it = range.first; it != range.second; ++it) {
            auto &var_binding_info = it->second;
            if (var_binding_info.size == 0)
                continue;

            const auto &seg = var_binding_info.segments;

            if (!seg.empty()) {
                if (!relocate(var_binding_info.relocation_entries.data(), var_binding_info.size, seg, mem, true, *replacement)) {
                    LOG_ERROR("Failed to relocate late binding info");
                }
            }
        }
    }
    return true;
}

static bool load_exports(SceKernelModuleInfo *kernel_module_info, const sce_module_info_raw &module, Ptr<const void> segment_address, KernelState &kernel, MemState &mem, bool is_unload = false) {
    if (module.export_top == module.export_end)
        return true;
    const uint8_t *const base = segment_address.cast<const uint8_t>().get(mem);
    const sce_module_exports_raw *const exports_begin = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_top);
    const sce_module_exports_raw *const exports_end = reinterpret_cast<const sce_module_exports_raw *>(base + module.export_end);

    for (const sce_module_exports_raw *exports = exports_begin; exports < exports_end; exports = reinterpret_cast<const sce_module_exports_raw *>(reinterpret_cast<const uint8_t *>(exports) + exports->size)) {
        const char *const lib_name = Ptr<const char>(exports->library_name).get(mem);

        if (kernel.debugger.log_exports)
            LOG_INFO("Loading func exports from {}", lib_name ? lib_name : "unknown");

        const uint32_t *const nids = Ptr<const uint32_t>(exports->nid_table).get(mem);
        const Ptr<uint32_t> *const entries = Ptr<Ptr<uint32_t>>(exports->entry_table).get(mem);
        if (!is_unload && !load_func_exports(kernel_module_info, nids, entries, exports->num_syms_funcs, exports->library_nid, kernel, mem))
            return false;
        if (is_unload && !unload_func_exports(nids, entries, exports->num_syms_funcs, exports->library_nid, kernel, mem))
            return false;

        const auto var_count = exports->num_syms_vars;

        if (kernel.debugger.log_exports && var_count > 0) {
            LOG_INFO("Loading var exports from {}", lib_name ? lib_name : "unknown");
        }

        if (!is_unload && !load_var_exports(&nids[exports->num_syms_funcs], &entries[exports->num_syms_funcs], var_count, segment_address.address(), kernel, mem))
            return false;
        if (is_unload && !unload_var_exports(&nids[exports->num_syms_funcs], &entries[exports->num_syms_funcs], var_count, segment_address.address(), kernel, mem))
            return false;
    }

    return true;
}

static std::vector<uint32_t> copy_import_relocations(Address address, const MemState &mem) {
    if (!address)
        return {};
    const auto *header = Ptr<const VarImportsHeader>(address).get(mem);
    const uint32_t size = header->reloc_data_size > sizeof(VarImportsHeader) ? header->reloc_data_size - sizeof(VarImportsHeader) : 0;
    std::vector<uint32_t> result(size / sizeof(uint32_t));
    if (size)
        std::memcpy(result.data(), header + 1, size);
    return result;
}

static ModuleLinkageSnapshot snapshot_module_linkage(const sce_module_info_raw &module, Ptr<const void> segment_address, const MemState &mem) {
    ModuleLinkageSnapshot result;
    const auto *base = segment_address.cast<const uint8_t>().get(mem);
    for (uint32_t offset = module.export_top; offset < module.export_end;) {
        const auto *raw = reinterpret_cast<const sce_module_exports_raw *>(base + offset);
        ModuleExportSnapshot record;
        record.library_nid = raw->library_nid;
        record.function_count = raw->num_syms_funcs;
        const size_t count = record.function_count + raw->num_syms_vars;
        if (count) {
            const auto *nids = Ptr<const uint32_t>(raw->nid_table).get(mem);
            const auto *entries = Ptr<const Ptr<uint32_t>>(raw->entry_table).get(mem);
            record.nids.assign(nids, nids + count);
            record.entries.assign(entries, entries + count);
        }
        result.exports.push_back(std::move(record));
        offset += raw->size;
    }
    for (uint32_t offset = module.import_top; offset < module.import_end;) {
        const auto *raw = reinterpret_cast<const sce_module_imports_raw *>(base + offset);
        ModuleImportSnapshot record;
        Address func_nids, func_entries, var_nids, var_entries;
        if (raw->size == 0x24) {
            const auto *short_raw = reinterpret_cast<const sce_module_imports_short_raw *>(raw);
            record.library_nid = short_raw->library_nid;
            func_nids = short_raw->func_nid_table;
            func_entries = short_raw->func_entry_table;
            var_nids = short_raw->var_nid_table;
            var_entries = short_raw->var_entry_table;
        } else {
            record.library_nid = raw->library_nid;
            func_nids = raw->func_nid_table;
            func_entries = raw->func_entry_table;
            var_nids = raw->var_nid_table;
            var_entries = raw->var_entry_table;
        }
        if (raw->num_syms_funcs) {
            const auto *nids = Ptr<const uint32_t>(func_nids).get(mem);
            const auto *entries = Ptr<const Ptr<uint32_t>>(func_entries).get(mem);
            record.function_nids.assign(nids, nids + raw->num_syms_funcs);
            record.function_entries.assign(entries, entries + raw->num_syms_funcs);
            for (const auto entry : record.function_entries)
                record.function_relocations.push_back(copy_import_relocations(entry.get(mem)[3], mem));
        }
        if (raw->num_syms_vars) {
            const auto *nids = Ptr<const uint32_t>(var_nids).get(mem);
            const auto *entries = Ptr<const Ptr<uint32_t>>(var_entries).get(mem);
            record.variable_nids.assign(nids, nids + raw->num_syms_vars);
            record.variable_entries.assign(entries, entries + raw->num_syms_vars);
            for (const auto entry : record.variable_entries)
                record.variable_relocations.push_back(copy_import_relocations(entry.address(), mem));
        }
        result.imports.push_back(std::move(record));
        offset += raw->size;
    }
    return result;
}

static bool link_snapshot_imports(const ModuleLinkageSnapshot &snapshot, const SegmentInfosForReloc &segments, KernelState &kernel, MemState &mem, uint32_t module_nid, ImportLinkJournal &journal, VariableStubPool &pool) {
    for (const auto &record : snapshot.imports) {
        if (!load_func_imports(record.function_nids.data(), record.function_entries.data(), record.function_nids.size(), record.library_nid, segments, kernel, mem, journal, &record.function_relocations)
            || !load_var_imports(record.variable_nids.data(), record.variable_entries.data(), record.variable_nids.size(), segments, kernel, mem, module_nid, journal, &record.variable_relocations, &pool))
            return false;
    }
    return true;
}

static bool unlink_snapshot(const ModuleLinkageSnapshot &snapshot, Address module_segment, KernelState &kernel, MemState &mem, uint32_t module_nid) {
    for (const auto &record : snapshot.exports) {
        if (!unload_func_exports(record.nids.data(), record.entries.data(), record.function_count, record.library_nid, kernel, mem)
            || (record.nids.size() > record.function_count && !unload_var_exports(record.nids.data() + record.function_count, record.entries.data() + record.function_count, record.nids.size() - record.function_count, module_segment, kernel, mem)))
            return false;
    }
    for (const auto &record : snapshot.imports) {
        if (!unload_func_imports(record.function_nids.data(), record.function_entries.data(), record.function_nids.size(), kernel, mem)
            || !unload_var_imports(record.variable_nids.data(), record.variable_entries.data(), record.variable_nids.size(), kernel, mem, module_nid))
            return false;
    }
    return true;
}

/**
 * \return Negative on failure
 */
static SceUID load_self_impl(KernelState &kernel, MemState &mem, const void *self, const std::string &self_path, const fs::path &dump_path, bool capture_shared_initial_segments, const KernelModule *initial_source) {
    const std::lock_guard<std::mutex> linker_lock(kernel.module_linker_mutex);
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
            if (capture_shared_initial_segments && seg_header.p_memsz && seg_index >= MODULE_INFO_NUM_SEGMENTS) {
                LOG_ERROR("Cannot capture {}: loadable segment {} exceeds module information capacity", self_path, seg_index);
                free_all_segments(mem, segment_reloc_info);
                return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
            }
            if (seg_header.p_filesz > seg_header.p_memsz) {
                LOG_ERROR("Cannot load {}: segment {} has more file data than memory", self_path, seg_index);
                free_all_segments(mem, segment_reloc_info);
                return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
            }
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
                if (initial_source) {
                    memcpy(seg_ptr.get(mem), initial_source->shared_initial_segments[seg_index].data(), seg_header.p_memsz);
                } else if (is_self && seg_infos[seg_index].compression == 2) {
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
                LOG_ERROR("Failed to relocate segment {} of {}", seg_index, self_path);
                free_all_segments(mem, segment_reloc_info);
                return -1;
            }
        } else if ((seg_header.p_type == PT_SCE_COMMENT) || (seg_header.p_type == PT_SCE_VERSION)
            || (seg_header.p_type == PT_ARM_EXIDX) /* TODO: this may be important and require being loaded */) {
            LOG_INFO("{}: Skipping special segment {}...", self_path, log_hex(seg_header.p_type));
        } else {
            LOG_CRITICAL("{}: Skipping segment with unknown p_type {}!", self_path, log_hex(seg_header.p_type));
        }
    }

    if (kernel.debugger.dump_elfs && !initial_source) {
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
    const auto module_info_segment = segment_reloc_info.find(module_info_segment_index);
    if (module_info_segment == segment_reloc_info.end() || module_info_offset > module_info_segment->second.size
        || module_info_segment->second.size - module_info_offset < sizeof(sce_module_info_raw)) {
        LOG_ERROR("Invalid module information segment or offset in {}", self_path);
        free_all_segments(mem, segment_reloc_info);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }
    const Ptr<const uint8_t> module_info_segment_address(module_info_segment->second.addr);
    const uint8_t *const module_info_segment_bytes = module_info_segment_address.get(mem);
    const sce_module_info_raw module_info_copy = *reinterpret_cast<const sce_module_info_raw *>(module_info_segment_bytes + module_info_offset);
    const sce_module_info_raw *const module_info = &module_info_copy;
    const bool secondary_mixed = !kernel.is_primary_address_space(mem)
        && module_info->export_top != module_info->export_end && module_info->import_top != module_info->import_end;

    for (const auto &[seg, infos] : segment_reloc_info) {
        LOG_INFO("Loaded module segment {} @ [0x{:08X} - 0x{:08X} / 0x{:08X}] (size: 0x{:08X}) of module {}", seg, infos.addr, infos.addr + infos.size, infos.p_vaddr, infos.size, self_path);
    }

    const SceKernelModulePtr kernelModuleInfo = std::make_shared<KernelModule>(mem);
    kernelModuleInfo->info_segment_address = module_info_segment_address;
    kernelModuleInfo->info_offset = module_info_offset;
    kernelModuleInfo->module_nid = module_info->module_nid;

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
        if (capture_shared_initial_segments && segment.memsz) {
            const auto *data = Ptr<const uint8_t>(segment.vaddr.address()).get(mem);
            kernelModuleInfo->shared_initial_segments[segment_index].assign(data, data + segment.memsz);
        }
    }

    sceKernelModuleInfo->state = module_info->type;

    LOG_INFO("Linking {} {}...", is_self ? "SELF" : "ELF", self_path);
    if (self_path.contains("eboot.bin"))
        LOG_INFO("eboot.bin module NID: {}", log_hex(module_info->module_nid));

    ImportLinkJournal import_journal;
    if (!validate_exports(*module_info, module_info_segment_address, segment_reloc_info, mem)) {
        LOG_ERROR("Failed to validate exports of {} before linking", self_path);
        free_all_segments(mem, segment_reloc_info);
        return -1;
    }
    if (!load_imports(*module_info, module_info_segment_address, segment_reloc_info, kernel, mem, import_journal, false, true)) {
        LOG_ERROR("Failed to validate imports of {} before linking", self_path);
        free_all_segments(mem, segment_reloc_info);
        return -1;
    }
    if (!validate_late_variable_bindings(*module_info, module_info_segment_address, kernel, mem)) {
        LOG_ERROR("Failed to validate existing variable bindings before linking {}", self_path);
        free_all_segments(mem, segment_reloc_info);
        return -1;
    }
    VariableStubPool stub_pool(mem);
    if (secondary_mixed) {
        kernelModuleInfo->linkage_snapshot = snapshot_module_linkage(*module_info, module_info_segment_address, mem);
        std::set<uint32_t> available_variables;
        {
            const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
            for (const auto &[nid, address] : kernel.module_bindings(mem).exports)
                available_variables.insert(nid);
        }
        bool has_process_param = static_cast<bool>(kernel.get_process_param(mem));
        for (const auto &record : kernelModuleInfo->linkage_snapshot->exports) {
            for (size_t i = 0; i < record.nids.size(); ++i) {
                const uint32_t nid = record.nids[i];
                if (i < record.function_count) {
                    if (nid != NID_MODULE_START && nid != NID_MODULE_STOP && nid != NID_MODULE_EXIT)
                        available_variables.insert(nid);
                } else if (nid == NID_PROCESS_PARAM) {
                    if (!has_process_param && record.entries[i].cast<SceProcessParam>().get(mem)->version != 0) {
                        available_variables.insert(0xDF084DFA);
                        has_process_param = true;
                    }
                } else if (nid != NID_MODULE_INFO && nid != NID_SYSLYB) {
                    available_variables.insert(nid);
                }
            }
        }
        for (const auto &record : kernelModuleInfo->linkage_snapshot->imports) {
            for (const uint32_t nid : record.variable_nids) {
                if (!available_variables.contains(nid) && !stub_pool.reserve(nid)) {
                    LOG_ERROR("Failed to reserve variable imports for {}", self_path);
                    free_all_segments(mem, segment_reloc_info);
                    return -1;
                }
            }
        }
    }
    if (!load_exports(sceKernelModuleInfo, *module_info, module_info_segment_address, kernel, mem)) {
        return -1;
    }

    const bool imports_linked = secondary_mixed
        ? link_snapshot_imports(*kernelModuleInfo->linkage_snapshot, segment_reloc_info, kernel, mem, module_info->module_nid, import_journal, stub_pool)
        : load_imports(*module_info, module_info_segment_address, segment_reloc_info, kernel, mem, import_journal);
    if (!imports_linked) {
        LOG_ERROR("Failed to link imports of {}", self_path);
        rollback_imports(kernel, mem, import_journal);
        if (module_info->export_top == module_info->export_end) {
            free_all_segments(mem, segment_reloc_info);
        } else {
            LOG_ERROR("Cannot release failed module {} while its exports remain published", self_path);
        }
        return -1;
    }
    if (sceKernelModuleInfo->tlsInit)
        kernel.set_thread_tls_info(mem, { sceKernelModuleInfo->tlsInit, sceKernelModuleInfo->tlsInitSize, sceKernelModuleInfo->tlsAreaSize });
    const SceUID uid = kernel.get_next_uid();
    sceKernelModuleInfo->modid = uid;
    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        kernel.loaded_modules[uid] = kernelModuleInfo;
    }
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        kernel.module_uid_by_nid[&mem][module_info->module_nid] = uid;
        kernel.module_uid_layers[&mem][module_info->module_nid].push_back(uid);
    }

    return uid;
}

SceUID load_self(KernelState &kernel, MemState &mem, const void *self, const std::string &self_path, const fs::path &dump_path, bool capture_shared_initial_segments) {
    return load_self_impl(kernel, mem, self, self_path, dump_path, capture_shared_initial_segments, nullptr);
}

SceUID load_self_from_initial_segments(KernelState &kernel, MemState &mem, const std::shared_ptr<KernelModule> &source) {
    if (!source || source->address_space == &mem) {
        LOG_ERROR("Cannot instantiate a module without a source in another address space");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    const auto lease = kernel.acquire_module_start(*source->address_space, source);
    if (!lease) {
        LOG_ERROR("Cannot instantiate a module whose source is no longer loaded");
        return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
    }

    struct InitialImageHeader {
        Elf32_Ehdr elf{};
        std::array<Elf32_Phdr, MODULE_INFO_NUM_SEGMENTS> segments{};
    } image;
    image.elf.e_ident[EI_MAG0] = ELFMAG0;
    image.elf.e_ident[EI_MAG1] = ELFMAG1;
    image.elf.e_ident[EI_MAG2] = ELFMAG2;
    image.elf.e_ident[EI_MAG3] = ELFMAG3;
    image.elf.e_ident[EI_CLASS] = ELFCLASS32;
    image.elf.e_ident[EI_DATA] = ELFDATA2LSB;
    image.elf.e_ident[EI_VERSION] = EV_CURRENT;
    image.elf.e_type = ET_SCE_EXEC;
    image.elf.e_machine = EM_ARM;
    image.elf.e_phoff = offsetof(InitialImageHeader, segments);
    image.elf.e_phnum = MODULE_INFO_NUM_SEGMENTS;
    bool found_module_info = false;
    for (size_t i = 0; i < MODULE_INFO_NUM_SEGMENTS; ++i) {
        const auto &segment = source->info.segments[i];
        if (!segment.size)
            continue;
        if (!segment.memsz || source->shared_initial_segments[i].size() != segment.memsz || segment.filesz > segment.memsz) {
            LOG_ERROR("Initial image for module {} segment {} is missing or invalid", source->info.module_name, i);
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }
        auto &header = image.segments[i];
        header.p_type = PT_LOAD;
        header.p_vaddr = segment.vaddr.address();
        header.p_filesz = segment.filesz;
        header.p_memsz = segment.memsz;
        header.p_flags = segment.perms;
        if (source->info_segment_address.address() == segment.vaddr.address()) {
            if (source->info_offset > 0x3fffffff) {
                LOG_ERROR("Invalid module information offset for {}", source->info.module_name);
                return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
            }
            image.elf.e_entry = (static_cast<uint32_t>(i) << 30) | source->info_offset;
            found_module_info = true;
        }
    }
    if (!found_module_info) {
        LOG_ERROR("Initial image for module {} has no module information segment", source->info.module_name);
        return SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER;
    }
    return load_self_impl(kernel, mem, &image, source->info.path, {}, false, source.get());
}

int unload_self(KernelState &kernel, MemState &mem, KernelModule &module) {
    const std::lock_guard<std::mutex> linker_lock(kernel.module_linker_mutex);
    if (module.address_space != &mem) {
        LOG_ERROR("Cannot unload module {} from a different address space", module.info.modid);
        return -1;
    }
    if (module.startup_references.load() != 0) {
        LOG_ERROR("Cannot unload module {} while process startup is using it", module.info.modid);
        return SCE_KERNEL_ERROR_MODULEMGR_IN_USE;
    }
    const auto registered = kernel.find_module(mem, module.info.modid);
    if (!registered || registered.get() != &module) {
        LOG_ERROR("Cannot unload module {} because it is not registered in this address space", module.info.modid);
        return -1;
    }
    LOG_INFO("Unlinking self...");
    const sce_module_info_raw *const module_info = reinterpret_cast<const sce_module_info_raw *>(module.info_segment_address.get(mem) + module.info_offset);
    if (module.linkage_snapshot) {
        if (!unlink_snapshot(*module.linkage_snapshot, module.info_segment_address.address(), kernel, mem, module.module_nid))
            return -1;
    } else if (!load_exports(&module.info, *module_info, module.info_segment_address, kernel, mem, true)) {
        return -1;
    }

    SegmentInfosForReloc segment_reloc_info;
    for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
        const auto &segment = module.info.segments[i];
        if (segment.size == 0)
            continue;

        segment_reloc_info[i] = { segment.vaddr.address(), 0, segment.memsz }; // p_vaddr is not used in variable relocations
    }

    ImportLinkJournal unused_journal;
    if (!module.linkage_snapshot && !load_imports(*module_info, module.info_segment_address, segment_reloc_info, kernel, mem, unused_journal, true)) {
        return -1;
    }

    const uint32_t mod_nid = module.module_nid;

    const auto process_param = kernel.get_process_param(mem);
    if (process_param && segment_contains(segment_reloc_info, process_param.address(), sizeof(SceProcessParam))) {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        kernel.unload_process_param(mem, process_param.cast<uint32_t>());
    }

    // last step: free the memory
    for (int i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
        const auto &segment = module.info.segments[i];
        if (segment.size == 0)
            continue;

        kernel.invalidate_jit_cache(mem, segment.vaddr.address(), segment.memsz);
        free(mem, module.info.segments[i].vaddr.address());
    }

    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        kernel.loaded_modules.erase(module.info.modid);
    }
    {
        const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
        const auto process = kernel.module_uid_by_nid.find(&mem);
        const auto process_layers = kernel.module_uid_layers.find(&mem);
        if (process != kernel.module_uid_by_nid.end() && process_layers != kernel.module_uid_layers.end()) {
            const auto layer = process_layers->second.find(mod_nid);
            if (layer != process_layers->second.end()) {
                auto &uids = layer->second;
                const auto uid = std::find(uids.begin(), uids.end(), static_cast<uint32_t>(module.info.modid));
                if (uid != uids.end()) {
                    uids.erase(uid);
                    if (uids.empty()) {
                        process_layers->second.erase(layer);
                        process->second.erase(mod_nid);
                    } else {
                        process->second[mod_nid] = uids.back();
                    }
                } else {
                    LOG_ERROR("Module {} missing from NID {} index", module.info.modid, log_hex(mod_nid));
                }
            } else {
                LOG_ERROR("NID {} missing from module index", log_hex(mod_nid));
            }
        } else {
            LOG_ERROR("Address-space module index missing while unloading module {}", module.info.modid);
        }
    }

    return 0;
}
