#pragma once

#include <array>
#include <cstdint>
#include <memory>

typedef uint32_t Address;

/*! \brief Base class for all CPU backend implementation */
class CPUInterface {
public:
    /*! \brief The thread context
    Stores register value and some pointer of the CPU
    */
    struct ThreadContext {
        std::array<uint32_t, 31> cpu_registers;
        uint32_t sp;
        uint32_t pc;
        uint32_t cpsr;
        std::array<uint32_t, 32> fpu_registers;
        uint32_t fpscr;
    };

    /*! Run the CPU */
    virtual void run() = 0;

    /*! Stop the CPU */
    virtual void stop() = 0;

    /*! Get a specific ARM Rx register. Range from r0 to r15 */
    virtual uint32_t get_reg(uint8_t idx) = 0;

    /*! Get the stack pointer */
    virtual uint32_t get_sp() = 0;

    /*! Get the program counter */
    virtual uint32_t get_pc() = 0;

    virtual uint32_t get_lr() = 0;

    virtual float get_float_reg(uint8_t idx) = 0;

    /*! Set a Rx register */
    virtual void set_reg(uint8_t idx, uint32_t val) = 0;

    virtual void set_cpsr(uint32_t val) = 0;

    /*! Set program counter */
    virtual void set_pc(uint32_t val) = 0;

    /*! Set LR */
    virtual void set_lr(uint32_t val) = 0;

    /*! Set stack pointer */
    virtual void set_sp(uint32_t val) = 0;

    virtual uint32_t get_cpsr() = 0;

    virtual ThreadContext save_context() = 0;

    virtual void load_context(ThreadContext context) = 0;
};