#include <cpu/state.h>
#include <cpu/dynarmic_cpu.h>
#include <cpu/factory.h>
#include <cpu/interface.h>
#include <cpu/unicorn_cpu.h>

CPUInterfacePtr new_cpu(CPUBackend backend, Address pc, Address sp, bool log_code, CPUState *state) {
    switch (backend) {
    case CPUBackend::Dynarmic:
        return std::make_unique<DynarmicCPU>(state, pc, sp, log_code);

    case CPUBackend::Unicorn:
        return std::make_unique<UnicornCPU>(state, pc, sp, log_code);

    default:
        return nullptr;
    }

    return nullptr;
}
