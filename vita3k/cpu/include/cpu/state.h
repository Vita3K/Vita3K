#pragma once

#include <cpu/common.h>
#include <cpu/functions.h>
#include <disasm/state.h>

struct CPUState {
    SceUID thread_id;
    MemState *mem = nullptr;
    CPUProtocolBase *protocol = nullptr;
    DisasmState disasm;

    Block halt_instruction;
    Address halt_instruction_pc; // thumb mode pc

    CPUInterfacePtr cpu;
};
