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

#include <cpu/common.h>
#include <cpu/disasm/state.h>
#include <mem/state.h>
#include <util/types.h>

#include <atomic>

struct CPUState {
    SceUID thread_id = 0;
    MemState *mem = nullptr;
    DisasmState disasm;

    std::unique_ptr<CPUInterface> cpu;
    // Return address place_call() writes to LR. Execution stops there.
    Address return_pc = 0;

    // Set by the kubridge fault handler from a signal, read by the CPU after it halts
    std::atomic<bool> abort_pending{ false };
    std::atomic<uint32_t> abort_fault_addr{ 0 };
};
