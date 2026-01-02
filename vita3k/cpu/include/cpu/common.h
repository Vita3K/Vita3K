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

#pragma once

#include <fmt/format.h>
#include <mem/util.h> // Address.

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

struct CPUState;
struct CPUContext;
struct CPUInterface;
struct ThreadState;

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;

typedef std::function<Address(Address)> GetWatchMemoryAddr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUInterface> CPUInterfacePtr;
typedef void *ExclusiveMonitorPtr;

struct CPUProtocolBase {
    virtual void call_svc(CPUState &cpu, uint32_t svc, Address pc, ThreadState &thread) = 0;
    virtual Address get_watch_memory_addr(Address addr) = 0;
    virtual ExclusiveMonitorPtr get_exclusive_monitor() = 0;
    virtual ~CPUProtocolBase() = default;
};

struct CPUContext {
    CPUContext() = default;

    std::array<uint32_t, 16> cpu_registers{};
    std::array<float, 64> fpu_registers{};
    uint32_t cpsr = 0;
    uint32_t fpscr = 0;

    bool thumb() const {
        return cpsr & 0x20;
    }

    uint32_t get_sp() const {
        return cpu_registers[13];
    }

    uint32_t get_lr() const {
        return cpu_registers[14];
    }

    uint32_t get_pc() const {
        return cpu_registers[15];
    }

    void set_sp(uint32_t val) {
        cpu_registers[13] = val;
    }

    void set_lr(uint32_t val) {
        cpu_registers[14] = val;
    }

    void set_pc(uint32_t val) {
        if (val & 1) {
            cpsr |= 0x20;
            val = val & 0xFFFFFFFE;
        } else {
            cpsr &= 0xFFFFFFDF;
            val = val & 0xFFFFFFFC;
        }
        cpu_registers[15] = val;
    }

    std::string description() {
        uint32_t pc = get_pc();
        uint32_t sp = get_sp();
        uint32_t lr = get_lr();

        std::string str;
        auto back_it = std::back_inserter(str);
        fmt::format_to(back_it, "PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}\n", pc, sp, lr);
        for (int a = 0; a < 6; a++) {
            fmt::format_to(back_it, "r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}\n", a, cpu_registers[a], a + 6, cpu_registers[a + 6]);
        }
        fmt::format_to(back_it, "r12: 0x{:0>8x}\n", cpu_registers[12]);
        fmt::format_to(back_it, "Thumb: {}\n", thumb());
        return str;
    }
};

union DoubleReg {
    double d;
    float f[2];
};
