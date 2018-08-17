// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <cpu/functions.h>

void write_return_value(CPUState &cpu, int32_t ret) {
    write_reg(cpu, 0, ret);
}

void write_return_value(CPUState &cpu, int64_t ret) {
    write_reg(cpu, 0, ret & UINT32_MAX);
    write_reg(cpu, 1, ret >> 32);
}

void write_return_value(CPUState &cpu, uint32_t ret) {
    write_reg(cpu, 0, ret);
}

void write_return_value(CPUState &cpu, uint64_t ret) {
    write_reg(cpu, 0, ret & UINT32_MAX);
    write_reg(cpu, 1, ret >> 32);
}
