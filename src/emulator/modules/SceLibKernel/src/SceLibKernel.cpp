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

#include <SceLibKernel/exports.h>

#include <host/functions.h>
#include <io/functions.h>
#include <kernel/functions.h>
#include <kernel/thread_functions.h>

#include <SDL_thread.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>

struct Semaphore {
};

struct ThreadParams {
    HostState *host = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    SceSize arglen = 0;
    Ptr<void> argp;
    std::shared_ptr<SDL_semaphore> host_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);

    const ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SemPost(params.host_may_destroy_params.get());

    const ThreadStatePtr thread = lock_and_find(params.thid, params.host->kernel.threads, params.host->kernel.mutex);
    write_reg(*thread->cpu, 0, params.arglen);
    write_reg(*thread->cpu, 0, params.argp.address());

    const bool succeeded = run_thread(*thread);
    assert(succeeded);

    const uint32_t r0 = read_reg(*thread->cpu, 0);

    return r0;
}

EXPORT(int, SceKernelStackChkGuard) {
    return unimplemented("SceKernelStackChkGuard");
}

EXPORT(int, __sce_aeabi_idiv0) {
    return unimplemented("__sce_aeabi_idiv0");
}

EXPORT(int, __sce_aeabi_ldiv0) {
    return unimplemented("__sce_aeabi_ldiv0");
}

EXPORT(int, __stack_chk_fail) {
    return unimplemented("__stack_chk_fail");
}

EXPORT(int, __stack_chk_guard) {
    return unimplemented("__stack_chk_guard");
}

EXPORT(int, _sceKernelCreateLwMutex) {
    return unimplemented("_sceKernelCreateLwMutex");
}

EXPORT(int, sceClibAbort) {
    return unimplemented("sceClibAbort");
}

EXPORT(int, sceClibDprintf) {
    return unimplemented("sceClibDprintf");
}

EXPORT(int, sceClibLookCtypeTable) {
    return unimplemented("sceClibLookCtypeTable");
}

EXPORT(int, sceClibMemchr) {
    return unimplemented("sceClibMemchr");
}

EXPORT(int, sceClibMemcmp) {
    return unimplemented("sceClibMemcmp");
}

EXPORT(int, sceClibMemcmpConstTime) {
    return unimplemented("sceClibMemcmpConstTime");
}

EXPORT(int, sceClibMemcpy) {
    return unimplemented("sceClibMemcpy");
}

EXPORT(int, sceClibMemcpyChk) {
    return unimplemented("sceClibMemcpyChk");
}

EXPORT(int, sceClibMemcpy_safe) {
    return unimplemented("sceClibMemcpy_safe");
}

EXPORT(int, sceClibMemmove) {
    return unimplemented("sceClibMemmove");
}

EXPORT(int, sceClibMemmoveChk) {
    return unimplemented("sceClibMemmoveChk");
}

EXPORT(int, sceClibMemset) {
    return unimplemented("sceClibMemset");
}

EXPORT(int, sceClibMemsetChk) {
    return unimplemented("sceClibMemsetChk");
}

EXPORT(int, sceClibMspaceCalloc) {
    return unimplemented("sceClibMspaceCalloc");
}

EXPORT(int, sceClibMspaceCreate) {
    return unimplemented("sceClibMspaceCreate");
}

EXPORT(int, sceClibMspaceDestroy) {
    return unimplemented("sceClibMspaceDestroy");
}

EXPORT(int, sceClibMspaceFree) {
    return unimplemented("sceClibMspaceFree");
}

EXPORT(int, sceClibMspaceIsHeapEmpty) {
    return unimplemented("sceClibMspaceIsHeapEmpty");
}

EXPORT(int, sceClibMspaceMalloc) {
    return unimplemented("sceClibMspaceMalloc");
}

EXPORT(int, sceClibMspaceMallocStats) {
    return unimplemented("sceClibMspaceMallocStats");
}

EXPORT(int, sceClibMspaceMallocStatsFast) {
    return unimplemented("sceClibMspaceMallocStatsFast");
}

EXPORT(int, sceClibMspaceMallocUsableSize) {
    return unimplemented("sceClibMspaceMallocUsableSize");
}

EXPORT(int, sceClibMspaceMemalign) {
    return unimplemented("sceClibMspaceMemalign");
}

EXPORT(int, sceClibMspaceRealloc) {
    return unimplemented("sceClibMspaceRealloc");
}

EXPORT(int, sceClibMspaceReallocalign) {
    return unimplemented("sceClibMspaceReallocalign");
}

EXPORT(int, sceClibPrintf) {
    return unimplemented("sceClibPrintf");
}

EXPORT(int, sceClibSnprintf) {
    return unimplemented("sceClibSnprintf");
}

EXPORT(int, sceClibSnprintfChk) {
    return unimplemented("sceClibSnprintfChk");
}

EXPORT(int, sceClibStrcatChk) {
    return unimplemented("sceClibStrcatChk");
}

EXPORT(int, sceClibStrchr) {
    return unimplemented("sceClibStrchr");
}

EXPORT(int, sceClibStrcmp) {
    return unimplemented("sceClibStrcmp");
}

EXPORT(int, sceClibStrcpyChk) {
    return unimplemented("sceClibStrcpyChk");
}

EXPORT(int, sceClibStrlcat) {
    return unimplemented("sceClibStrlcat");
}

EXPORT(int, sceClibStrlcatChk) {
    return unimplemented("sceClibStrlcatChk");
}

EXPORT(int, sceClibStrlcpy) {
    return unimplemented("sceClibStrlcpy");
}

EXPORT(int, sceClibStrlcpyChk) {
    return unimplemented("sceClibStrlcpyChk");
}

EXPORT(int, sceClibStrncasecmp) {
    return unimplemented("sceClibStrncasecmp");
}

EXPORT(int, sceClibStrncat) {
    return unimplemented("sceClibStrncat");
}

EXPORT(int, sceClibStrncatChk) {
    return unimplemented("sceClibStrncatChk");
}

EXPORT(int, sceClibStrncmp) {
    return unimplemented("sceClibStrncmp");
}

EXPORT(int, sceClibStrncpy) {
    return unimplemented("sceClibStrncpy");
}

EXPORT(int, sceClibStrncpyChk) {
    return unimplemented("sceClibStrncpyChk");
}

EXPORT(int, sceClibStrnlen) {
    return unimplemented("sceClibStrnlen");
}

EXPORT(int, sceClibStrrchr) {
    return unimplemented("sceClibStrrchr");
}

EXPORT(int, sceClibStrstr) {
    return unimplemented("sceClibStrstr");
}

EXPORT(int, sceClibStrtoll) {
    return unimplemented("sceClibStrtoll");
}

EXPORT(int, sceClibTolower) {
    return unimplemented("sceClibTolower");
}

EXPORT(int, sceClibToupper) {
    return unimplemented("sceClibToupper");
}

