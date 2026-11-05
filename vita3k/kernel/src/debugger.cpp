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

#include <kernel/debugger.h>
#include <kernel/state.h>

#include <cpu/state.h>

#include <algorithm>
#include <cstring>
#include <optional>

constexpr unsigned char THUMB_BREAKPOINT[2] = { 0x00, 0xBE };
constexpr unsigned char ARM_BREAKPOINT[4] = { 0x70, 0x00, 0x20, 0xE1 };

void Debugger::add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode, std::vector<BreakpointCondition> conditions) {
    const auto lock = std::lock_guard(mutex);
    // GDB sends a breakpoint again when its conditions change
    if (const auto it = breakpoints.find(addr); it != breakpoints.end()) {
        it->second.conditions = std::move(conditions);
        return;
    }
    Breakpoint bk;
    bk.thumb_mode = thumb_mode;
    bk.conditions = std::move(conditions);
    if (thumb_mode) {
        std::memcpy(&bk.data, Ptr<uint8_t>(addr).get(mem), sizeof(THUMB_BREAKPOINT));
        std::memcpy(Ptr<uint8_t>(addr).get(mem), THUMB_BREAKPOINT, sizeof(THUMB_BREAKPOINT));
    } else {
        std::memcpy(&bk.data, Ptr<uint8_t>(addr).get(mem), sizeof(ARM_BREAKPOINT));
        std::memcpy(Ptr<uint8_t>(addr).get(mem), ARM_BREAKPOINT, sizeof(ARM_BREAKPOINT));
    }
    breakpoints.emplace(addr, bk);
    parent.invalidate_jit_cache(addr, 4);
}

void Debugger::remove_breakpoint(MemState &mem, uint32_t addr) {
    const auto lock = std::lock_guard(mutex);
    if (breakpoints.contains(addr)) {
        auto last = breakpoints[addr];
        std::memcpy(Ptr<uint8_t>(addr).get(mem), &last.data, last.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        breakpoints.erase(addr);
        parent.invalidate_jit_cache(addr, 4);
    }
}

Debugger::Debugger(KernelState &kernel)
    : parent(kernel) {
}

void Debugger::remove_all_breakpoints(MemState &mem) {
    const auto lock = std::lock_guard(mutex);
    for (const auto &[addr, bk] : breakpoints) {
        std::memcpy(Ptr<uint8_t>(addr).get(mem), &bk.data, bk.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        parent.invalidate_jit_cache(addr, 4);
    }
    breakpoints.clear();
}

void Debugger::notify_breakpoint(SceUID thread_id) {
    {
        std::lock_guard<std::mutex> lock(break_mutex);
        break_thread_id = thread_id;
        has_break = true;
    }
    break_cv.notify_one();
}

void Debugger::add_watch_memory_addr(Address addr, size_t size, WatchType type) {
    std::lock_guard<std::mutex> lock(mutex);
    watch_memory_addrs.emplace(addr, WatchMemory{ addr, size, type });
}

void Debugger::remove_watch_memory_addr(KernelState &state, Address addr) {
    std::lock_guard<std::mutex> lock(mutex);
    watch_memory_addrs.erase(addr);
}

// TODO use boost icl or interval tree instead if this turns out to be a significant bottleneck
Address Debugger::get_watch_memory_addr(Address addr) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &item : watch_memory_addrs) {
        if (item.second.start <= addr && addr < item.second.start + item.second.size) {
            return item.second.start;
        }
    }
    return 0;
}

Address Debugger::check_watchpoint(Address addr, bool is_write) {
    for (const auto &item : watch_memory_addrs) {
        if (item.second.start <= addr && addr < item.second.start + item.second.size) {
            const auto t = item.second.type;
            if (t == WatchType::ACCESS
                || (t == WatchType::WRITE && is_write)
                || (t == WatchType::READ && !is_write)) {
                break_watch_addr = item.second.start;
                break_watch_type = t;
                return item.second.start;
            }
        }
    }
    return 0;
}

