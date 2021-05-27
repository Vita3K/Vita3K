#pragma once

#include <cpu/common.h>
#include <cpu/disasm/state.h>
#include <cpu/functions.h>

struct CPUState {
    CPUState() = default;

    SceUID thread_id = 0;
    MemState *mem = nullptr;
    CPUProtocolBase *protocol = nullptr;
    DisasmState disasm;

    Block halt_instruction;
    Address halt_instruction_pc; // thumb mode pc

    CPUInterfacePtr cpu;
};
