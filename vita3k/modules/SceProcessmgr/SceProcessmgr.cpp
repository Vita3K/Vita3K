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
#include <rtc/rtc.h>

#include <ctime>

enum SceKernelPowerTickType {
    /** Cancel all timers */
    SCE_KERNEL_POWER_TICK_DEFAULT = 0,
    /** Cancel automatic suspension timer */
    SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND = 1,
    /** Cancel OLED-off timer */
    SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF = 4,
    /** Cancel OLED dimming timer */
    SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING = 6
};

struct VitaTimeval {
    uint32_t tv_sec;
    uint32_t tv_usec;
};
struct VitaTimezone {
    int tz_minuteswest;
    int tz_dsttime;
};

using VitaTime = std::uint32_t;

EXPORT(int, _sceKernelExitProcessForUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimer5Reg) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelRegisterLibkernelAddresses) {
    return UNIMPLEMENTED();
}

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
    return open_file(*host.io, "tty0:", SCE_O_WRONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdin) {
    return open_file(*host.io, "tty0:", SCE_O_RDONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdout) {
    return open_file(*host.io, "tty0:", SCE_O_WRONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelIsCDialogAvailable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsGameBudget) {
    return UNIMPLEMENTED();
}

EXPORT(VitaTime, sceKernelLibcClock) {
    return rtc_get_ticks(host.kernel.base_tick.tick) - rtc_get_ticks(host.kernel.start_tick.tick);
}

EXPORT(int, sceKernelLibcGettimeofday, VitaTimeval *timeAddr, VitaTimezone *tzAddr) {
    const auto ticks = rtc_get_ticks(host.kernel.base_tick.tick) - RTC_OFFSET;
    if (timeAddr != nullptr) {
        timeAddr->tv_sec = static_cast<std::uint32_t>(ticks / VITA_CLOCKS_PER_SEC);
        timeAddr->tv_usec = ticks % VITA_CLOCKS_PER_SEC;
    }
    if (tzAddr != nullptr) {
        std::time_t t = std::time(nullptr);
        std::time_t lt = mktime(std::localtime(&t));
        tzAddr->tz_minuteswest = static_cast<int>((lt - t) / 60);
    }
    return 0;
}

EXPORT(int, sceKernelLibcGmtime_r) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<struct tm>, sceKernelLibcLocaltime_r, const VitaTime *time, Ptr<struct tm> date) {
    const time_t plat_time = *time;

#if defined(WIN32)
    localtime_s(date.get(host.mem), &plat_time);
#else
    localtime_r(&plat_time, date.get(host.mem));
#endif

    return date;
}

EXPORT(int, sceKernelLibcMktime) {
    return UNIMPLEMENTED();
}

EXPORT(VitaTime, sceKernelLibcTime, VitaTime *time) {
    const auto secs = (rtc_get_ticks(host.kernel.base_tick.tick) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC;

    if (time) {
        *time = static_cast<VitaTime>(secs);
    }

    return static_cast<VitaTime>(secs);
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

BRIDGE_IMPL(_sceKernelExitProcessForUser)
BRIDGE_IMPL(_sceKernelGetTimer5Reg)
BRIDGE_IMPL(_sceKernelRegisterLibkernelAddresses)
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
