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

#include <cpu/disasm/functions.h>
#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/state.h>
#include <mem/ptr.h>
#include <util/types.h>

#include <algorithm>
#include <bit>
#include <memory>
#include <string>

CPUState::CPUState(MemState &mem, SceUID thread_id, std::size_t processor_id)
    : thread_id(thread_id)
    , mem(mem)
    , processor_id(processor_id) {}

CPUStatePtr init_cpu(bool cpu_opt, SceUID thread_id, std::size_t processor_id, MemState &mem) {
    CPUStatePtr cpu = std::make_unique<DynarmicCPU>(mem, thread_id, processor_id, cpu_opt);
    if (!init(cpu->disasm))
        return nullptr;
    return cpu;
}

void CPUState::set_pc(uint32_t val) {
    if (val & 1) {
        set_cpsr(get_cpsr() | 0x20);
        val = val & 0xFFFFFFFE;
    } else {
        set_cpsr(get_cpsr() & 0xFFFFFFDF);
        val = val & 0xFFFFFFFC;
    }
    set_reg(15, val);
}

std::optional<Address> CPUState::take_abort() {
    if (!abort_pending.exchange(false))
        return std::nullopt;
    return abort_fault_addr.load();
}

void CPUState::place_call(Address fn, std::initializer_list<uint32_t> args) {
    set_pc(fn);
    set_lr(return_pc);
    Address sp = get_sp();
    for (size_t i = 0; i < std::min<size_t>(args.size(), 4); i++)
        set_reg(i, args.begin()[i]);
    if (args.size() > 4) {
        // TODO align to 16 bytes
        sp -= 4 * (args.size() - 4);
        std::ranges::copy(args.begin() + 4, args.end(), Ptr<uint32_t>(sp).get(mem));
    }
    set_sp(sp);
}

void CPUState::write_call_result(const CallResult &result) {
    if (result.in_s0) {
        set_float_reg(0, std::bit_cast<float>(result.regs[0]));
        return;
    }
    for (uint8_t i = 0; i < result.count; i++)
        set_reg(i, result.regs[i]);
}

uint32_t CPUState::stack_alloc(size_t size) {
    const uint32_t new_sp = get_sp() - size;
    set_sp(new_sp);
    return new_sp;
}

void CPUState::stack_free(size_t size) {
    set_sp(get_sp() + size);
}

std::string CPUState::disassemble(uint64_t at, bool thumb, uint16_t *insn_size) {
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(at)).get(mem);
    const size_t buffer_size = GiB(4) - at;
    return ::disassemble(disasm, code, buffer_size, at, thumb, insn_size);
}

std::string CPUState::disassemble(uint64_t at, uint16_t *insn_size) {
    return disassemble(at, is_thumb_mode(), insn_size);
}

static thread_local CPUState *current_cpu_state = nullptr;

void set_current_cpu_state(CPUState *state) {
    current_cpu_state = state;
}

CPUState *get_current_cpu_state() {
    return current_cpu_state;
}
