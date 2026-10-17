// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "SceSysmemForDriver.h"
#include "memblock_validation.h"

#include <kernel/types.h>
#include <mem/functions.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

struct SecondaryMemblock {
    Block allocation;
    SceKernelMemBlockInfo info{};
    bool vm = false;
};

struct SecondaryMemblockState {
    std::mutex mutex;
    std::map<SceUID, SecondaryMemblock> blocks;
    uint64_t allocated_user = 0;
    SceSize vm_space_used = 0;
};

inline bool secondary_guest_copy(MemState &mem, Address address, void *data, uint32_t size, bool write) {
    const GuestMemoryRange range{ address, size };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    auto *bytes = static_cast<uint8_t *>(data);
    for (uint32_t i = 0; i < size; ++i) {
        auto *guest = Ptr<uint8_t>(address + i).get(mem);
        if (write)
            *guest = bytes[i];
        else
            bytes[i] = *guest;
    }
    unpin_guest_memory(mem, pin);
    return true;
}

inline bool secondary_block_name(MemState &mem, Address address, std::string &name) {
    if (!address)
        return false;
    for (uint32_t i = 0; i < SCE_UID_NAMELEN; ++i) {
        if (static_cast<uint64_t>(address) + i > UINT32_MAX)
            return false;
        char c;
        if (!secondary_guest_copy(mem, address + i, &c, 1, false))
            return false;
        if (!c)
            return true;
        name.push_back(c);
    }
    return true;
}

inline SceInt32 secondary_prepare_alloc(MemState &mem, Ptr<const char> name_ptr,
    SceKernelMemBlockType type, SceSize size, Ptr<SceKernelAllocMemBlockOpt> opt_ptr,
    bool vm, bool firmware, std::string &name, SceSize &alignment) {
    if (!secondary_block_name(mem, name_ptr.address(), name))
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    SceKernelAllocMemBlockOpt opt{};
    if (opt_ptr) {
        if (!secondary_guest_copy(mem, opt_ptr.address(), &opt.size, sizeof(opt.size), false))
            return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        constexpr SceSize sizes[] = { 0x14, 0x38, 0x3C, 0x40 };
        if (!std::ranges::contains(sizes, opt.size))
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
        const SceSize requested_size = opt.size;
        if (!secondary_guest_copy(mem, opt_ptr.address(), &opt, requested_size, false))
            return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        if (opt.size != requested_size)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
        constexpr SceUInt32 allowed = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ENABLE_OPEN;
        if ((opt.attr & ~(allowed | (firmware ? SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_0x20 : 0U)))
            || (opt.size == sizeof(opt) && opt.reserved[9]))
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        if (opt.attr & (SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ENABLE_OPEN | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT))
            return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
    }
    if (!vm) {
        if (const auto error = type_error(type))
            return error;
        if (const auto error = size_error(type, size))
            return error;
    } else if (size % MiB(1)) {
        return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE;
    } else if (!size) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    alignment = 0;
    if (opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) {
        if (const auto error = alignment_error(type, opt.alignment))
            return error;
        alignment = opt.alignment;
    }
    if (opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_0x20)
        LOG_WARN("Secondary memblock '{}' uses firmware attribute 0x20; its effect is not emulated", name);
    return 0;
}

inline Address secondary_block_region(SceKernelMemBlockType type) {
    const auto partition = type & MEMBLOCK_TYPE_PARTITION_INDEX_MASK;
    const bool uncached = (type & MEMBLOCK_TYPE_MEMORY_ATTR_MASK) == MEMBLOCK_TYPE_MEMORY_ATTR_NORMAL_NC;
    if (partition == MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDRAM)
        return USER_CDRAM_BASE;
    if (partition == MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDIALOG)
        return uncached ? USER_CDIALOG_UNCACHED_BASE : USER_CDIALOG_BASE;
    return uncached ? USER_UNCACHED_BASE : USER_MAIN_BASE;
}

inline bool secondary_takes_user(SceKernelMemBlockType type) {
    const auto part = type & MEMBLOCK_TYPE_PHYMEMPART_MASK;
    return (type & MEMBLOCK_TYPE_PARTITION_INDEX_MASK) != MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDIALOG
        && part != MEMBLOCK_TYPE_PHYMEMPART_CDRAM && part != MEMBLOCK_TYPE_PHYMEMPART_PHYCONT
        && part != MEMBLOCK_TYPE_PHYMEMPART_CDIALOG;
}

