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
    parent->invalidate_jit_cache(addr, 4);
}

void Debugger::remove_breakpoint(MemState &mem, uint32_t addr) {
    if (breakpoints.find(addr) != breakpoints.end()) {
        auto last = breakpoints[addr];
        std::memcpy(&mem.memory[addr], &last.data, last.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        breakpoints.erase(addr);
        parent->invalidate_jit_cache(addr, 4);
    }
}

void Debugger::add_trampoile(MemState &mem, uint32_t addr, bool thumb_mode, TrampolineCallback callback, bool hook_before_orig) {
    const auto encode_thumb_and_swap = [](uint8_t type, uint32_t immed, uint16_t reg) {
        const uint32_t inst = encode_thumb_inst(type, immed, reg);
        return (inst << 16) | ((inst >> 16) & 0xFFFF);
    };
    const auto encode_inst = thumb_mode ? encode_thumb_and_swap : encode_arm_inst;

    std::unique_ptr<Trampoline> tr = std::make_unique<Trampoline>();
    tr->addr = addr;
    tr->thumb_mode = thumb_mode;
    tr->callback = callback;
    tr->trampoline_code = alloc_block(mem, 0x60, "trampoline");
    tr->lr = tr->addr + 12;
    if (thumb_mode)
        tr->lr |= 1;

    const Address trampoline_addr = align(tr->trampoline_code.get(), 4);
    // Insert trampoline jumper and back up
    uint32_t *insts = reinterpret_cast<uint32_t *>(&mem.memory[addr]);
    memcpy(tr->original, insts, 12);
    const Address destination = thumb_mode ? trampoline_addr | 1 : trampoline_addr;

    insts[0] = encode_inst(INSTRUCTION_MOVW, (uint16_t)destination, 12);
    insts[1] = encode_inst(INSTRUCTION_MOVT, (uint16_t)(destination >> 16), 12);
    insts[2] = encode_inst(INSTRUCTION_BRANCH, 0, 12);

    // Create trampoline
    uint32_t *trampoline_insts = reinterpret_cast<uint32_t *>(&mem.memory[trampoline_addr]);
    if (!hook_before_orig) {
        memcpy(trampoline_insts, tr->original, 12);
        trampoline_insts[3] = thumb_mode ? 0xDF53BF00 : 0xEF000053; // SVC 0x53
        Trampoline **trampoline_host_ptr = Ptr<Trampoline *>(trampoline_addr + 16).get(mem);
        *trampoline_host_ptr = tr.get();
    } else {
        trampoline_insts[0] = thumb_mode ? 0xDF53BF00 : 0xEF000053; // SVC 0x53
        Trampoline **trampoline_host_ptr = Ptr<Trampoline *>(trampoline_addr + 4).get(mem);
        *trampoline_host_ptr = tr.get();
        memcpy(&trampoline_insts[3], tr->original, 12);
        trampoline_insts[6] = encode_inst(INSTRUCTION_MOVW, (uint16_t)tr->lr, 12);
        trampoline_insts[7] = encode_inst(INSTRUCTION_MOVT, (uint16_t)(tr->lr >> 16), 12);
        trampoline_insts[8] = encode_inst(INSTRUCTION_BRANCH, 0, 12);
        tr->lr = trampoline_addr + 12;
        if (thumb_mode)
            tr->lr |= 1;
    }

    std::lock_guard<std::mutex> lock(mutex);
    trampolines.emplace(addr, std::move(tr));
    parent->invalidate_jit_cache(addr, 12);
}

void Debugger::remove_trampoline(MemState &mem, uint32_t addr) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = trampolines.find(addr);
    if (it != trampolines.end()) {
        uint32_t *insts = reinterpret_cast<uint32_t *>(&mem.memory[addr]);
        memcpy(insts, it->second->original, 12);
        trampolines.erase(it);
        parent->invalidate_jit_cache(addr, 12);
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
