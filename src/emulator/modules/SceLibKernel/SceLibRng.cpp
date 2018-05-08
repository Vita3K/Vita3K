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

#include "SceLibRng.h"

#include <psp2/kernel/error.h>

#include <algorithm>
#include <random>

#define SCE_RNG_ERROR_INVALID_ARGUMENT 0x810C0000

EXPORT(int, sceKernelGetRandomNumber, uint64_t *output, unsigned int size) {
    if (size > 64) {
        return RET_ERROR(export_name, SCE_RNG_ERROR_INVALID_ARGUMENT);
    }

    thread_local std::random_device dev;
    thread_local std::mt19937_64 mt(dev());
    thread_local auto rng = [&]() { return mt(); };

    int repeat = size / sizeof(uint64_t);
    int remain = size % sizeof(uint64_t);

    std::generate_n(output, repeat, rng);

    if (remain == 0) {
        return SCE_KERNEL_OK;
    }

    uint64_t value = rng();
    char *ptr = reinterpret_cast<char*>(&value);
    std::copy(ptr, ptr + remain, reinterpret_cast<char*>(&output[repeat]));
    return SCE_KERNEL_OK;
}

BRIDGE_IMPL(sceKernelGetRandomNumber)
