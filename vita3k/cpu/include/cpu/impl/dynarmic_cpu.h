#pragma once

#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/context.h>
#include <dynarmic/interface/A32/coprocessor.h>
#include <dynarmic/interface/exclusive_monitor.h>

#include <cpu/functions.h>
#include <cpu/impl/unicorn_cpu.h>

#include <functional>
#include <memory>

class ArmDynarmicCallback;
class ArmDynarmicCP15;

class DynarmicCPU : public CPUInterface {
    friend class ArmDynarmicCallback;

    UnicornCPU fallback;
    CPUState *parent;

    std::unique_ptr<Dynarmic::A32::Jit> jit;
    std::unique_ptr<ArmDynarmicCallback> cb;
    std::shared_ptr<ArmDynarmicCP15> cp15;

    Address ep;
    Address tpidruro;

    std::size_t core_id = 0;

    bool exit_request = false;
    bool halted = false;

    bool did_break = false;

    bool log_read = false;
    bool log_write = false;

    std::unique_ptr<Dynarmic::A32::Jit> make_jit(Dynarmic::ExclusiveMonitor *monitor, bool cpu_opt);

public:
    DynarmicCPU(CPUState *state, std::size_t processor_id, Address pc, Address sp, Dynarmic::ExclusiveMonitor *monitor, bool cpu_opt);
    ~DynarmicCPU();
    int run() override;
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

    CPUContext save_context() override;
    void load_context(CPUContext context) override;

    bool is_thumb_mode() override;
    int step() override;

    bool hit_breakpoint() override;
    void trigger_breakpoint() override;
    void set_log_code(bool log) override;
    void set_log_mem(bool log) override;
    bool get_log_code() override;
    bool get_log_mem() override;

    std::size_t processor_id() const override;
    void invalidate_jit_cache(Address start, size_t length) override;
};