EXPORT(int, sceClibVdprintf) {
    return unimplemented("sceClibVdprintf");
}

EXPORT(int, sceClibVprintf) {
    return unimplemented("sceClibVprintf");
}

EXPORT(int, sceClibVsnprintf) {
    return unimplemented("sceClibVsnprintf");
}

EXPORT(int, sceClibVsnprintfChk) {
    return unimplemented("sceClibVsnprintfChk");
}

EXPORT(int, sceIoChstat) {
    return unimplemented("sceIoChstat");
}

EXPORT(int, sceIoChstatByFd) {
    return unimplemented("sceIoChstatByFd");
}

EXPORT(int, sceIoDevctl) {
    return unimplemented("sceIoDevctl");
}

EXPORT(int, sceIoDopen) {
    return unimplemented("sceIoDopen");
}

EXPORT(int, sceIoDread) {
    return unimplemented("sceIoDread");
}

EXPORT(int, sceIoGetstat) {
    return unimplemented("sceIoGetstat");
}

EXPORT(int, sceIoGetstatByFd) {
    return unimplemented("sceIoGetstatByFd");
}

EXPORT(int, sceIoIoctl) {
    return unimplemented("sceIoIoctl");
}

EXPORT(int, sceIoIoctlAsync) {
    return unimplemented("sceIoIoctlAsync");
}

EXPORT(int, sceIoLseek) {
    return unimplemented("sceIoLseek");
}

EXPORT(int, sceIoMkdir, const char *dir, SceMode mode) {
    return create_dir(dir, mode, host.pref_path.c_str());
}

EXPORT(int, sceIoLseekAsync) {
    return unimplemented("sceIoLseekAsync");
}

EXPORT(SceUID, sceIoOpen, const char *file, int flags, SceMode mode) {
    if (file == nullptr){
        return error("sceIoOpen", 0x80010016); // SCE_ERROR_ERRNO_EINVAL, missing in vita-headers
    }
    return open_file(host.io, file, flags, host.pref_path.c_str());
}

EXPORT(int, sceIoOpenAsync) {
    return unimplemented("sceIoOpenAsync");
}

EXPORT(int, sceIoPread) {
    return unimplemented("sceIoPread");
}

EXPORT(int, sceIoPwrite) {
    return unimplemented("sceIoPwrite");
}

EXPORT(int, sceIoRemove) {
    return unimplemented("sceIoRemove");
}

EXPORT(int, sceIoRename) {
    return unimplemented("sceIoRename");
}

EXPORT(int, sceIoRmdir) {
    return unimplemented("sceIoRmdir");
}

EXPORT(int, sceIoSync) {
    return unimplemented("sceIoSync");
}

EXPORT(int, sceKernelAtomicAddAndGet16) {
    return unimplemented("sceKernelAtomicAddAndGet16");
}

EXPORT(int, sceKernelAtomicAddAndGet32) {
    return unimplemented("sceKernelAtomicAddAndGet32");
}

EXPORT(int, sceKernelAtomicAddAndGet64) {
    return unimplemented("sceKernelAtomicAddAndGet64");
}

EXPORT(int, sceKernelAtomicAddAndGet8) {
    return unimplemented("sceKernelAtomicAddAndGet8");
}

EXPORT(int, sceKernelAtomicAddUnless16) {
    return unimplemented("sceKernelAtomicAddUnless16");
}

EXPORT(int, sceKernelAtomicAddUnless32) {
    return unimplemented("sceKernelAtomicAddUnless32");
}

EXPORT(int, sceKernelAtomicAddUnless64) {
    return unimplemented("sceKernelAtomicAddUnless64");
}

EXPORT(int, sceKernelAtomicAddUnless8) {
    return unimplemented("sceKernelAtomicAddUnless8");
}

EXPORT(int, sceKernelAtomicAndAndGet16) {
    return unimplemented("sceKernelAtomicAndAndGet16");
}

EXPORT(int, sceKernelAtomicAndAndGet32) {
    return unimplemented("sceKernelAtomicAndAndGet32");
}

EXPORT(int, sceKernelAtomicAndAndGet64) {
    return unimplemented("sceKernelAtomicAndAndGet64");
}

EXPORT(int, sceKernelAtomicAndAndGet8) {
    return unimplemented("sceKernelAtomicAndAndGet8");
}

EXPORT(int, sceKernelAtomicClearAndGet16) {
    return unimplemented("sceKernelAtomicClearAndGet16");
}

EXPORT(int, sceKernelAtomicClearAndGet32) {
    return unimplemented("sceKernelAtomicClearAndGet32");
}

EXPORT(int, sceKernelAtomicClearAndGet64) {
    return unimplemented("sceKernelAtomicClearAndGet64");
}

EXPORT(int, sceKernelAtomicClearAndGet8) {
    return unimplemented("sceKernelAtomicClearAndGet8");
}

EXPORT(int, sceKernelAtomicClearMask16) {
    return unimplemented("sceKernelAtomicClearMask16");
}

EXPORT(int, sceKernelAtomicClearMask32) {
    return unimplemented("sceKernelAtomicClearMask32");
}

EXPORT(int, sceKernelAtomicClearMask64) {
    return unimplemented("sceKernelAtomicClearMask64");
}

EXPORT(int, sceKernelAtomicClearMask8) {
    return unimplemented("sceKernelAtomicClearMask8");
}

EXPORT(int, sceKernelAtomicCompareAndSet16) {
    return unimplemented("sceKernelAtomicCompareAndSet16");
}

EXPORT(int, sceKernelAtomicCompareAndSet32) {
    return unimplemented("sceKernelAtomicCompareAndSet32");
}

EXPORT(int, sceKernelAtomicCompareAndSet64) {
    return unimplemented("sceKernelAtomicCompareAndSet64");
}

EXPORT(int, sceKernelAtomicCompareAndSet8) {
    return unimplemented("sceKernelAtomicCompareAndSet8");
}

EXPORT(int, sceKernelAtomicDecIfPositive16) {
    return unimplemented("sceKernelAtomicDecIfPositive16");
}

EXPORT(int, sceKernelAtomicDecIfPositive32) {
    return unimplemented("sceKernelAtomicDecIfPositive32");
}

EXPORT(int, sceKernelAtomicDecIfPositive64) {
    return unimplemented("sceKernelAtomicDecIfPositive64");
}

EXPORT(int, sceKernelAtomicDecIfPositive8) {
    return unimplemented("sceKernelAtomicDecIfPositive8");
}

EXPORT(int, sceKernelAtomicGetAndAdd16) {
    return unimplemented("sceKernelAtomicGetAndAdd16");
}

EXPORT(int, sceKernelAtomicGetAndAdd32) {
    return unimplemented("sceKernelAtomicGetAndAdd32");
}

EXPORT(int, sceKernelAtomicGetAndAdd64) {
    return unimplemented("sceKernelAtomicGetAndAdd64");
}