inline SceInt32 secondary_alloc_block(SecondaryMemblockState &state, MemState &mem, SceUID uid,
    const std::string &name, SceKernelMemBlockType type, SceSize size, SceSize requested_alignment, bool vm) {
    std::lock_guard lock(state.mutex);
    if (vm && size > MiB(16) - state.vm_space_used)
        return SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
    if (secondary_takes_user(type) && state.allocated_user + size > MiB(256))
        return SCE_KERNEL_ERROR_NO_MEMORY;
    const auto part = type & MEMBLOCK_TYPE_PHYMEMPART_MASK;
    SceSize alignment = part == MEMBLOCK_TYPE_PHYMEMPART_CDRAM ? MEMBLOCK_CDRAM_MIN_ALIGNMENT
        : part == MEMBLOCK_TYPE_PHYMEMPART_PHYCONT             ? MEMBLOCK_PHYCONT_MIN_ALIGNMENT
                                                               : MEMBLOCK_MIN_ALIGNMENT;
    alignment = std::max({ alignment, requested_alignment, size & -size });
    const Address address = alloc_aligned(mem, size, name.c_str(), alignment, secondary_block_region(type));
    if (!address)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    SecondaryMemblock block;
    block.allocation = Block(address, [&mem](Address addr) { free(mem, addr); });
    block.info.size = sizeof(SceKernelMemBlockInfo);
    block.info.type = type;
    block.info.mappedBase = Ptr<void>(address);
    block.info.mappedSize = size;
    block.info.memoryType = ((type & MEMBLOCK_TYPE_MEMORY_ATTR_MASK) >> MEMBLOCK_TYPE_MEMORY_ATTR_SHIFT) & SCE_KERNEL_MEMORY_TYPE_NORMAL;
    block.info.access = ((type & MEMBLOCK_TYPE_USER_ACCESS_MASK) >> MEMBLOCK_TYPE_USER_ACCESS_SHIFT) & (SCE_KERNEL_MEMORY_ACCESS_R | SCE_KERNEL_MEMORY_ACCESS_W);
    block.vm = vm;
    state.blocks.emplace(uid, std::move(block));
    if (secondary_takes_user(type))
        state.allocated_user += size;
    if (vm)
        state.vm_space_used += size;
    return uid;
}

inline SceInt32 secondary_free_block(SecondaryMemblockState &state, SceUID uid, bool vm_only) {
    std::lock_guard lock(state.mutex);
    const auto it = state.blocks.find(uid);
    if (it == state.blocks.end())
        return vm_only ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : SCE_KERNEL_ERROR_INVALID_UID;
    if (vm_only && !it->second.vm)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (secondary_takes_user(it->second.info.type))
        state.allocated_user -= it->second.info.mappedSize;
    if (it->second.vm)
        state.vm_space_used -= it->second.info.mappedSize;
    state.blocks.erase(it);
    return SCE_KERNEL_OK;
}

inline SceInt32 secondary_block_base(SecondaryMemblockState &state, MemState &mem, SceUID uid, Address output) {
    std::lock_guard lock(state.mutex);
    const auto it = state.blocks.find(uid);
    if (it == state.blocks.end())
        return SCE_KERNEL_ERROR_INVALID_UID;
    Address base = it->second.info.mappedBase.address();
    return secondary_guest_copy(mem, output, &base, sizeof(base), true) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_ILLEGAL_ADDR;
}

inline SceInt32 secondary_find_block(SecondaryMemblockState &state, MemState &mem, Address address, Address output, bool info) {
    std::lock_guard lock(state.mutex);
    for (const auto &[uid, block] : state.blocks) {
        const uint64_t base = block.info.mappedBase.address();
        if (address >= base && static_cast<uint64_t>(address) < base + block.info.mappedSize) {
            if (!info)
                return uid;
            auto copy = block.info;
            return secondary_guest_copy(mem, output, &copy, sizeof(copy), true) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        }
    }
    return SCE_KERNEL_ERROR_BLOCK_ERROR;
}
