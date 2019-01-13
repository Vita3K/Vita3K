#pragma once

#include <cpu/common.h>
#include <disasm/state.h>

struct MemState;

struct CPUState {
    MemState *mem = nullptr;
    CallSVC call_svc;
    DisasmState disasm;
    CPUInterfacePtr cpu;
    Address entry_point;
    bool did_break;
};
