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

#include <module/exported_variable.h>

#include <kernel/state.h>
#include <mem/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

std::optional<ExportedVariable> make_stack_chk_guard(MemState &mem) {
    const Address address = alloc(mem, sizeof(uint32_t), "__stack_chk_guard");
    if (!address) {
        LOG_ERROR("Failed to allocate __stack_chk_guard");
        return std::nullopt;
    }
    *Ptr<uint32_t>(address).get(mem) = 0x0DEADBEE;
    return ExportedVariable{ address, { address } };
}

std::optional<ExportedVariable> make_perf_capture_flag(MemState &mem) {
    const Address pointer = alloc(mem, sizeof(uint32_t), "_pLibPerfCaptureFlagPtr");
    if (!pointer) {
        LOG_ERROR("Failed to allocate _pLibPerfCaptureFlagPtr");
        return std::nullopt;
    }
    const Address flag = alloc(mem, sizeof(uint32_t), "_pLibPerfCaptureFlag");
    if (!flag) {
        LOG_ERROR("Failed to allocate _pLibPerfCaptureFlag");
        free(mem, pointer);
        return std::nullopt;
    }
    *Ptr<uint32_t>(pointer).get(mem) = flag;
    *Ptr<uint32_t>(flag).get(mem) = 0;
    return ExportedVariable{ pointer, { pointer, flag } };
}

bool initialize_exported_variables(KernelState &kernel, MemState &mem, std::span<const ExportedVariableDefinition> definitions) {
    const std::lock_guard<std::mutex> guard(kernel.export_nids_mutex);
    const auto bindings = kernel.module_bindings(mem);
    std::vector<ExportedVariable> initialized;
    initialized.reserve(definitions.size());
    for (const auto &definition : definitions) {
        auto value = definition.factory(mem);
        if (!value) {
            LOG_ERROR("Failed to initialize HLE variable {}", log_hex(definition.nid));
            for (const auto &previous : initialized)
                for (const Address address : previous.allocations)
                    free(mem, address);
            return false;
        }
        initialized.push_back(std::move(*value));
    }
    for (size_t i = 0; i < definitions.size(); ++i) {
        const auto &value = initialized[i];
        if (!bindings.exports.emplace(definitions[i].nid, value.address).second) {
            for (const Address address : value.allocations)
                free(mem, address);
        }
    }
    return true;
}
