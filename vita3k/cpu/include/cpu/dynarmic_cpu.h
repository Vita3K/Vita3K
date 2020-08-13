#pragma once

#include <Dynarmic/A32/a32.h>

#include <cpu/functions.h>
#include <cpu/unicorn_cpu.h>

#include <functional>
#include <memory>


class ArmDynarmicCallback;

class DynarmicCPU : public CPUInterface {
    friend class ArmDynarmicCallback;

    UnicornCPU fallback;
    CPUState *parent;

    std::unique_ptr<Dynarmic::A32::Jit> jit;
    std::unique_ptr<ArmDynarmicCallback> cb;

    Address ep;
    Address tpidruro;
public:
    DynarmicCPU(CPUState *state, Address pc, Address sp, bool log_code);
    ~DynarmicCPU();
    int run(bool callback, Address entry_point) override;
    void stop() override;

    uint32_t get_reg(uint8_t idx) override;
    void set_reg(uint8_t idx, uint32_t val) override;

    uint32_t get_sp() override;
    void set_sp(uint32_t val) override;

    uint32_t get_pc() override;
    void set_pc(uint32_t val) override;

    uint32_t get_lr() override;
    void set_lr(uint32_t val) override;

    uint32_t get_cpsr() override;
    void set_cpsr(uint32_t val) override;

    uint32_t get_tpidruro() override;
    void set_tpidruro(uint32_t val) override;

    float get_float_reg(uint8_t idx) override;
    void set_float_reg(uint8_t idx, float val) override;

    uint32_t get_fpscr() override;
    void set_fpscr(uint32_t val) override;

    CPUContextPtr save_context() override;
    void load_context(CPUContext *context) override;

    bool is_thumb_mode() override;
    int step(Address entry_point) override;

    bool is_returning() override;
    bool hit_breakpoint() override;
    void trigger_breakpoint() override;
    void set_log_code(bool log) override;
    void set_log_mem(bool log) override;
    bool get_log_code() override;
    bool get_log_mem() override;
};