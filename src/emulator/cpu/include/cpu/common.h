#pragma once

#include <mem/mem.h> // Address.

#include <memory>
#include <cstdint>
#include <functional>

struct CPUState;
struct MemState;

enum class CPUBackend {
    Dynarmic,
    Unicorn
};

union DoubleReg {
    double d;
    float f[2];
};

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

class CPUInterface;
using CPUInterfacePtr = std::shared_ptr<CPUInterface>;