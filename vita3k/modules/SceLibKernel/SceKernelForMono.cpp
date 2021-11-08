// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceKernelForMono.h"
#include "../SceKernelThreadMgr/SceThreadmgr.h"

EXPORT(int, sceKernelGetThreadContextForMono, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    return CALL_EXPORT(_sceKernelGetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(int, sceKernelResumeThreadForMono, SceUID threadId) {
    return CALL_EXPORT(sceKernelResumeThreadForVM, threadId);
}

EXPORT(int, sceKernelSetThreadContextForMono, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    return CALL_EXPORT(_sceKernelSetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(int, sceKernelSuspendThreadForMono, SceUID threadId) {
    return CALL_EXPORT(sceKernelSuspendThreadForVM, threadId);
}

EXPORT(int, sceKernelWaitExceptionCBForMono) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitExceptionForMono) {
    STUBBED("Inifinite wait");
    while (true) {
    }

    return 0;
}

BRIDGE_IMPL(sceKernelGetThreadContextForMono)
BRIDGE_IMPL(sceKernelResumeThreadForMono)
BRIDGE_IMPL(sceKernelSetThreadContextForMono)
BRIDGE_IMPL(sceKernelSuspendThreadForMono)
BRIDGE_IMPL(sceKernelWaitExceptionCBForMono)
BRIDGE_IMPL(sceKernelWaitExceptionForMono)
