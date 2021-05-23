#include <kernel/debugger.h>
#include <kernel/state.h>
#include <util/align.h>
#include <util/arm.h>
#include <util/log.h>

constexpr unsigned char THUMB_BREAKPOINT[2] = { 0x00, 0xBE };
constexpr unsigned char ARM_BREAKPOINT[4] = { 0x70, 0x00, 0x20, 0xE1 };

inline bool is_thumb16(uint32_t inst) {
    return (inst & 0xF8000000) < 0xE8000000;
}

void Debugger::init(KernelState *kernel) {
    parent = kernel;
}

void Debugger::add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode) {
    const auto lock = std::lock_guard(mutex);
    Breakpoint bk;
    bk.thumb_mode = thumb_mode;
    if (thumb_mode) {
        std::memcpy(&bk.data, &mem.memory[addr], sizeof(THUMB_BREAKPOINT));
        std::memcpy(&mem.memory[addr], THUMB_BREAKPOINT, sizeof(THUMB_BREAKPOINT));
    } else {
        std::memcpy(&bk.data, &mem.memory[addr], sizeof(ARM_BREAKPOINT));
        std::memcpy(&mem.memory[addr], ARM_BREAKPOINT, sizeof(ARM_BREAKPOINT));
    }
    breakpoints.emplace(addr, bk);
    parent->invalidate_jit_cache(addr, 4);
}

void Debugger::remove_breakpoint(MemState &mem, uint32_t addr) {
    const auto lock = std::lock_guard(mutex);
    if (breakpoints.find(addr) != breakpoints.end()) {
        auto last = breakpoints[addr];
        std::memcpy(&mem.memory[addr], &last.data, last.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        breakpoints.erase(addr);
        parent->invalidate_jit_cache(addr, 4);
    }
}

void Debugger::add_trampoile(MemState &mem, uint32_t addr, bool thumb_mode, TrampolineCallback callback) {
    const auto swap_inst = [](uint32_t inst) {
        return (inst << 16) | ((inst >> 16) & 0xFFFF);
    };

    std::unique_ptr<Trampoline> tr = std::make_unique<Trampoline>();
    tr->addr = addr;
    tr->thumb_mode = thumb_mode;
    tr->callback = callback;
    tr->trampoline_code = alloc_block(mem, 0x60, "trampoline");

    const Address trampoline_addr = align(tr->trampoline_code.get(), 4);
    tr->trampoline_addr = trampoline_addr;
    if (thumb_mode)
        tr->trampoline_addr |= 1;

    // Insert trampoline jumper and back up
    uint32_t *inst = Ptr<uint32_t>(addr).get(mem);
    tr->original = *inst;
    uint32_t back_inst;
    if (thumb_mode && is_thumb16(swap_inst(*inst))) {
        *inst = (tr->original & 0xFFFF0000) | 0xDF54; // SVC 0x54
        tr->lr = tr->addr + 2;
        tr->lr |= 1;
        back_inst = 0xBF000000 | (tr->original & 0xFFFF);
    } else if (thumb_mode) {
        *inst = 0xDF54BF00; // SVC 0x54
        tr->lr = tr->addr + 4;
        tr->lr |= 1;
        back_inst = tr->original;
    } else { // ARM
        *inst = 0xEF000054; // SVC 0x54
        tr->lr = tr->addr + 4;
        back_inst = tr->original;
    }

    // Create trampoline body
    uint32_t *trampoline_insts = reinterpret_cast<uint32_t *>(&mem.memory[trampoline_addr]);
    trampoline_insts[0] = back_inst; // original instruction; if thumb16 it's nop + original thumb16 instruction
    trampoline_insts[1] = thumb_mode ? 0xDF53BF00 : 0xEF000053; // SVC 0x53
    Trampoline **trampoline_host_ptr = Ptr<Trampoline *>(trampoline_addr + 8).get(mem); // interrupt handler will read this
    *trampoline_host_ptr = tr.get();

    std::lock_guard<std::mutex> lock(mutex);
    trampolines.emplace(addr, std::move(tr));
    parent->invalidate_jit_cache(addr, 4);
}

Trampoline *Debugger::get_trampoline(Address addr) {
    const auto lock = std::lock_guard(mutex);
    const auto it = trampolines.find(addr);
    if (it == trampolines.end())
        return nullptr;
    return it->second.get();
}

void Debugger::remove_trampoline(MemState &mem, uint32_t addr) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = trampolines.find(addr);
    if (it != trampolines.end()) {
        uint32_t *insts = reinterpret_cast<uint32_t *>(&mem.memory[addr]);
        insts[0] = it->second->original;
        trampolines.erase(it);
        parent->invalidate_jit_cache(addr, 4);
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
    parent->set_memory_watch(watch_memory);
}
