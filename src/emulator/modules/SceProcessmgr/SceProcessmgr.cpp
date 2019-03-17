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

#include "SceProcessmgr.h"

#include <io/functions.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/processmgr.h>
#include <rtc/rtc.h>

struct VitaTimeval {
    uint32_t tv_sec;
    uint32_t tv_usec;
};

EXPORT(int, sceKernelCDialogSessionClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCDialogSetLeaseLimit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallAbortHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetCurrentProcess) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetExtraTty) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessName) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<uint32_t>, sceKernelGetProcessParam, void *args) {
    return host.kernel.process_param;
}

EXPORT(int, sceKernelGetProcessTimeCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTimeLowCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTimeWideCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTitleId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetRemoteProcessTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetStderr) {
    return open_file(host.io, "tty0:", SCE_O_WRONLY, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceKernelGetStdin) {
    return open_file(host.io, "tty0:", SCE_O_RDONLY, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceKernelGetStdout) {
    return open_file(host.io, "tty0:", SCE_O_WRONLY, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceKernelIsCDialogAvailable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsGameBudget) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLibcClock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLibcGettimeofday, Ptr<VitaTimeval> timeAddr, Ptr<Address> tzAddr) {
    if (timeAddr) {
        auto *tv = timeAddr.get(host.mem);

        const auto ticks = rtc_get_ticks(host.kernel.base_tick.tick) - RTC_OFFSET;

        tv->tv_sec = ticks / VITA_CLOCKS_PER_SEC;
        tv->tv_usec = ticks % VITA_CLOCKS_PER_SEC;
    }

    return 0;
}

EXPORT(int, sceKernelLibcGmtime_r) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLibcLocaltime_r) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLibcMktime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLibcTime, uint32_t *time) {
    const auto ticks = rtc_get_ticks(host.kernel.base_tick.tick) / VITA_CLOCKS_PER_SEC;

    if (time) {
        *time = ticks;
    }

    return ticks;
}

EXPORT(int, sceKernelPowerLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPowerTick, SceKernelPowerTickType type) {
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelPowerUnlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelRegisterProcessTerminationCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterProcessTerminationCallback) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceKernelCDialogSessionClose)
BRIDGE_IMPL(sceKernelCDialogSetLeaseLimit)
BRIDGE_IMPL(sceKernelCallAbortHandler)
BRIDGE_IMPL(sceKernelGetCurrentProcess)
BRIDGE_IMPL(sceKernelGetExtraTty)
BRIDGE_IMPL(sceKernelGetProcessName)
BRIDGE_IMPL(sceKernelGetProcessParam)
BRIDGE_IMPL(sceKernelGetProcessTimeCore)
BRIDGE_IMPL(sceKernelGetProcessTimeLowCore)
BRIDGE_IMPL(sceKernelGetProcessTimeWideCore)
BRIDGE_IMPL(sceKernelGetProcessTitleId)
BRIDGE_IMPL(sceKernelGetRemoteProcessTime)
BRIDGE_IMPL(sceKernelGetStderr)
BRIDGE_IMPL(sceKernelGetStdin)
BRIDGE_IMPL(sceKernelGetStdout)
BRIDGE_IMPL(sceKernelIsCDialogAvailable)
BRIDGE_IMPL(sceKernelIsGameBudget)
BRIDGE_IMPL(sceKernelLibcClock)
BRIDGE_IMPL(sceKernelLibcGettimeofday)
BRIDGE_IMPL(sceKernelLibcGmtime_r)
BRIDGE_IMPL(sceKernelLibcLocaltime_r)
BRIDGE_IMPL(sceKernelLibcMktime)
BRIDGE_IMPL(sceKernelLibcTime)
BRIDGE_IMPL(sceKernelPowerLock)
BRIDGE_IMPL(sceKernelPowerTick)
BRIDGE_IMPL(sceKernelPowerUnlock)
BRIDGE_IMPL(sceKernelRegisterProcessTerminationCallback)
BRIDGE_IMPL(sceKernelUnregisterProcessTerminationCallback)
