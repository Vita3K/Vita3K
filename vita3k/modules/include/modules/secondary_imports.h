// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kernel/types.h>

#include <cstdint>

inline bool secondary_import_allowed(uint32_t nid, uint32_t arg0, uint32_t arg2) {
    switch (nid) {
    case 0x02DB3F5F: // sceAudioOutOutput
    case 0x0FB972F9: // sceKernelGetThreadId
    case 0x12FB1767: // sceAudioOutGetAdopt
    case 0x0C7B834B: // sceKernelWaitSema
    case 0x120AFC8C: // sceKernelUnlockLwMutex2
    case 0x174692B4: // sceKernelWaitSemaCB
    case 0x1A372EC8: // sceKernelUnlockMutex
    case 0x1BD67366: // sceKernelCreateSema
    case 0x244E76D2: // sceKernelDeleteLwMutex
    case 0x297AA2AE: // sceKernelCreateSema_16XX
    case 0x2D635A00: // sceKernelGetExtraTty
    case 0x2E3B02A1: // sceClibMemcpy_safe
    case 0x33B83B70: // __cxa_atexit
    case 0x34EFD876: // sceIoWrite
    case 0x4010AD65: // sceKernelGetMemBlockInfoByAddr
    case 0x422A221A: // sceIoDclose
    case 0x48C7EAE6: // sceKernelCreateLwCond
    case 0x49252B9B: // sceIoLseek32
    case 0x499EA781: // sceKernelUnlockLwMutex_0
    case 0x4B675D05: // sceKernelDelayThread
    case 0x4EA13FEA: // sceKernelFreeMemBlockForVM
    case 0x52315AD7: // sceIoPread
    case 0x595D3FA6: // sceKernelGetSemaInfo
    case 0x5BC341E4: // sceAudioOutOpenPort
    case 0x64167F11: // sceAudioOutSetVolume
    case 0x69E2E6B5: // sceAudioOutReleasePort
    case 0x57F8CD25: // sceIoGetstatByFd
    case 0x5795E898: // sceDisplayWaitVblankStart
    case 0x66D6BF05: // sceKernelCancelSema
    case 0x6C60AC61: // sceIoOpen
    case 0x72FC1F54: // sceKernelTryLockMutex
    case 0x7A410B64: // sceDisplaySetFrameBuf
    case 0x721F6CB3: // sceKernelDeleteLwCond
    case 0x6DC1F0D8: // memset
    case 0x7205BFDB: // memcpy
    case 0x866EF048: // sceKernelPollSema
    case 0x8713D662: // _sceIoDread
    case 0x8E7E11F2: // _sceIoGetstat
    case 0x8DE6FEB7: // sceRtcGetTime_t
    case 0x811313B3: // sceRtcGetTickResolution
    case 0x8F1ACC32: // _sceIoMkdir
    case 0x8FFFF5A8: // sceIoPwrite
    case 0x91FA6614: // sceKernelUnlockLwMutex
    case 0x97C4A7C4: // sceKernelDelayThread200
    case 0x9DCB4B7A: // sceKernelGetProcessId
    case 0x9A5370C4: // sceAudioOutGetRestSample
    case 0x9C8B6624: // sceIoDread
    case 0x9EF798C1: // sceKernelTryLockLwMutex_16XX
    case 0x99BA173E: // sceIoLseek
    case 0xA33B99D1: // sceKernelFindMemBlockByAddr
    case 0xA6A2C915: // sceKernelTryLockLwMutex
    case 0xA91E15EE: // sceKernelFreeMemBlock
    case 0xA9283DD0: // sceIoDopen
    case 0xAAED7419: // sceAppMgrAcquireBgmPortWithPriority
    case 0xA604764A: // _sceIoLseek
    case 0xAF5C218D: // memmove
    case 0xAFCEAB96: // sceAppMgrAcquireBgmPort
    case 0xB110C123: // sceKernelGetProcessTimeWide
    case 0xB220B00B: // sceAppUtilShutdown
    case 0xB295EB61: // sceKernelGetTLSAddr
    case 0xB538BF48: // __cxa_finalize
    case 0xB8EF5818: // sceKernelGetMemBlockBase
    case 0xB8BA0D07: // sceAudioOutSetConfig
    case 0xB9D5EBDE: // sceKernelAllocMemBlock
    case 0xBCA5B623: // sceIoGetstat
    case 0xBFE02B3A: // __cxa_set_dso_handle_main
    case 0xC1727F59: // sceKernelGetStdin
    case 0xC5C11EE7: // sceKernelCreateThread
    case 0xC70B8886: // sceIoClose
    case 0xC995DE02: // sceRtcGetTime64_t
    case 0xCB78710D: // sceKernelDeleteMutex
    case 0xCC67B6FD: // _sceIoOpen
    case 0xDAFFE671: // sceAppUtilInit
    case 0xDDB395A9: // sceKernelWaitThreadEnd
    case 0xDD0A13B8: // sceDisplayWaitVblankStartMulti
    case 0xDA6EC8EF: // sceKernelCreateLwMutex
    case 0xDB32948A: // sceKernelDeleteSema
    case 0xE2D7E137: // sceKernelAllocMemBlockForVM
    case 0xE20ED0F3: // sceIoRemove
    case 0xE5AA625C: // sceKernelGetStdout
    case 0xE6B761D1: // sceKernelSignalSema
    case 0xE6C53567: // _sceIoGetstatByFd
    case 0xE6E614B5: // _sceIoDopen
    case 0xE1878282: // sceKernelWaitLwCond
    case 0x8FA54B07: // sceKernelWaitLwCondCB
    case 0x3AC63B9A: // sceKernelSignalLwCond
    case 0xE5241A0C: // sceKernelSignalLwCondAll
    case 0xE9F973B1: // sceKernelGetProcessTimeLow
    case 0xE9F91EC8: // sceIoRmdir
    case 0xEA5C52F5: // _sceKernelWaitThreadEnd
    case 0xEDC939E1: // __aeabi_atexit
    case 0xED53334A: // sceKernelCreateMutex
    case 0xF08DE149: // sceKernelStartThread
    case 0xF3717E37: // sceAppMgrReleaseBgmPort
    case 0x7333C2F1: // sceAppMgrSetBgmSubPriority
    case 0xF737E369: // sceIoRename
    case 0x9670D39F: // sceIoMkdir
    case 0xF4EE4FA9: // sceKernelGetSystemTimeWide
    case 0xFA5E3ADA: // sceKernelGetStderr
    case 0xFA26BC62: // sceClibPrintf
    case 0xFDB32293: // sceIoRead
        return true;
    case 0xC3C26339: // sceSysmoduleLoadModuleInternalWithArg
        return arg0 == SCE_SYSMODULE_INTERNAL_PAF;
    case 0x46E7BE7B: // sceKernelLockLwMutex
    case 0xA7819967: // sceKernelLockLwMutex_0
    case 0x1D8D7945: // sceKernelLockMutex
        return arg2 == 0;
    default:
        return false;
    }
}