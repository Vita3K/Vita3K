#pragma once

#include <fmt/format.h>
#include <mem/ptr.h> // Address.
#include <util/types.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct CPUState;
struct MemState;
struct CPUContext;
struct CPUInterface;

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;

typedef std::function<Address(Address)> GetWatchMemoryAddr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUInterface> CPUInterfacePtr;
typedef void *ExclusiveMonitorPtr;

struct CPUProtocolBase {
    virtual void call_svc(CPUState &cpu, uint32_t svc, Address pc, SceUID thread_id) = 0;
    virtual Address get_watch_memory_addr(Address addr) = 0;
    virtual ExclusiveMonitorPtr get_exlusive_monitor() = 0;
    virtual ~CPUProtocolBase() {}
};

struct CPUContext {
    std::array<uint32_t, 16> cpu_registers;
    uint32_t cpsr;
    std::array<uint32_t, 64> fpu_registers;
    uint32_t fpscr;

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
        std::stringstream ss;
        uint32_t pc = get_pc();
        uint32_t sp = get_sp();
        uint32_t lr = get_lr();

        ss << fmt::format("PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}\n", pc, sp, lr);
        for (int a = 0; a < 6; a++) {
            ss << fmt::format("r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}\n", a, cpu_registers[a], a + 6, cpu_registers[a + 6]);
        }
        ss << fmt::format("r12: 0x{:0>8x}\n", cpu_registers[12]);
        return ss.str();
    }
};

enum class CPUBackend {
    Unicorn,
    Dynarmic
};

union DoubleReg {
    double d;
    float f[2];
};
