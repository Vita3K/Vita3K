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

#pragma once

#include <module/module.h>

BRIDGE_DECL(_sceKernelExitProcessForUser)
BRIDGE_DECL(_sceKernelGetTimer5Reg)
BRIDGE_DECL(_sceKernelRegisterLibkernelAddresses)
BRIDGE_DECL(sceKernelCDialogSessionClose)
BRIDGE_DECL(sceKernelCDialogSetLeaseLimit)
BRIDGE_DECL(sceKernelCallAbortHandler)
BRIDGE_DECL(sceKernelGetCurrentProcess)
BRIDGE_DECL(sceKernelGetExtraTty)
BRIDGE_DECL(sceKernelGetProcessName)
BRIDGE_DECL(sceKernelGetProcessParam)
BRIDGE_DECL(sceKernelGetProcessTimeCore)
BRIDGE_DECL(sceKernelGetProcessTimeLowCore)
BRIDGE_DECL(sceKernelGetProcessTimeWideCore)
BRIDGE_DECL(sceKernelGetProcessTitleId)
BRIDGE_DECL(sceKernelGetRemoteProcessTime)
BRIDGE_DECL(sceKernelGetStderr)
BRIDGE_DECL(sceKernelGetStdin)
BRIDGE_DECL(sceKernelGetStdout)
BRIDGE_DECL(sceKernelIsCDialogAvailable)
BRIDGE_DECL(sceKernelIsGameBudget)
BRIDGE_DECL(sceKernelLibcClock)
BRIDGE_DECL(sceKernelLibcGettimeofday)
BRIDGE_DECL(sceKernelLibcGmtime_r)
BRIDGE_DECL(sceKernelLibcLocaltime_r)
BRIDGE_DECL(sceKernelLibcMktime)
BRIDGE_DECL(sceKernelLibcTime)
BRIDGE_DECL(sceKernelPowerLock)
BRIDGE_DECL(sceKernelPowerTick)
BRIDGE_DECL(sceKernelPowerUnlock)
BRIDGE_DECL(sceKernelRegisterProcessTerminationCallback)
BRIDGE_DECL(sceKernelUnregisterProcessTerminationCallback)
BRIDGE_DECL(sceLibKernel_9F793F84)
