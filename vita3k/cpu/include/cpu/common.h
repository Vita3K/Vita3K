#pragma once

#include <mem/mem.h> // Address.
#include <util/types.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct CPUState;
struct MemState;
struct CPUContext;
struct CPUInterface;

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;
typedef std::function<void(CPUState &cpu, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;
typedef std::function<Address(Address)> GetWatchMemoryAddr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUContext> CPUContextPtr;
typedef std::unique_ptr<CPUInterface> CPUInterfacePtr;

struct CPUContext {
    std::array<uint32_t, 16> cpu_registers;
    uint32_t cpsr;
    std::array<uint32_t, 64> fpu_registers;
    uint32_t fpscr;
};

struct ModuleRegion {
    uint32_t nid;
    std::string name;
    Address start;
    uint32_t size;
    Address vaddr;
};

struct CPUDepInject {
    CallImport call_import;
    CallSVC call_svc;
    ResolveNIDName resolve_nid_name;
    GetWatchMemoryAddr get_watch_memory_addr;
    std::vector<ModuleRegion> module_regions;
    bool trace_stack;
};

enum class CPUBackend {
    Unicorn,
    Dynarmic
};

union DoubleReg {
    double d;
    float f[2];
};