// Runs a GDB agent expression for a thread and returns its value, or nothing for an expression
// it can't run. Conditions only need the arithmetic, register and memory bytecodes.
// https://sourceware.org/gdb/current/onlinedocs/gdb.html/Bytecode-Descriptions.html
static std::optional<int64_t> run_agent_expression(const BreakpointCondition &code, MemState &mem, CPUState &cpu) {
    std::vector<int64_t> stack;
    size_t pc = 0;
    // An expression with a loop that doesn't end stops the thread instead
    for (int budget = 10000; budget > 0; budget--) {
        if (pc >= code.size())
            return std::nullopt;
        const uint8_t op = code[pc++];
        // Big-endian operands after the opcode
        const auto operand = [&](size_t bytes) -> std::optional<uint64_t> {
            if (pc + bytes > code.size())
                return std::nullopt;
            uint64_t value = 0;
            for (size_t i = 0; i < bytes; i++)
                value = (value << 8) | code[pc++];
            return value;
        };
        const auto pop = [&]() -> std::optional<int64_t> {
            if (stack.empty())
                return std::nullopt;
            const int64_t value = stack.back();
            stack.pop_back();
            return value;
        };
        const auto read = [&](Address addr, size_t bytes) -> std::optional<int64_t> {
            if (!Ptr<uint8_t>(addr).valid(mem) || !Ptr<uint8_t>(addr + bytes - 1).valid(mem))
                return std::nullopt;
            uint64_t value = 0;
            std::memcpy(&value, Ptr<uint8_t>(addr).get(mem), bytes);
            return static_cast<int64_t>(value);
        };

        // Opcodes with no operand that take one value or two
        if (op == 0x0e || op == 0x12 || (op >= 0x17 && op <= 0x1a)) {
            const auto a = pop();
            if (!a)
                return std::nullopt;
            std::optional<int64_t> result;
            switch (op) {
            case 0x0e: result = *a == 0; break; // log_not
            case 0x12: result = ~*a; break; // bit_not
            case 0x17: result = read(static_cast<Address>(*a), 1); break; // ref8
            case 0x18: result = read(static_cast<Address>(*a), 2); break; // ref16
            case 0x19: result = read(static_cast<Address>(*a), 4); break; // ref32
            case 0x1a: result = read(static_cast<Address>(*a), 8); break; // ref64
            }
            if (!result)
                return std::nullopt;
            stack.push_back(*result);
            continue;
        }
        if ((op >= 0x02 && op <= 0x0b) || (op >= 0x0f && op <= 0x11) || (op >= 0x13 && op <= 0x15)) {
            const auto b = pop();
            const auto a = pop();
            if (!a || !b)
                return std::nullopt;
            const uint64_t ua = *a;
            const uint64_t ub = *b;
            if ((op >= 0x05 && op <= 0x08) && *b == 0)
                return std::nullopt;
            int64_t result = 0;
            switch (op) {
            case 0x02: result = ua + ub; break; // add
            case 0x03: result = ua - ub; break; // sub
            case 0x04: result = ua * ub; break; // mul
            case 0x05: result = *a / *b; break; // div_signed
            case 0x06: result = ua / ub; break; // div_unsigned
            case 0x07: result = *a % *b; break; // rem_signed
            case 0x08: result = ua % ub; break; // rem_unsigned
            case 0x09: result = ub < 64 ? ua << ub : 0; break; // lsh
            case 0x0a: result = *a >> std::min<uint64_t>(ub, 63); break; // rsh_signed
            case 0x0b: result = ub < 64 ? ua >> ub : 0; break; // rsh_unsigned
            case 0x0f: result = *a & *b; break; // bit_and
            case 0x10: result = *a | *b; break; // bit_or
            case 0x11: result = *a ^ *b; break; // bit_xor
            case 0x13: result = *a == *b; break; // equal
            case 0x14: result = *a < *b; break; // less_signed
            case 0x15: result = ua < ub; break; // less_unsigned
            }
            stack.push_back(result);
            continue;
        }

        switch (op) {
        case 0x16: // ext: sign extend from n bits
        case 0x2a: { // zero_ext
            const auto bits = operand(1);
            if (!bits || stack.empty() || *bits == 0 || *bits > 64)
                return std::nullopt;
            if (*bits < 64) {
                const uint64_t mask = (uint64_t(1) << *bits) - 1;
                uint64_t value = static_cast<uint64_t>(stack.back()) & mask;
                if (op == 0x16 && (value >> (*bits - 1)) & 1)
                    value |= ~mask;
                stack.back() = static_cast<int64_t>(value);
            }
            break;
        }
        case 0x20: // if_goto
        case 0x21: { // goto
            const auto target = operand(2);
            if (!target)
                return std::nullopt;
            if (op == 0x20) {
                const auto value = pop();
                if (!value)
                    return std::nullopt;
                if (*value == 0)
                    break;
            }
            pc = *target;
            break;
        }
        case 0x22: // const8
        case 0x23: // const16
        case 0x24: // const32
        case 0x25: { // const64
            const auto value = operand(size_t(1) << (op - 0x22));
            if (!value)
                return std::nullopt;
            stack.push_back(static_cast<int64_t>(*value));
            break;
        }
        case 0x26: { // reg, numbered as in remote packets
            const auto reg = operand(2);
            if (!reg)
                return std::nullopt;
            if (*reg <= 12)
                stack.push_back(cpu.get_reg(static_cast<uint8_t>(*reg)));
            else if (*reg == 13)
                stack.push_back(cpu.get_sp());
            else if (*reg == 14)
                stack.push_back(cpu.get_lr());
            else if (*reg == 15)
                stack.push_back(cpu.get_pc());
            else if (*reg == 16)
                stack.push_back(cpu.get_cpsr());
            else
                return std::nullopt;
            break;
        }
        case 0x27: // end
            return pop();
        case 0x28: // dup
            if (stack.empty())
                return std::nullopt;
            stack.push_back(stack.back());
            break;
        case 0x29: // pop
            if (!pop())
                return std::nullopt;
            break;
        case 0x2b: // swap
            if (stack.size() < 2)
                return std::nullopt;
            std::swap(stack[stack.size() - 1], stack[stack.size() - 2]);
            break;
        case 0x32: { // pick: copy the value n below the top
            const auto n = operand(1);
            if (!n || *n >= stack.size())
                return std::nullopt;
            stack.push_back(stack[stack.size() - 1 - *n]);
            break;
        }
        case 0x33: // rot: a b c => c a b
            if (stack.size() < 3)
                return std::nullopt;
            std::rotate(stack.end() - 3, stack.end() - 1, stack.end());
            break;
        default:
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool Debugger::breakpoint_stops(MemState &mem, CPUState &cpu) {
    const auto lock = std::lock_guard(mutex);
    const auto it = breakpoints.find(cpu.get_pc() & ~1);
    if (it == breakpoints.end() || it->second.conditions.empty())
        return true;
    // A condition that can't run stops the thread, as GDB does
    return std::any_of(it->second.conditions.begin(), it->second.conditions.end(), [&](const BreakpointCondition &condition) {
        const std::optional<int64_t> value = run_agent_expression(condition, mem, cpu);
        return !value || *value != 0;
    });
}

GuestExit Debugger::step_over_breakpoint(MemState &mem, CPUState &cpu) {
    // Other threads miss the breakpoint while the instruction is back
    const auto lock = std::lock_guard(mutex);
    const Address addr = cpu.get_pc() & ~1;
    const auto it = breakpoints.find(addr);
    if (it == breakpoints.end())
        return cpu.run(CPUState::RunMode::single_step);
    const Breakpoint &bk = it->second;
    const size_t size = bk.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT);
    std::memcpy(Ptr<uint8_t>(addr).get(mem), &bk.data, size);
    parent.invalidate_jit_cache(addr, 4);
    const GuestExit exit = cpu.run(CPUState::RunMode::single_step);
    std::memcpy(Ptr<uint8_t>(addr).get(mem), bk.thumb_mode ? THUMB_BREAKPOINT : ARM_BREAKPOINT, size);
    parent.invalidate_jit_cache(addr, 4);
    return exit;
}

void Debugger::update_watches() {
    parent.set_memory_watch(watch_memory);
}

void Debugger::deinit() {
    std::lock_guard<std::mutex> lock(mutex);
    breakpoints.clear();
    watch_memory_addrs.clear();
}
