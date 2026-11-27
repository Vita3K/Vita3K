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

#include <cpu/common.h>
#include <mem/ptr.h>

#include <cstdlib>

// The result of an export that returns a value, for the kernel to write to the registers
CallResult call_result(int32_t ret);
CallResult call_result(int64_t ret);
CallResult call_result(uint32_t ret);
CallResult call_result(uint64_t ret);
CallResult call_result(std::div_t ret);
CallResult call_result(std::lldiv_t ret);
CallResult call_result(bool ret);
CallResult call_result(float ret);

template <typename Pointee>
CallResult call_result(const Ptr<Pointee> &ret) {
    return call_result(ret.address());
}
