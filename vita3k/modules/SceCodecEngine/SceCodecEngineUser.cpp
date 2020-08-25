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

#include "SceCodecEngineUser.h"
#include <../SceSysmem/SceSysmem.h>

#include <kernel/state.h>

EXPORT(int32_t, sceCodecEngineAllocMemoryFromUnmapMemBlock, SceUID uid, uint32_t size, uint32_t alignment) {
    STUBBED("fake vaddr");
    auto guard = std::lock_guard<std::mutex>(host.kernel->mutex);
    auto it = host.kernel->codec_blocks.find(uid);
    if (it == host.kernel->codec_blocks.end())
        return SCE_CODECENGINE_ERROR_INVALID_VALUE;
    auto &block = it->second;
    return 1 + block.vaddr++;
}

EXPORT(int, sceCodecEngineCloseUnmapMemBlock, SceUID uid) {
    auto guard = std::lock_guard<std::mutex>(host.kernel->mutex);
    if (host.kernel->codec_blocks.find(uid) == host.kernel->codec_blocks.end())
        return SCE_CODECENGINE_ERROR_INVALID_VALUE;

    host.kernel->codec_blocks.erase(uid);
    return 0;
}

EXPORT(int, sceCodecEngineFreeMemoryFromUnmapMemBlock) {
    STUBBED("always return success");
    return 0;
}

EXPORT(SceUID, sceCodecEngineOpenUnmapMemBlock, Address memBlock, uint32_t size) {
    auto uid = host.kernel->get_next_uid();
    auto guard = std::lock_guard<std::mutex>(host.kernel->mutex);
    CodecEngineBlock block;
    block.size = size;
    host.kernel->codec_blocks.emplace(uid, block);
    return uid;
}

BRIDGE_IMPL(sceCodecEngineAllocMemoryFromUnmapMemBlock)
BRIDGE_IMPL(sceCodecEngineCloseUnmapMemBlock)
BRIDGE_IMPL(sceCodecEngineFreeMemoryFromUnmapMemBlock)
BRIDGE_IMPL(sceCodecEngineOpenUnmapMemBlock)
