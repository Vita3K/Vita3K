// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <../SceSysmem/SceSysmem.h>
#include <module/module.h>

#include <kernel/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceCodecEngineUser);

enum SceCodecEngineErrorCode : uint32_t {
    SCE_CODECENGINE_ERROR_INVALID_POINTER = 0x80600000,
    SCE_CODECENGINE_ERROR_INVALID_SIZE = 0x80600001,
    SCE_CODECENGINE_ERROR_INVALID_HEAP = 0x80600005,
    SCE_CODECENGINE_ERROR_INVALID_VALUE = 0x80600009
};

EXPORT(int32_t, sceCodecEngineAllocMemoryFromUnmapMemBlock, SceUID uid, uint32_t size, uint32_t alignment) {
    TRACY_FUNC(sceCodecEngineAllocMemoryFromUnmapMemBlock, uid, size, alignment);
    STUBBED("fake vaddr");
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    auto it = emuenv.kernel.codec_blocks.find(uid);
    if (it == emuenv.kernel.codec_blocks.end())
        return SCE_CODECENGINE_ERROR_INVALID_VALUE;
    auto &block = it->second;
    return 0xDEADBEAF + block.vaddr++;
}

EXPORT(int, sceCodecEngineCloseUnmapMemBlock, SceUID uid) {
    TRACY_FUNC(sceCodecEngineCloseUnmapMemBlock, uid);
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    if (!emuenv.kernel.codec_blocks.contains(uid))
        return SCE_CODECENGINE_ERROR_INVALID_VALUE;

    emuenv.kernel.codec_blocks.erase(uid);
    return 0;
}

EXPORT(int, sceCodecEngineFreeMemoryFromUnmapMemBlock) {
    TRACY_FUNC(sceCodecEngineFreeMemoryFromUnmapMemBlock);
    STUBBED("always return success");
    return 0;
}

EXPORT(SceUID, sceCodecEngineOpenUnmapMemBlock, Address memBlock, uint32_t size) {
    TRACY_FUNC(sceCodecEngineOpenUnmapMemBlock, memBlock, size);
    auto uid = emuenv.kernel.get_next_uid();
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    CodecEngineBlock block;
    block.size = size;
    emuenv.kernel.codec_blocks.emplace(uid, block);
    return uid;
}