EXPORT(int, sceKernelAtomicGetAndAdd8) {
    return unimplemented("sceKernelAtomicGetAndAdd8");
}

EXPORT(int, sceKernelAtomicGetAndAnd16) {
    return unimplemented("sceKernelAtomicGetAndAnd16");
}

EXPORT(int, sceKernelAtomicGetAndAnd32) {
    return unimplemented("sceKernelAtomicGetAndAnd32");
}

EXPORT(int, sceKernelAtomicGetAndAnd64) {
    return unimplemented("sceKernelAtomicGetAndAnd64");
}

EXPORT(int, sceKernelAtomicGetAndAnd8) {
    return unimplemented("sceKernelAtomicGetAndAnd8");
}

EXPORT(int, sceKernelAtomicGetAndClear16) {
    return unimplemented("sceKernelAtomicGetAndClear16");
}

EXPORT(int, sceKernelAtomicGetAndClear32) {
    return unimplemented("sceKernelAtomicGetAndClear32");
}

EXPORT(int, sceKernelAtomicGetAndClear64) {
    return unimplemented("sceKernelAtomicGetAndClear64");
}

EXPORT(int, sceKernelAtomicGetAndClear8) {
    return unimplemented("sceKernelAtomicGetAndClear8");
}

EXPORT(int, sceKernelAtomicGetAndOr16) {
    return unimplemented("sceKernelAtomicGetAndOr16");
}

EXPORT(int, sceKernelAtomicGetAndOr32) {
    return unimplemented("sceKernelAtomicGetAndOr32");
}

EXPORT(int, sceKernelAtomicGetAndOr64) {
    return unimplemented("sceKernelAtomicGetAndOr64");
}

EXPORT(int, sceKernelAtomicGetAndOr8) {
    return unimplemented("sceKernelAtomicGetAndOr8");
}

EXPORT(int, sceKernelAtomicGetAndSet16) {
    return unimplemented("sceKernelAtomicGetAndSet16");
}

EXPORT(int, sceKernelAtomicGetAndSet32) {
    return unimplemented("sceKernelAtomicGetAndSet32");
}

EXPORT(int, sceKernelAtomicGetAndSet64) {
    return unimplemented("sceKernelAtomicGetAndSet64");
}

EXPORT(int, sceKernelAtomicGetAndSet8) {
    return unimplemented("sceKernelAtomicGetAndSet8");
}

EXPORT(int, sceKernelAtomicGetAndSub16) {
    return unimplemented("sceKernelAtomicGetAndSub16");
}

EXPORT(int, sceKernelAtomicGetAndSub32) {
    return unimplemented("sceKernelAtomicGetAndSub32");
}

EXPORT(int, sceKernelAtomicGetAndSub64) {
    return unimplemented("sceKernelAtomicGetAndSub64");
}

EXPORT(int, sceKernelAtomicGetAndSub8) {
    return unimplemented("sceKernelAtomicGetAndSub8");
}

EXPORT(int, sceKernelAtomicGetAndXor16) {
    return unimplemented("sceKernelAtomicGetAndXor16");
}

EXPORT(int, sceKernelAtomicGetAndXor32) {
    return unimplemented("sceKernelAtomicGetAndXor32");
}

EXPORT(int, sceKernelAtomicGetAndXor64) {
    return unimplemented("sceKernelAtomicGetAndXor64");
}

EXPORT(int, sceKernelAtomicGetAndXor8) {
    return unimplemented("sceKernelAtomicGetAndXor8");
}

EXPORT(int, sceKernelAtomicOrAndGet16) {
    return unimplemented("sceKernelAtomicOrAndGet16");
}

EXPORT(int, sceKernelAtomicOrAndGet32) {
    return unimplemented("sceKernelAtomicOrAndGet32");
}

EXPORT(int, sceKernelAtomicOrAndGet64) {
    return unimplemented("sceKernelAtomicOrAndGet64");
}

EXPORT(int, sceKernelAtomicOrAndGet8) {
    return unimplemented("sceKernelAtomicOrAndGet8");
}

EXPORT(int, sceKernelAtomicSet16) {
    return unimplemented("sceKernelAtomicSet16");
}

EXPORT(int, sceKernelAtomicSet32) {
    return unimplemented("sceKernelAtomicSet32");
}

EXPORT(int, sceKernelAtomicSet64) {
    return unimplemented("sceKernelAtomicSet64");
}

EXPORT(int, sceKernelAtomicSet8) {
    return unimplemented("sceKernelAtomicSet8");
}

EXPORT(int, sceKernelAtomicSubAndGet16) {
    return unimplemented("sceKernelAtomicSubAndGet16");
}

EXPORT(int, sceKernelAtomicSubAndGet32) {
    return unimplemented("sceKernelAtomicSubAndGet32");
}

EXPORT(int, sceKernelAtomicSubAndGet64) {
    return unimplemented("sceKernelAtomicSubAndGet64");
}

EXPORT(int, sceKernelAtomicSubAndGet8) {
    return unimplemented("sceKernelAtomicSubAndGet8");
}

EXPORT(int, sceKernelAtomicXorAndGet16) {
    return unimplemented("sceKernelAtomicXorAndGet16");
}

EXPORT(int, sceKernelAtomicXorAndGet32) {
    return unimplemented("sceKernelAtomicXorAndGet32");
}

EXPORT(int, sceKernelAtomicXorAndGet64) {
    return unimplemented("sceKernelAtomicXorAndGet64");
}

EXPORT(int, sceKernelAtomicXorAndGet8) {
    return unimplemented("sceKernelAtomicXorAndGet8");
}

EXPORT(int, sceKernelBacktrace) {
    return unimplemented("sceKernelBacktrace");
}

EXPORT(int, sceKernelBacktraceSelf) {
    return unimplemented("sceKernelBacktraceSelf");
}

EXPORT(int, sceKernelCallModuleExit) {
    return unimplemented("sceKernelCallModuleExit");
}

EXPORT(int, sceKernelCallWithChangeStack) {
    return unimplemented("sceKernelCallWithChangeStack");
}

EXPORT(int, sceKernelCancelEvent) {
    return unimplemented("sceKernelCancelEvent");
}

EXPORT(int, sceKernelCancelEventFlag) {
    return unimplemented("sceKernelCancelEventFlag");
}

EXPORT(int, sceKernelCancelEventWithSetPattern) {
    return unimplemented("sceKernelCancelEventWithSetPattern");
}

EXPORT(int, sceKernelCancelMsgPipe) {
    return unimplemented("sceKernelCancelMsgPipe");
}

EXPORT(int, sceKernelCancelMutex) {
    return unimplemented("sceKernelCancelMutex");
}

EXPORT(int, sceKernelCancelRWLock) {
    return unimplemented("sceKernelCancelRWLock");
}

EXPORT(int, sceKernelCancelSema) {
    return unimplemented("sceKernelCancelSema");
}

EXPORT(int, sceKernelCancelTimer) {
    return unimplemented("sceKernelCancelTimer");
}

