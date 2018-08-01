#pragma once

#include <cpu/interface.h>

#include <unicorn/unicorn.h>

#include <functional>
#include <array>
#include <memory>

typedef std::unique_ptr<uc_struct, std::function<void(uc_struct *)>> UnicornPtr;

struct CPUState;

/*! \brief Base class for all CPU backend implementation */
class UnicornCPU: public CPUInterface {
    UnicornPtr uc;

public:
    UnicornCPU(CPUState *state, Address pc, Address sp, bool log_code);

    /*! \brief Execute a amount of instructions. */
    bool execute_instructions(int num);

    /*! Run the CPU */
    void run() override;

    /*! Stop the CPU */
    void stop() override;

    /*! Get a specific ARM Rx register. Range from r0 to r15 */
    uint32_t get_reg(uint8_t idx) override;

    /*! Get the stack pointer */
    uint32_t get_sp() override;

    /*! Get the program counter */
    uint32_t get_pc() override;

    uint32_t get_lr() override;

    /*! Set a Rx register */
    void set_reg(uint8_t idx, uint32_t val) override;

    void set_cpsr(uint32_t val) override;

    /*! Set program counter */
    void set_pc(uint32_t val) override;

    /*! Set LR */
    void set_lr(uint32_t val) override;

    /*! Set stack pointer */
    void set_sp(uint32_t val) override;

    uint32_t get_cpsr() override;

    float get_float_reg(uint8_t idx) override;

    ThreadContext save_context() override;

    void load_context(UnicornCPU::ThreadContext context) override;
};