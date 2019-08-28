#pragma once
#include <mem/ptr.h>

#include <psp2/kernel/threadmgr.h>
#include <psp2/types.h>

#include <atomic>

#define SCE_KERNEL_DEFAULT_PRIORITY static_cast<SceInt32>(0x10000100)
#define SCE_KERNEL_DEFAULT_PRIORITY_USER SCE_KERNEL_DEFAULT_PRIORITY
#define SCE_KERNEL_DEFAULT_PRIORITY_USER_INTERNAL SCE_KERNEL_DEFAULT_PRIORITY
#define SCE_KERNEL_HIGHEST_PRIORITY_USER 64
#define SCE_KERNEL_LOWEST_PRIORITY_USER 191

#define SCE_KERNEL_STACK_SIZE_USER_MAIN KB(256)
#define SCE_KERNEL_STACK_SIZE_USER_DEFAULT KB(4)

#define SCE_KERNEL_ATTR_TH_FIFO 0x00000000U
#define SCE_KERNEL_ATTR_TH_PRIO 0x00002000U
// #define SCE_KERNEL_MUTEX_ATTR_RECURSIVE 0x2U

#define SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO SCE_KERNEL_ATTR_TH_FIFO
#define SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO SCE_KERNEL_ATTR_TH_PRIO

#define KERNELOBJECT_MAX_NAME_LENGTH 31

#define SCE_ERROR_ERRNO_EINVAL 0x80010016

constexpr size_t MODULE_INFO_NUM_SEGMENTS = 4;

namespace emu {
struct SceKernelSegmentInfo {
    SceUInt size; //!< sizeof(SceKernelSegmentInfo)
    SceUInt perms; //!< probably rwx in low bits
    Ptr<const void> vaddr; //!< address in memory
    SceUInt memsz; //!< size in memory
    SceUInt flags; //!< meaning unknown
    SceUInt res; //!< unused?
};

struct SceKernelModuleInfo {
    SceUInt size; //!< 0x1B8 for Vita 1.x
    SceUInt handle; //!< kernel module handle?
    SceUInt flags; //!< some bits. could be priority or whatnot
    char module_name[28];
    SceUInt unk28;
    Ptr<const void> module_start;
    SceUInt unk30;
    Ptr<const void> module_stop;
    Ptr<const void> exidxTop;
    Ptr<const void> exidxBtm;
    SceUInt unk40;
    SceUInt unk44;
    Ptr<const void> tlsInit;
    SceSize tlsInitSize;
    SceSize tlsAreaSize;
    char path[256];
    SceKernelSegmentInfo segments[MODULE_INFO_NUM_SEGMENTS];
    SceUInt type; //!< 6 = user-mode PRX?
};

// We only use workarea for uid
struct SceKernelLwMutexWork {
    SceUID uid;

    std::uint8_t padding[28];
};

// We only use workarea for uid
struct SceKernelLwCondWork {
    SceUID uid;

    std::uint8_t padding[28];
};

static_assert(sizeof(SceKernelLwMutexWork) == 32, "Incorrect size");
} // namespace emu
