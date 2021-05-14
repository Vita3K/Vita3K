#pragma once

#include <cpu/common.h>

#include <cstdint>
#include <memory>

/*! \brief Base class for all CPU backend implementation */
struct CPUInterface {
    virtual int run() = 0;
    virtual void stop() = 0;

    virtual uint32_t get_reg(uint8_t idx) = 0;
    virtual void set_reg(uint8_t idx, uint32_t val) = 0;

    virtual uint32_t get_sp() = 0;
    virtual void set_sp(uint32_t val) = 0;

    virtual uint32_t get_pc() = 0;
    virtual void set_pc(uint32_t val) = 0;

    virtual uint32_t get_lr() = 0;
    virtual void set_lr(uint32_t val) = 0;

    virtual uint32_t get_cpsr() = 0;
    virtual void set_cpsr(uint32_t val) = 0;

    virtual uint32_t get_tpidruro() = 0;
    virtual void set_tpidruro(uint32_t val) = 0;

    virtual float get_float_reg(uint8_t idx) = 0;
    virtual void set_float_reg(uint8_t idx, float val) = 0;

    virtual uint32_t get_fpscr() = 0;
    virtual void set_fpscr(uint32_t val) = 0;

    virtual CPUContext save_context() = 0;
    virtual void load_context(CPUContext context) = 0;

    virtual bool is_thumb_mode() = 0;
    virtual int step() = 0;

    virtual bool hit_breakpoint() = 0;
    virtual void trigger_breakpoint() = 0;
    virtual void set_log_code(bool log) = 0;
    virtual void set_log_mem(bool log) = 0;
    virtual bool get_log_code() = 0;
    virtual bool get_log_mem() = 0;

    virtual std::size_t processor_id() const {
        return 0;
    }
};
