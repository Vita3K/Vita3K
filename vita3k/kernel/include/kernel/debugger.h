#pragma once
#include <cpu/state.h>
#include <map>
#include <mem/state.h>
#include <mem/util.h>

constexpr uint32_t TRAMPOLINE_SVC = 0x53;

struct KernelState;

typedef std::function<bool(KernelState &kernel, CPUState &cpu, MemState &mem, Address lr)> TrampolineCallback;

struct Trampoline {
    bool thumb_mode;
    uint32_t original[3];
    Block trampoline_code;
    Address addr;
    Address lr;
    TrampolineCallback callback;
};

struct Breakpoint {
    bool thumb_mode;
    unsigned char data[4];
};

struct WatchMemory {
    Address start;
    size_t size;
};

typedef std::map<Address, WatchMemory> WatchMemoryAddrs;
typedef std::map<Address, Breakpoint> Breakpoints;
typedef std::map<Address, std::unique_ptr<Trampoline>> Trampolines;

struct Debugger {
    Debugger() = default;
    ~Debugger() = default;

    bool wait_for_debugger = false;
    bool watch_import_calls = false;
    bool watch_code = false;
    bool watch_memory = false;

    void init(KernelState *kernel);
    void add_watch_memory_addr(Address addr, size_t size);
    void remove_watch_memory_addr(KernelState &state, Address addr);
    void add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode);
    void remove_breakpoint(MemState &mem, uint32_t addr);
    void add_trampoile(MemState &mem, uint32_t addr, bool thumb_mode, TrampolineCallback callback, bool hook_before_orig = false);
    void remove_trampoline(MemState &mem, uint32_t addr);
    Address get_watch_memory_addr(Address addr);
    void update_watches();

private:
    std::mutex mutex;
    KernelState *parent;
    WatchMemoryAddrs watch_memory_addrs;
    Breakpoints breakpoints;
    Trampolines trampolines;
};