EXPORT(int, sceKernelChangeCurrentThreadAttr) {
    return unimplemented("sceKernelChangeCurrentThreadAttr");
}

EXPORT(int, sceKernelCheckThreadStack) {
    return unimplemented("sceKernelCheckThreadStack");
}

EXPORT(int, sceKernelCloseModule) {
    return unimplemented("sceKernelCloseModule");
}

EXPORT(int, sceKernelCreateCond) {
    return unimplemented("sceKernelCreateCond");
}

EXPORT(int, sceKernelCreateEventFlag) {
    return unimplemented("sceKernelCreateEventFlag");
}

EXPORT(int, sceKernelCreateLwCond) {
    return unimplemented("sceKernelCreateLwCond");
}

EXPORT(int, sceKernelCreateLwMutex, SceKernelLwMutexWork *pWork, const char *pName, unsigned int attr, int initCount, const SceKernelLwMutexOptParam *pOptParam) {
    assert(pWork != nullptr);
    assert(pName != nullptr);
    assert((attr == 0) || (attr == 2));
    assert(initCount >= 0);
    assert(initCount <= 1);
    assert(pOptParam == nullptr);

    return unimplemented("sceKernelCreateLwMutex");
}

EXPORT(int, sceKernelCreateMsgPipe) {
    return unimplemented("sceKernelCreateMsgPipe");
}

EXPORT(int, sceKernelCreateMutex) {
    return unimplemented("sceKernelCreateMutex");
}

EXPORT(int, sceKernelCreateRWLock) {
    return unimplemented("sceKernelCreateRWLock");
}

