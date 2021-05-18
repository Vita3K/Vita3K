#include <kernel/debugger.h>
#include <kernel/state.h>
#include <util/align.h>
#include <util/arm.h>

constexpr unsigned char THUMB_BREAKPOINT[2] = { 0x00, 0xBE };
constexpr unsigned char ARM_BREAKPOINT[4] = { 0x70, 0x00, 0x20, 0xE1 };

bool is_thumb16(uint32_t inst) {
    return (inst & 0xF8000000) < 0xE8000000;
}

void Debugger::init(KernelState *kernel) {
    parent = kernel;
}

void Debugger::add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode) {
    Breakpoint last = {
        thumb_mode,
        { 0 },
    };
    if (thumb_mode) {
        std::memcpy(&last.data, &mem.memory[addr], sizeof(THUMB_BREAKPOINT));
        std::memcpy(&mem.memory[addr], THUMB_BREAKPOINT, sizeof(THUMB_BREAKPOINT));
    } else {
        std::memcpy(&last.data, &mem.memory[addr], sizeof(ARM_BREAKPOINT));
        std::memcpy(&mem.memory[addr], ARM_BREAKPOINT, sizeof(ARM_BREAKPOINT));
    }
    breakpoints.emplace(addr, last);
}

void Debugger::remove_breakpoint(MemState &mem, uint32_t addr) {
    if (breakpoints.find(addr) != breakpoints.end()) {
        auto last = breakpoints[addr];
        std::memcpy(&mem.memory[addr], &last.data, last.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        breakpoints.erase(addr);
    }
}

void Debugger::add_trampoile(MemState &mem, uint32_t addr, bool thumb_mode, TrampolineCallback callback) {
    std::unique_ptr<Trampoline> tr = std::make_unique<Trampoline>();
    tr->addr = addr;
    tr->thumb_mode = thumb_mode;
    tr->callback = callback;
    uint32_t *insts = reinterpret_cast<uint32_t *>(&mem.memory[addr]);
    memcpy(tr->original, insts, 12);
    tr->trampoline_code = alloc_block(mem, 0x1C, "trampoline");
    tr->lr = tr->addr + 12;
    if (thumb_mode)
        tr->lr |= 1;
    const Address trampoline_addr = align(tr->trampoline_code.get(), 4);
    const auto encode_inst = thumb_mode ? encode_thumb_inst : encode_arm_inst;

    const Address imm = thumb_mode ? trampoline_addr | 1 : trampoline_addr;
    insts[0] = encode_inst(INSTRUCTION_MOVW, (uint16_t)imm, 12);
    insts[1] = encode_inst(INSTRUCTION_MOVT, (uint16_t)(imm >> 16), 12);
    insts[2] = encode_inst(INSTRUCTION_BRANCH, 0, 12);

    uint32_t *trampoline_insts = reinterpret_cast<uint32_t *>(&mem.memory[trampoline_addr]);
    memcpy(trampoline_insts, tr->original, 12);
    trampoline_insts[3] = thumb_mode ? 0xDF53BF00 : 0xEF000053; // SVC 0x53
    uint64_t *trampoline_ptr = reinterpret_cast<uint64_t *>(&trampoline_insts[4]);
    *trampoline_ptr = reinterpret_cast<uintptr_t>(tr.get());
}

void Debugger::remove_trampoline(MemState &mem, uint32_t addr) {
    auto it = trampolines.find(addr);
    if (it != trampolines.end()) {
        uint32_t *insts = reinterpret_cast<uint32_t *>(&mem.memory[addr]);
        memcpy(insts, it->second->original, 12);
        trampolines.erase(addr);
    }
}

void Debugger::add_watch_memory_addr(Address addr, size_t size) {
    std::lock_guard<std::mutex> lock(mutex);
    watch_memory_addrs.emplace(addr, WatchMemory{ addr, size });
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

void Debugger::update_watches() {
    for (const auto &thread : parent->threads) {
        auto &cpu = *thread.second->cpu;
        if (watch_code != get_log_code(cpu)) {
            if (watch_code)
                set_log_code(cpu, true);
            else
                set_log_code(cpu, false);
        }
        if (watch_memory != get_log_mem(cpu)) {
            if (watch_memory)
                set_log_mem(cpu, true);
            else
                set_log_mem(cpu, false);
        }
    }
}
