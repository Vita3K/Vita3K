#pragma once
#include <mem/ptr.h>
#include <psp2/types.h>

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
        SceKernelSegmentInfo segments[4];
        SceUInt type; //!< 6 = user-mode PRX?
    };
}
