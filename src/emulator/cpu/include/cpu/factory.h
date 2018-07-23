#pragma once

#include <cstdint>
#include <memory>

class CPUInterface;
struct CPUState;

enum class CPUBackend {
    Dynarmic,
    Unicorn
};

typedef uint32_t Address;
using CPUInterfacePtr = std::unique_ptr<CPUInterface>;

CPUInterfacePtr new_cpu(CPUBackend backend, Address pc, Address sp, bool log_code, CPUState *state);