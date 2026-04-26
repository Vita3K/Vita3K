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

#include <kernel/types.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

struct MemState;

// Shared SysmemState definition used by SceSysmem and kubridge HLE.

struct KernelMemBlock : SceKernelMemBlockInfo {
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

typedef std::shared_ptr<KernelMemBlock> KernelMemBlockPtr;
typedef std::map<SceUID, KernelMemBlockPtr> Blocks;

struct SysmemState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    SceUID next_uid = 1;

    uint32_t allocated_user = 0;
    uint32_t allocated_cdram = 0;
    uint32_t allocated_phycont = 0;

    SceUID get_next_uid() {
        return next_uid++;
    }
};
