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

#include <../SceSysmem/SceSysmem.h>
#include <module/module.h>

#include <kernel/state.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceCodecEngineUser);

enum SceCodecEngineErrorCode : uint32_t {
    SCE_CODECENGINE_ERROR_INVALID_POINTER = 0x80600000,
    SCE_CODECENGINE_ERROR_INVALID_SIZE = 0x80600001,
    SCE_CODECENGINE_ERROR_INVALID_ALIGNMENT = 0x80600002,
    SCE_CODECENGINE_ERROR_INVALID_HEAP = 0x80600005,
    SCE_CODECENGINE_ERROR_INVALID_VALUE = 0x80600009
};

EXPORT(uint32_t, sceCodecEngineAllocMemoryFromUnmapMemBlock, SceUID uid, uint32_t size, uint32_t alignment) {
    TRACY_FUNC(sceCodecEngineAllocMemoryFromUnmapMemBlock, uid, size, alignment);
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    auto it = emuenv.kernel.codec_blocks.find(uid);
    if (it == emuenv.kernel.codec_blocks.end()) {
        LOG_ERROR("Cannot allocate codec memory: unknown block {}", uid);
        return 0;
    }
    const uint32_t address = it->second.allocate(size, alignment);
    if (!address && size != 0)
        LOG_ERROR("Cannot allocate {} bytes aligned to {} in codec block {}", size, alignment, uid);
    return address;
}

EXPORT(int, sceCodecEngineCloseUnmapMemBlock, SceUID uid) {
    TRACY_FUNC(sceCodecEngineCloseUnmapMemBlock, uid);
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    if (!emuenv.kernel.codec_blocks.contains(uid))
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_VALUE);

    emuenv.kernel.codec_blocks.erase(uid);
    return 0;
}

EXPORT(int, sceCodecEngineFreeMemoryFromUnmapMemBlock, SceUID uid, Address address) {
    TRACY_FUNC(sceCodecEngineFreeMemoryFromUnmapMemBlock, uid, address);
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    auto it = emuenv.kernel.codec_blocks.find(uid);
    if (it == emuenv.kernel.codec_blocks.end())
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_VALUE);
    if (!it->second.release(address))
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_POINTER);
    return 0;
}

EXPORT(SceUID, sceCodecEngineOpenUnmapMemBlock, Address memBlock, uint32_t size) {
    TRACY_FUNC(sceCodecEngineOpenUnmapMemBlock, memBlock, size);
    if (!memBlock)
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_POINTER);
    if ((memBlock & 255) != 0)
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_ALIGNMENT);
    if (!size || (size & 255) != 0 || static_cast<uint64_t>(memBlock) + size > 0xfffff000ULL)
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_SIZE);
    if (!is_valid_addr_range(emuenv.mem, memBlock, memBlock + size))
        return RET_ERROR(SCE_CODECENGINE_ERROR_INVALID_POINTER);
    auto uid = emuenv.kernel.get_next_uid();
    auto guard = std::lock_guard<std::mutex>(emuenv.kernel.mutex);
    emuenv.kernel.codec_blocks.emplace(uid, CodecEngineBlock{ .base = memBlock, .size = size });
    return uid;
}
