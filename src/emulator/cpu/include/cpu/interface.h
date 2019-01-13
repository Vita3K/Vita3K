#pragma once

#include <cpu/common.h>

#include <cstdint>
#include <memory>

/*! \brief Base class for all CPU backend implementation */
class CPUInterface {
public:
    /*! \brief The thread context
    Stores register value and some pointer of the CPU
    */
    struct ThreadContext {
        std::array<uint32_t, 16> cpu_registers;
        uint32_t sp;
        uint32_t pc;
        uint32_t cpsr;
        std::array<uint32_t, 32> fpu_registers;
        uint32_t fpscr;
    };

    virtual int run() = 0;
    virtual void stop() = 0;

    virtual uint32_t get_reg(uint8_t idx) = 0;
    virtual uint32_t get_sp() = 0;
    virtual uint32_t get_pc() = 0;

    virtual uint32_t get_lr() = 0;
    virtual float get_float_reg(uint8_t idx) = 0;

    virtual void set_reg(uint8_t idx, uint32_t val) = 0;
    virtual void set_cpsr(uint32_t val) = 0;
    virtual void set_pc(uint32_t val) = 0;
    virtual void set_lr(uint32_t val) = 0;

    virtual void set_sp(uint32_t val) = 0;
    virtual uint32_t get_cpsr() = 0;
    virtual uint32_t get_fpscr() = 0;

    virtual ThreadContext save_context() = 0;

    virtual void load_context(ThreadContext context) = 0;

    virtual bool is_thumb_mode() = 0;
    virtual int step() = 0;
};

