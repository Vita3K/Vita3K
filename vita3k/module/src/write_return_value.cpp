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

#include <module/write_return_value.h>

#include <cpu/state.h>

void write_return_value(CPUState &cpu, int32_t ret) {
    cpu.set_reg(0, ret);
}

void write_return_value(CPUState &cpu, int64_t ret) {
    cpu.set_reg(0, ret & UINT32_MAX);
    cpu.set_reg(1, ret >> 32);
}

void write_return_value(CPUState &cpu, uint32_t ret) {
    cpu.set_reg(0, ret);
}

void write_return_value(CPUState &cpu, uint64_t ret) {
    cpu.set_reg(0, ret & UINT32_MAX);
    cpu.set_reg(1, ret >> 32);
}

void write_return_value(CPUState &cpu, std::div_t ret) {
    cpu.set_reg(0, ret.quot);
    cpu.set_reg(1, ret.rem);
}

void write_return_value(CPUState &cpu, std::lldiv_t ret) {
    cpu.set_reg(0, ret.quot & UINT32_MAX);
    cpu.set_reg(1, ret.quot >> 32);
    cpu.set_reg(2, ret.rem & UINT32_MAX);
    cpu.set_reg(3, ret.rem >> 32);
}

void write_return_value(CPUState &cpu, bool ret) {
    cpu.set_reg(0, ret);
}

void write_return_value(CPUState &cpu, float ret) {
    cpu.set_float_reg(0, ret);
}
