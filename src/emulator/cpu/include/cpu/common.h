#pragma once

#include <mem/mem.h> // Address.

#include <cstdint>
#include <functional>
#include <memory>

struct CPUState;
struct MemState;

enum class CPUBackend {
    Unicorn,
    Dynarmic
};

union DoubleReg {
    double d;
    float f[2];
};

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

class CPUInterface;
using CPUInterfacePtr = std::shared_ptr<CPUInterface>;