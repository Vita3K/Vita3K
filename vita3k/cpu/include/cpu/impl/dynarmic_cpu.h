// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <dynarmic/interface/A32/a32.h>

#include <cpu/state.h>

#include <memory>

class ArmDynarmicCallback;
class ArmDynarmicCP15;

class DynarmicCPU : public CPUState {
    friend class ArmDynarmicCallback;

    std::unique_ptr<Dynarmic::A32::Jit> jit;
    std::unique_ptr<ArmDynarmicCallback> cb;
    std::shared_ptr<ArmDynarmicCP15> cp15;

    bool log_mem = false;
    bool log_code = false;
    bool cpu_opt;

    std::unique_ptr<Dynarmic::A32::Jit> make_jit();

public:
    DynarmicCPU(MemState &mem, SceUID thread_id, std::size_t processor_id, bool cpu_opt);
    ~DynarmicCPU() override;
    GuestExit run(RunMode mode) override;
    void request_stop() override;
    void clear_exclusive() override;

    uint32_t get_reg(uint8_t idx) override;
    void set_reg(uint8_t idx, uint32_t val) override;

    uint32_t get_cpsr() override;
    void set_cpsr(uint32_t val) override;

    uint32_t get_tpidruro() override;
    void set_tpidruro(uint32_t val) override;

    uint32_t get_tpidrurw() override;
    void set_tpidrurw(uint32_t val) override;

    float get_float_reg(uint8_t idx) override;
    void set_float_reg(uint8_t idx, float val) override;

    uint32_t get_fpscr() override;
    void set_fpscr(uint32_t val) override;

    CPUContext save_context() override;
    void load_context(const CPUContext &ctx) override;

    void set_log_code(bool log) override;
    void set_log_mem(bool log) override;
    bool get_log_mem() override;
    void invalidate_jit_cache(Address start, size_t length) override;
};