EXPORT(SceUID, sceKernelCreateSema, const char *name, SceUInt attr, int initVal, int maxVal, SceKernelSemaOptParam *option) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)){
        return error("sceKernelCreateSema", SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    
    const SemaphorePtr semaphore = std::make_shared<Semaphore>();
    const std::unique_lock<std::mutex> lock(host.kernel.mutex);
    const SceUID uid = host.kernel.next_uid++;
    host.kernel.semaphores.emplace(uid, semaphore);

    return uid;
}

EXPORT(int, sceKernelCreateSimpleEvent) {
    return unimplemented("sceKernelCreateSimpleEvent");
}

EXPORT(SceUID, sceKernelCreateThread, const char *name, emu::SceKernelThreadEntry entry, int initPriority, int stackSize, SceUInt attr, int cpuAffinityMask, const SceKernelThreadOptParam *option) {
    if (cpuAffinityMask > 0x70000){
        return error("sceKernelCreateThread", SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }
    
    WaitingThreadState waiting;
    waiting.name = name;

    const SceUID thid = host.kernel.next_uid++;
    const CallImport call_import = [&host, thid](uint32_t nid) {
        ::call_import(host, nid, thid);
    };

    const bool log_code = false;
    const ThreadStatePtr thread = init_thread(entry.cast<const void>(), stackSize, log_code, host.mem, call_import);
    if (!thread) {
        return error("sceKernelCreateThread", SCE_KERNEL_ERROR_ERROR);
    }

    const std::unique_lock<std::mutex> lock(host.kernel.mutex);
    host.kernel.threads.emplace(thid, thread);
    host.kernel.waiting_threads.emplace(thid, waiting);

    return thid;
}

EXPORT(int, sceKernelCreateTimer) {
    return unimplemented("sceKernelCreateTimer");
}

EXPORT(int, sceKernelDeleteLwCond) {
    return unimplemented("sceKernelDeleteLwCond");
}

EXPORT(int, sceKernelDeleteLwMutex) {
    return unimplemented("sceKernelDeleteLwMutex");
}

EXPORT(int, sceKernelExitProcess, int res) {
    // TODO Handle exit code?
    stop_all_threads(host.kernel);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetCallbackInfo) {
    return unimplemented("sceKernelGetCallbackInfo");
}

EXPORT(int, sceKernelGetCondInfo) {
    return unimplemented("sceKernelGetCondInfo");
}

EXPORT(int, sceKernelGetCurrentThreadVfpException) {
    return unimplemented("sceKernelGetCurrentThreadVfpException");
}

EXPORT(int, sceKernelGetEventFlagInfo) {
    return unimplemented("sceKernelGetEventFlagInfo");
}

EXPORT(int, sceKernelGetEventInfo) {
    return unimplemented("sceKernelGetEventInfo");
}

EXPORT(int, sceKernelGetEventPattern) {
    return unimplemented("sceKernelGetEventPattern");
}

EXPORT(int, sceKernelGetLwCondInfo) {
    return unimplemented("sceKernelGetLwCondInfo");
}

EXPORT(int, sceKernelGetLwCondInfoById) {
    return unimplemented("sceKernelGetLwCondInfoById");
}

EXPORT(int, sceKernelGetLwMutexInfo) {
    return unimplemented("sceKernelGetLwMutexInfo");
}

EXPORT(int, sceKernelGetLwMutexInfoById) {
    return unimplemented("sceKernelGetLwMutexInfoById");
}

EXPORT(int, sceKernelGetModuleInfoByAddr) {
    return unimplemented("sceKernelGetModuleInfoByAddr");
}

EXPORT(int, sceKernelGetMsgPipeInfo) {
    return unimplemented("sceKernelGetMsgPipeInfo");
}

EXPORT(int, sceKernelGetMutexInfo) {
    return unimplemented("sceKernelGetMutexInfo");
}

EXPORT(int, sceKernelGetOpenPsId) {
    return unimplemented("sceKernelGetOpenPsId");
}

EXPORT(int, sceKernelGetPMUSERENR) {
    return unimplemented("sceKernelGetPMUSERENR");
}

EXPORT(int, sceKernelGetProcessTime) {
    return unimplemented("sceKernelGetProcessTime");
}

static SceUInt64 _sceKernelGetProcessTimeWide(){
    constexpr SceUInt64 scale = 1000000 / CLOCKS_PER_SEC;
    static_assert((CLOCKS_PER_SEC * scale) == 1000000, "CLOCKS_PER_SEC doesn't scale easily to Vita's.");

    const clock_t clocks = clock();

    return clocks * scale;
}

EXPORT(SceUInt32, sceKernelGetProcessTimeLow) {
    return (SceUInt32)(_sceKernelGetProcessTimeWide());
}

EXPORT(SceUInt64, sceKernelGetProcessTimeWide) {
    return _sceKernelGetProcessTimeWide();
}

EXPORT(int, sceKernelGetRWLockInfo) {
    return unimplemented("sceKernelGetRWLockInfo");
}

EXPORT(int, sceKernelGetSemaInfo) {
    return unimplemented("sceKernelGetSemaInfo");
}

EXPORT(int, sceKernelGetSystemInfo) {
    return unimplemented("sceKernelGetSystemInfo");
}

EXPORT(Ptr<Ptr<void>>, sceKernelGetTLSAddr, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thread_id, key);
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask) {
    return unimplemented("sceKernelGetThreadCpuAffinityMask");
}

EXPORT(int, sceKernelGetThreadCurrentPriority) {
    return unimplemented("sceKernelGetThreadCurrentPriority");
}

EXPORT(int, sceKernelGetThreadExitStatus) {
    return unimplemented("sceKernelGetThreadExitStatus");
}

EXPORT(int, sceKernelGetThreadId) {
    return thread_id;
}

EXPORT(int, sceKernelGetThreadInfo) {
    return unimplemented("sceKernelGetThreadInfo");
}

EXPORT(int, sceKernelGetThreadRunStatus) {
    return unimplemented("sceKernelGetThreadRunStatus");
}

EXPORT(int, sceKernelGetTimerBase) {
    return unimplemented("sceKernelGetTimerBase");
}

EXPORT(int, sceKernelGetTimerEventRemainingTime) {
    return unimplemented("sceKernelGetTimerEventRemainingTime");
}

EXPORT(int, sceKernelGetTimerInfo) {
    return unimplemented("sceKernelGetTimerInfo");
}

EXPORT(int, sceKernelGetTimerTime) {
    return unimplemented("sceKernelGetTimerTime");
}

EXPORT(int, sceKernelLoadModule) {
    return unimplemented("sceKernelLoadModule");
}

EXPORT(int, sceKernelLoadStartModule) {
    return unimplemented("sceKernelLoadStartModule");
}

EXPORT(int, sceKernelLockLwMutex, SceKernelLwMutexWork *pWork, int lockCount, unsigned int *pTimeout) {
    assert(pWork != nullptr);
    assert(lockCount == 1);
    assert(pTimeout == nullptr);

    return unimplemented("sceKernelLockLwMutex");
}

EXPORT(int, sceKernelLockLwMutexCB) {
    return unimplemented("sceKernelLockLwMutexCB");
}

EXPORT(int, sceKernelLockMutex) {
    return unimplemented("sceKernelLockMutex");
}

EXPORT(int, sceKernelLockMutexCB) {
    return unimplemented("sceKernelLockMutexCB");
}

EXPORT(int, sceKernelLockReadRWLock) {
    return unimplemented("sceKernelLockReadRWLock");
}

EXPORT(int, sceKernelLockReadRWLockCB) {
    return unimplemented("sceKernelLockReadRWLockCB");
}

EXPORT(int, sceKernelLockWriteRWLock) {
    return unimplemented("sceKernelLockWriteRWLock");
}

EXPORT(int, sceKernelLockWriteRWLockCB) {
    return unimplemented("sceKernelLockWriteRWLockCB");
}

EXPORT(int, sceKernelOpenModule) {
    return unimplemented("sceKernelOpenModule");
}

EXPORT(int, sceKernelPMonThreadGetCounter) {
    return unimplemented("sceKernelPMonThreadGetCounter");
}

EXPORT(int, sceKernelPollEvent) {
    return unimplemented("sceKernelPollEvent");
}

EXPORT(int, sceKernelPollEventFlag) {
    return unimplemented("sceKernelPollEventFlag");
}

EXPORT(int, sceKernelPrintBacktrace) {
    return unimplemented("sceKernelPrintBacktrace");
}

EXPORT(int, sceKernelPulseEventWithNotifyCallback) {
    return unimplemented("sceKernelPulseEventWithNotifyCallback");
}

EXPORT(int, sceKernelReceiveMsgPipe) {
    return unimplemented("sceKernelReceiveMsgPipe");
}

EXPORT(int, sceKernelReceiveMsgPipeCB) {
    return unimplemented("sceKernelReceiveMsgPipeCB");
}

EXPORT(int, sceKernelReceiveMsgPipeVector) {
    return unimplemented("sceKernelReceiveMsgPipeVector");
}

EXPORT(int, sceKernelReceiveMsgPipeVectorCB) {
    return unimplemented("sceKernelReceiveMsgPipeVectorCB");
}

EXPORT(int, sceKernelSendMsgPipe) {
    return unimplemented("sceKernelSendMsgPipe");
}

EXPORT(int, sceKernelSendMsgPipeCB) {
    return unimplemented("sceKernelSendMsgPipeCB");
}

EXPORT(int, sceKernelSendMsgPipeVector) {
    return unimplemented("sceKernelSendMsgPipeVector");
}

EXPORT(int, sceKernelSendMsgPipeVectorCB) {
    return unimplemented("sceKernelSendMsgPipeVectorCB");
}

EXPORT(int, sceKernelSetEventWithNotifyCallback) {
    return unimplemented("sceKernelSetEventWithNotifyCallback");
}

EXPORT(int, sceKernelSetTimerEvent) {
    return unimplemented("sceKernelSetTimerEvent");
}

EXPORT(int, sceKernelSetTimerTime) {
    return unimplemented("sceKernelSetTimerTime");
}

EXPORT(int, sceKernelSignalLwCond) {
    return unimplemented("sceKernelSignalLwCond");
}

EXPORT(int, sceKernelSignalLwCondAll) {
    return unimplemented("sceKernelSignalLwCondAll");
}

EXPORT(int, sceKernelSignalLwCondTo) {
    return unimplemented("sceKernelSignalLwCondTo");
}

EXPORT(int, sceKernelStackChkFail) {
    return unimplemented("sceKernelStackChkFail");
}

EXPORT(int, sceKernelStartModule) {
    return unimplemented("sceKernelStartModule");
}

EXPORT(int, sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    const std::unique_lock<std::mutex> lock(host.kernel.mutex);

    const WaitingThreadStates::const_iterator waiting = host.kernel.waiting_threads.find(thid);
    if (waiting == host.kernel.waiting_threads.end()) {
        return error("sceKernelStartThread", SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    const ThreadStatePtr thread = find(thid, host.kernel.threads);
    assert(thread);

    ThreadParams params;
    params.host = &host;
    params.thid = thid;
    params.arglen = arglen;
    params.argp = argp;

    const std::function<void(SDL_Thread *)> delete_thread = [thread](SDL_Thread *running_thread) {
        {
            const std::unique_lock<std::mutex> lock(thread->mutex);
            thread->to_do = ThreadToDo::exit;
        }
        thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
        SDL_WaitThread(running_thread, nullptr);
    };

    const ThreadPtr running_thread(SDL_CreateThread(&thread_function, waiting->second.name.c_str(), &params), delete_thread);
    if (!running_thread) {
        return error("sceKernelStartThread", SCE_KERNEL_ERROR_THREAD_ERROR);
    }

    host.kernel.waiting_threads.erase(waiting);
    host.kernel.running_threads.emplace(thid, running_thread);

    SDL_SemWait(params.host_may_destroy_params.get());

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelStopModule) {
    return unimplemented("sceKernelStopModule");
}

EXPORT(int, sceKernelStopUnloadModule) {
    return unimplemented("sceKernelStopUnloadModule");
}

EXPORT(int, sceKernelTryLockLwMutex) {
    return unimplemented("sceKernelTryLockLwMutex");
}

EXPORT(int, sceKernelTryReceiveMsgPipe) {
    return unimplemented("sceKernelTryReceiveMsgPipe");
}

EXPORT(int, sceKernelTryReceiveMsgPipeVector) {
    return unimplemented("sceKernelTryReceiveMsgPipeVector");
}

EXPORT(int, sceKernelTrySendMsgPipe) {
    return unimplemented("sceKernelTrySendMsgPipe");
}

EXPORT(int, sceKernelTrySendMsgPipeVector) {
    return unimplemented("sceKernelTrySendMsgPipeVector");
}

EXPORT(int, sceKernelUnloadModule) {
    return unimplemented("sceKernelUnloadModule");
}

EXPORT(int, sceKernelUnlockLwMutex, SceKernelLwMutexWork *pWork, int unlockCount) {
    assert(pWork != nullptr);
    assert(unlockCount == 1);

    return unimplemented("sceKernelUnlockLwMutex");
}

EXPORT(int, sceKernelWaitCond) {
    return unimplemented("sceKernelWaitCond");
}

EXPORT(int, sceKernelWaitCondCB) {
    return unimplemented("sceKernelWaitCondCB");
}

EXPORT(int, sceKernelWaitEvent) {
    return unimplemented("sceKernelWaitEvent");
}

EXPORT(int, sceKernelWaitEventCB) {
    return unimplemented("sceKernelWaitEventCB");
}

EXPORT(int, sceKernelWaitEventFlag) {
    return unimplemented("sceKernelWaitEventFlag");
}

EXPORT(int, sceKernelWaitEventFlagCB) {
    return unimplemented("sceKernelWaitEventFlagCB");
}

EXPORT(int, sceKernelWaitLwCond) {
    return unimplemented("sceKernelWaitLwCond");
}

EXPORT(int, sceKernelWaitLwCondCB) {
    return unimplemented("sceKernelWaitLwCondCB");
}

EXPORT(int, sceKernelWaitMultipleEvents) {
    return unimplemented("sceKernelWaitMultipleEvents");
}

EXPORT(int, sceKernelWaitMultipleEventsCB) {
    return unimplemented("sceKernelWaitMultipleEventsCB");
}

EXPORT(int, sceKernelWaitSema, SceUID semaid, int signal, SceUInt *timeout) {
    assert(semaid >= 0);
    assert(signal == 1);
    assert(timeout == nullptr);

    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, host.kernel.semaphores, host.kernel.mutex);
    if (!semaphore) {
        return error("sceKernelWaitSema", SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    {
        const std::unique_lock<std::mutex> lock(thread->mutex);
        assert(thread->to_do == ThreadToDo::run);
        thread->to_do = ThreadToDo::wait;
    }
    stop(*thread->cpu);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelWaitSemaCB) {
    return unimplemented("sceKernelWaitSemaCB");
}

EXPORT(int, sceKernelWaitSignal) {
    return unimplemented("sceKernelWaitSignal");
}

EXPORT(int, sceKernelWaitSignalCB) {
    return unimplemented("sceKernelWaitSignalCB");
}

EXPORT(int, sceKernelWaitThreadEnd) {
    return unimplemented("sceKernelWaitThreadEnd");
}

EXPORT(int, sceKernelWaitThreadEndCB) {
    return unimplemented("sceKernelWaitThreadEndCB");
}

EXPORT(int, sceSblACMgrIsGameProgram) {
    return unimplemented("sceSblACMgrIsGameProgram");
}

BRIDGE_IMPL(SceKernelStackChkGuard)
BRIDGE_IMPL(__sce_aeabi_idiv0)
BRIDGE_IMPL(__sce_aeabi_ldiv0)
BRIDGE_IMPL(__stack_chk_fail)
BRIDGE_IMPL(__stack_chk_guard)
BRIDGE_IMPL(_sceKernelCreateLwMutex)
BRIDGE_IMPL(sceClibAbort)
BRIDGE_IMPL(sceClibDprintf)
BRIDGE_IMPL(sceClibLookCtypeTable)
BRIDGE_IMPL(sceClibMemchr)
BRIDGE_IMPL(sceClibMemcmp)
BRIDGE_IMPL(sceClibMemcmpConstTime)
BRIDGE_IMPL(sceClibMemcpy)
BRIDGE_IMPL(sceClibMemcpyChk)
BRIDGE_IMPL(sceClibMemcpy_safe)
BRIDGE_IMPL(sceClibMemmove)
BRIDGE_IMPL(sceClibMemmoveChk)
BRIDGE_IMPL(sceClibMemset)
BRIDGE_IMPL(sceClibMemsetChk)
BRIDGE_IMPL(sceClibMspaceCalloc)
BRIDGE_IMPL(sceClibMspaceCreate)
BRIDGE_IMPL(sceClibMspaceDestroy)
BRIDGE_IMPL(sceClibMspaceFree)
BRIDGE_IMPL(sceClibMspaceIsHeapEmpty)
BRIDGE_IMPL(sceClibMspaceMalloc)
BRIDGE_IMPL(sceClibMspaceMallocStats)
BRIDGE_IMPL(sceClibMspaceMallocStatsFast)
BRIDGE_IMPL(sceClibMspaceMallocUsableSize)
BRIDGE_IMPL(sceClibMspaceMemalign)
BRIDGE_IMPL(sceClibMspaceRealloc)
BRIDGE_IMPL(sceClibMspaceReallocalign)
BRIDGE_IMPL(sceClibPrintf)
BRIDGE_IMPL(sceClibSnprintf)
BRIDGE_IMPL(sceClibSnprintfChk)
BRIDGE_IMPL(sceClibStrcatChk)
BRIDGE_IMPL(sceClibStrchr)
BRIDGE_IMPL(sceClibStrcmp)
BRIDGE_IMPL(sceClibStrcpyChk)
BRIDGE_IMPL(sceClibStrlcat)
BRIDGE_IMPL(sceClibStrlcatChk)
BRIDGE_IMPL(sceClibStrlcpy)
BRIDGE_IMPL(sceClibStrlcpyChk)
BRIDGE_IMPL(sceClibStrncasecmp)
BRIDGE_IMPL(sceClibStrncat)
BRIDGE_IMPL(sceClibStrncatChk)
BRIDGE_IMPL(sceClibStrncmp)
BRIDGE_IMPL(sceClibStrncpy)
BRIDGE_IMPL(sceClibStrncpyChk)
BRIDGE_IMPL(sceClibStrnlen)
BRIDGE_IMPL(sceClibStrrchr)
BRIDGE_IMPL(sceClibStrstr)
BRIDGE_IMPL(sceClibStrtoll)
BRIDGE_IMPL(sceClibTolower)
BRIDGE_IMPL(sceClibToupper)
BRIDGE_IMPL(sceClibVdprintf)
BRIDGE_IMPL(sceClibVprintf)
BRIDGE_IMPL(sceClibVsnprintf)
BRIDGE_IMPL(sceClibVsnprintfChk)
BRIDGE_IMPL(sceIoChstat)
BRIDGE_IMPL(sceIoChstatByFd)
BRIDGE_IMPL(sceIoDevctl)
BRIDGE_IMPL(sceIoDopen)
BRIDGE_IMPL(sceIoDread)
BRIDGE_IMPL(sceIoGetstat)
BRIDGE_IMPL(sceIoGetstatByFd)
BRIDGE_IMPL(sceIoIoctl)
BRIDGE_IMPL(sceIoIoctlAsync)
BRIDGE_IMPL(sceIoLseek)
BRIDGE_IMPL(sceIoLseekAsync)
BRIDGE_IMPL(sceIoMkdir)
BRIDGE_IMPL(sceIoOpen)
BRIDGE_IMPL(sceIoOpenAsync)
BRIDGE_IMPL(sceIoPread)
BRIDGE_IMPL(sceIoPwrite)
BRIDGE_IMPL(sceIoRemove)
BRIDGE_IMPL(sceIoRename)
BRIDGE_IMPL(sceIoRmdir)
BRIDGE_IMPL(sceIoSync)
BRIDGE_IMPL(sceKernelAtomicAddAndGet16)
BRIDGE_IMPL(sceKernelAtomicAddAndGet32)
BRIDGE_IMPL(sceKernelAtomicAddAndGet64)
BRIDGE_IMPL(sceKernelAtomicAddAndGet8)
BRIDGE_IMPL(sceKernelAtomicAddUnless16)
BRIDGE_IMPL(sceKernelAtomicAddUnless32)
BRIDGE_IMPL(sceKernelAtomicAddUnless64)
BRIDGE_IMPL(sceKernelAtomicAddUnless8)
BRIDGE_IMPL(sceKernelAtomicAndAndGet16)
BRIDGE_IMPL(sceKernelAtomicAndAndGet32)
BRIDGE_IMPL(sceKernelAtomicAndAndGet64)
BRIDGE_IMPL(sceKernelAtomicAndAndGet8)
BRIDGE_IMPL(sceKernelAtomicClearAndGet16)
BRIDGE_IMPL(sceKernelAtomicClearAndGet32)
BRIDGE_IMPL(sceKernelAtomicClearAndGet64)
BRIDGE_IMPL(sceKernelAtomicClearAndGet8)
BRIDGE_IMPL(sceKernelAtomicClearMask16)
BRIDGE_IMPL(sceKernelAtomicClearMask32)
BRIDGE_IMPL(sceKernelAtomicClearMask64)
BRIDGE_IMPL(sceKernelAtomicClearMask8)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet16)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet32)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet64)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet8)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive16)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive32)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive64)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive8)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd16)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd32)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd64)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd8)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd16)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd32)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd64)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd8)
BRIDGE_IMPL(sceKernelAtomicGetAndClear16)
BRIDGE_IMPL(sceKernelAtomicGetAndClear32)
BRIDGE_IMPL(sceKernelAtomicGetAndClear64)
BRIDGE_IMPL(sceKernelAtomicGetAndClear8)
BRIDGE_IMPL(sceKernelAtomicGetAndOr16)
BRIDGE_IMPL(sceKernelAtomicGetAndOr32)
BRIDGE_IMPL(sceKernelAtomicGetAndOr64)
BRIDGE_IMPL(sceKernelAtomicGetAndOr8)
BRIDGE_IMPL(sceKernelAtomicGetAndSet16)
BRIDGE_IMPL(sceKernelAtomicGetAndSet32)
BRIDGE_IMPL(sceKernelAtomicGetAndSet64)
BRIDGE_IMPL(sceKernelAtomicGetAndSet8)
BRIDGE_IMPL(sceKernelAtomicGetAndSub16)
BRIDGE_IMPL(sceKernelAtomicGetAndSub32)
BRIDGE_IMPL(sceKernelAtomicGetAndSub64)
BRIDGE_IMPL(sceKernelAtomicGetAndSub8)
BRIDGE_IMPL(sceKernelAtomicGetAndXor16)
BRIDGE_IMPL(sceKernelAtomicGetAndXor32)
BRIDGE_IMPL(sceKernelAtomicGetAndXor64)
BRIDGE_IMPL(sceKernelAtomicGetAndXor8)
BRIDGE_IMPL(sceKernelAtomicOrAndGet16)
BRIDGE_IMPL(sceKernelAtomicOrAndGet32)
BRIDGE_IMPL(sceKernelAtomicOrAndGet64)
BRIDGE_IMPL(sceKernelAtomicOrAndGet8)
BRIDGE_IMPL(sceKernelAtomicSet16)
BRIDGE_IMPL(sceKernelAtomicSet32)
BRIDGE_IMPL(sceKernelAtomicSet64)
BRIDGE_IMPL(sceKernelAtomicSet8)
BRIDGE_IMPL(sceKernelAtomicSubAndGet16)
BRIDGE_IMPL(sceKernelAtomicSubAndGet32)
BRIDGE_IMPL(sceKernelAtomicSubAndGet64)
BRIDGE_IMPL(sceKernelAtomicSubAndGet8)
BRIDGE_IMPL(sceKernelAtomicXorAndGet16)
BRIDGE_IMPL(sceKernelAtomicXorAndGet32)
BRIDGE_IMPL(sceKernelAtomicXorAndGet64)
BRIDGE_IMPL(sceKernelAtomicXorAndGet8)
BRIDGE_IMPL(sceKernelBacktrace)
BRIDGE_IMPL(sceKernelBacktraceSelf)
BRIDGE_IMPL(sceKernelCallModuleExit)
BRIDGE_IMPL(sceKernelCallWithChangeStack)
BRIDGE_IMPL(sceKernelCancelEvent)
BRIDGE_IMPL(sceKernelCancelEventFlag)
BRIDGE_IMPL(sceKernelCancelEventWithSetPattern)
BRIDGE_IMPL(sceKernelCancelMsgPipe)
BRIDGE_IMPL(sceKernelCancelMutex)
BRIDGE_IMPL(sceKernelCancelRWLock)
BRIDGE_IMPL(sceKernelCancelSema)
BRIDGE_IMPL(sceKernelCancelTimer)
BRIDGE_IMPL(sceKernelChangeCurrentThreadAttr)
BRIDGE_IMPL(sceKernelCheckThreadStack)
BRIDGE_IMPL(sceKernelCloseModule)
BRIDGE_IMPL(sceKernelCreateCond)
BRIDGE_IMPL(sceKernelCreateEventFlag)
BRIDGE_IMPL(sceKernelCreateLwCond)
BRIDGE_IMPL(sceKernelCreateLwMutex)
BRIDGE_IMPL(sceKernelCreateMsgPipe)
BRIDGE_IMPL(sceKernelCreateMutex)
BRIDGE_IMPL(sceKernelCreateRWLock)
BRIDGE_IMPL(sceKernelCreateSema)
BRIDGE_IMPL(sceKernelCreateSimpleEvent)
BRIDGE_IMPL(sceKernelCreateThread)
BRIDGE_IMPL(sceKernelCreateTimer)
BRIDGE_IMPL(sceKernelDeleteLwCond)
BRIDGE_IMPL(sceKernelDeleteLwMutex)
BRIDGE_IMPL(sceKernelExitProcess)
BRIDGE_IMPL(sceKernelGetCallbackInfo)
BRIDGE_IMPL(sceKernelGetCondInfo)
BRIDGE_IMPL(sceKernelGetCurrentThreadVfpException)
BRIDGE_IMPL(sceKernelGetEventFlagInfo)
BRIDGE_IMPL(sceKernelGetEventInfo)
BRIDGE_IMPL(sceKernelGetEventPattern)
BRIDGE_IMPL(sceKernelGetLwCondInfo)
BRIDGE_IMPL(sceKernelGetLwCondInfoById)
BRIDGE_IMPL(sceKernelGetLwMutexInfo)
BRIDGE_IMPL(sceKernelGetLwMutexInfoById)
BRIDGE_IMPL(sceKernelGetModuleInfoByAddr)
BRIDGE_IMPL(sceKernelGetMsgPipeInfo)
BRIDGE_IMPL(sceKernelGetMutexInfo)
BRIDGE_IMPL(sceKernelGetOpenPsId)
BRIDGE_IMPL(sceKernelGetPMUSERENR)
BRIDGE_IMPL(sceKernelGetProcessTime)
BRIDGE_IMPL(sceKernelGetProcessTimeLow)
BRIDGE_IMPL(sceKernelGetProcessTimeWide)
BRIDGE_IMPL(sceKernelGetRWLockInfo)
BRIDGE_IMPL(sceKernelGetSemaInfo)
BRIDGE_IMPL(sceKernelGetSystemInfo)
BRIDGE_IMPL(sceKernelGetTLSAddr)
BRIDGE_IMPL(sceKernelGetThreadCpuAffinityMask)
BRIDGE_IMPL(sceKernelGetThreadCurrentPriority)
BRIDGE_IMPL(sceKernelGetThreadExitStatus)
BRIDGE_IMPL(sceKernelGetThreadId)
BRIDGE_IMPL(sceKernelGetThreadInfo)
BRIDGE_IMPL(sceKernelGetThreadRunStatus)
BRIDGE_IMPL(sceKernelGetTimerBase)
BRIDGE_IMPL(sceKernelGetTimerEventRemainingTime)
BRIDGE_IMPL(sceKernelGetTimerInfo)
BRIDGE_IMPL(sceKernelGetTimerTime)
BRIDGE_IMPL(sceKernelLoadModule)
BRIDGE_IMPL(sceKernelLoadStartModule)
BRIDGE_IMPL(sceKernelLockLwMutex)
BRIDGE_IMPL(sceKernelLockLwMutexCB)
BRIDGE_IMPL(sceKernelLockMutex)
BRIDGE_IMPL(sceKernelLockMutexCB)
BRIDGE_IMPL(sceKernelLockReadRWLock)
BRIDGE_IMPL(sceKernelLockReadRWLockCB)
BRIDGE_IMPL(sceKernelLockWriteRWLock)
BRIDGE_IMPL(sceKernelLockWriteRWLockCB)
BRIDGE_IMPL(sceKernelOpenModule)
BRIDGE_IMPL(sceKernelPMonThreadGetCounter)
BRIDGE_IMPL(sceKernelPollEvent)
BRIDGE_IMPL(sceKernelPollEventFlag)
BRIDGE_IMPL(sceKernelPrintBacktrace)
BRIDGE_IMPL(sceKernelPulseEventWithNotifyCallback)
BRIDGE_IMPL(sceKernelReceiveMsgPipe)
BRIDGE_IMPL(sceKernelReceiveMsgPipeCB)
BRIDGE_IMPL(sceKernelReceiveMsgPipeVector)
BRIDGE_IMPL(sceKernelReceiveMsgPipeVectorCB)
BRIDGE_IMPL(sceKernelSendMsgPipe)
BRIDGE_IMPL(sceKernelSendMsgPipeCB)
BRIDGE_IMPL(sceKernelSendMsgPipeVector)
BRIDGE_IMPL(sceKernelSendMsgPipeVectorCB)
BRIDGE_IMPL(sceKernelSetEventWithNotifyCallback)
BRIDGE_IMPL(sceKernelSetTimerEvent)
BRIDGE_IMPL(sceKernelSetTimerTime)
BRIDGE_IMPL(sceKernelSignalLwCond)
BRIDGE_IMPL(sceKernelSignalLwCondAll)
BRIDGE_IMPL(sceKernelSignalLwCondTo)
BRIDGE_IMPL(sceKernelStackChkFail)
BRIDGE_IMPL(sceKernelStartModule)
BRIDGE_IMPL(sceKernelStartThread)
BRIDGE_IMPL(sceKernelStopModule)
BRIDGE_IMPL(sceKernelStopUnloadModule)
BRIDGE_IMPL(sceKernelTryLockLwMutex)
BRIDGE_IMPL(sceKernelTryReceiveMsgPipe)
BRIDGE_IMPL(sceKernelTryReceiveMsgPipeVector)
BRIDGE_IMPL(sceKernelTrySendMsgPipe)
BRIDGE_IMPL(sceKernelTrySendMsgPipeVector)
BRIDGE_IMPL(sceKernelUnloadModule)
BRIDGE_IMPL(sceKernelUnlockLwMutex)
BRIDGE_IMPL(sceKernelWaitCond)
BRIDGE_IMPL(sceKernelWaitCondCB)
BRIDGE_IMPL(sceKernelWaitEvent)
BRIDGE_IMPL(sceKernelWaitEventCB)
BRIDGE_IMPL(sceKernelWaitEventFlag)
BRIDGE_IMPL(sceKernelWaitEventFlagCB)
BRIDGE_IMPL(sceKernelWaitLwCond)
BRIDGE_IMPL(sceKernelWaitLwCondCB)
BRIDGE_IMPL(sceKernelWaitMultipleEvents)
BRIDGE_IMPL(sceKernelWaitMultipleEventsCB)
BRIDGE_IMPL(sceKernelWaitSema)
BRIDGE_IMPL(sceKernelWaitSemaCB)
BRIDGE_IMPL(sceKernelWaitSignal)
BRIDGE_IMPL(sceKernelWaitSignalCB)
BRIDGE_IMPL(sceKernelWaitThreadEnd)
BRIDGE_IMPL(sceKernelWaitThreadEndCB)
BRIDGE_IMPL(sceSblACMgrIsGameProgram)
