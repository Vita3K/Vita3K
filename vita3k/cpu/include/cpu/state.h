#pragma once

#include <disasm/state.h>
#include <cpu/common.h>
#include <cpu/functions.h>

struct CPUState {
    SceUID thread_id;
    MemState *mem = nullptr;
    CallSVC call_svc;
    ResolveNIDName resolve_nid_name;
    DisasmState disasm;
    GetWatchMemoryAddr get_watch_memory_addr;

    Address halt_instruction_pc;

    CPUInterfacePtr cpu;

    std::vector<ModuleRegion> module_regions;
};
