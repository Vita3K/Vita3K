// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include "../SceKernelThreadMgr/SceThreadmgr.h"
#include <kernel/state.h>
#include <util/tracy.h>
TRACY_MODULE_NAME(SceKernelForMono);

EXPORT(int, sceKernelGetThreadContextForMono, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(sceKernelGetThreadContextForMono, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    return CALL_EXPORT(_sceKernelGetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(int, sceKernelResumeThreadForMono, SceUID threadId) {
    TRACY_FUNC(sceKernelResumeThreadForMono, threadId);
    return CALL_EXPORT(sceKernelResumeThreadForVM, threadId);
}

EXPORT(int, sceKernelSetThreadContextForMono, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(sceKernelSetThreadContextForMono, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    return CALL_EXPORT(_sceKernelSetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(int, sceKernelSuspendThreadForMono, SceUID threadId) {
    TRACY_FUNC(sceKernelSuspendThreadForMono, threadId);
    return CALL_EXPORT(sceKernelSuspendThreadForVM, threadId);
}

EXPORT(int, sceKernelWaitExceptionCBForMono) {
    TRACY_FUNC(sceKernelWaitExceptionCBForMono);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitExceptionForMono) {
    TRACY_FUNC(sceKernelWaitExceptionForMono);
    STUBBED("Inifinite wait");
    ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    thread->suspend();
    return 0;
}
