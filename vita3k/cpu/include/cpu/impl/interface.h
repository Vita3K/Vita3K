// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cpu/common.h>

#include <cstdint>
#include <memory>

/*! \brief Base class for all CPU backend implementation */
struct CPUInterface {
    virtual ~CPUInterface() = default;

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
    virtual void invalidate_jit_cache(Address start, size_t length) {}

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
