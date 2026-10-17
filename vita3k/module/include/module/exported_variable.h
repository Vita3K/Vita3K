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

#pragma once

#include <mem/util.h>

#include <functional>
#include <optional>
#include <span>
#include <vector>

struct KernelState;
struct MemState;

struct ExportedVariable {
    Address address = 0;
    std::vector<Address> allocations;
};

struct ExportedVariableDefinition {
    uint32_t nid;
    std::function<std::optional<ExportedVariable>(MemState &)> factory;
};

std::optional<ExportedVariable> make_stack_chk_guard(MemState &mem);
std::optional<ExportedVariable> make_perf_capture_flag(MemState &mem);
bool initialize_exported_variables(KernelState &kernel, MemState &mem, std::span<const ExportedVariableDefinition> definitions);
