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

#include <fmt/format.h>
#include <mem/util.h> // Address.

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

struct CPUState;
struct CPUContext;

using CPUStatePtr = std::unique_ptr<CPUState>;

inline constexpr std::size_t MAX_CORE_COUNT = 150;

// Why guest code stopped
struct GuestExit {
    enum class Kind : uint8_t {
        // The function returned to the return address, value is r0
        returned,
        // Guest code called an import, value is its NID
        import,
        // A request_stop(), a breakpoint or a finished step
        stopped,
    };
    Kind kind;
    uint32_t value = 0;
    // A breakpoint or a step that ran its instruction, so the debugger holds the thread. A stepped
    // import has it too.
    bool debugger_stop = false;
};

// An import's result, which the kernel writes to the registers when the call returns to guest code
struct CallResult {
    // r0 to r3, of which count are written
    std::array<uint32_t, 4> regs{};
    uint8_t count = 0;
    // A float result, in s0 with its bits in regs[0]
    bool in_s0 = false;
};

struct CPUContext {
    std::array<uint32_t, 16> cpu_registers{};
    std::array<float, 64> fpu_registers{};
    uint32_t cpsr = 0;
    uint32_t fpscr = 0;

    // The registers as 32-bit words: r0 to r15, cpsr, s0 to s63, then fpscr. A d register is two
    // words and a q register four.
    static constexpr uint32_t WORD_CPSR = 16;
    static constexpr uint32_t WORD_S0 = 17;
    static constexpr uint32_t WORD_FPSCR = WORD_S0 + 64;

    uint32_t word(uint32_t i) const {
        if (i < WORD_CPSR)
            return cpu_registers[i];
        if (i == WORD_CPSR)
            return cpsr;
        if (i < WORD_FPSCR)
            return std::bit_cast<uint32_t>(fpu_registers[i - WORD_S0]);
        return fpscr;
    }

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

    std::string description() const {
        std::string str = fmt::format("PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}\n", get_pc(), get_sp(), get_lr());
        auto back_it = std::back_inserter(str);
        for (int a = 0; a < 6; a++) {
            fmt::format_to(back_it, "r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}\n", a, cpu_registers[a], a + 6, cpu_registers[a + 6]);
        }
        fmt::format_to(back_it, "r12: 0x{:0>8x}\n", cpu_registers[12]);
        fmt::format_to(back_it, "Thumb: {}\n", thumb());
        return str;
    }
};
