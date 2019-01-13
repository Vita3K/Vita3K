#pragma once

#include <Dynarmic/A32/a32.h>

#include <cpu/functions.h>
#include <cpu/unicorn_cpu.h>

#include <functional>
#include <memory>

struct CPUState;
struct MemState;

class ArmDynarmicCallback;

/*! \brief Base class for all CPU backend implementation */
class DynarmicCPU : public CPUInterface {
    friend class ArmDynarmicCallback;

    UnicornCPU fallback;

    std::unique_ptr<Dynarmic::A32::Jit> jit;
    std::unique_ptr<ArmDynarmicCallback> cb;

    Address ep;

public:
    DynarmicCPU(CPUState *state, Address pc, Address sp, bool log_code);
    ~DynarmicCPU();

    int run() override;
    int step() override;
    
    void stop() override;

    uint32_t get_reg(uint8_t idx) override;
    uint32_t get_sp() override;
    uint32_t get_pc() override;

    uint32_t get_lr() override;

    void set_reg(uint8_t idx, uint32_t val) override;

    void set_cpsr(uint32_t val) override;
    void set_pc(uint32_t val) override;
    void set_lr(uint32_t val) override;
    void set_sp(uint32_t val) override;

    uint32_t get_cpsr() override;
    uint32_t get_fpscr() override;

    float get_float_reg(uint8_t idx) override;
    bool is_thumb_mode() override;

    ThreadContext save_context() override;

    void load_context(UnicornCPU::ThreadContext context) override;
};
