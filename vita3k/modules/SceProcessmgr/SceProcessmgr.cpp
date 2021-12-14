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

#include "SceProcessmgr.h"

#include <io/functions.h>
#include <rtc/rtc.h>

#include <util/safe_time.h>

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

struct SceLibkernelAddresses {
    uint32_t size;
    Ptr<void> sceKernelExitThread;
    Ptr<void> sceKernelExitDeleteThread;
    Ptr<void> _sceKernelExitCallback;
    Ptr<void> field_0x10;
    Ptr<void> field_0x14;
    Ptr<void> field_0x18;
};

EXPORT(int, _sceKernelExitProcessForUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimer5Reg, Ptr<uint64_t> *timer) {
    *timer = alloc<uint64_t>(host.mem, "timer5reg");
    *(*timer).get(host.mem) = rtc_get_ticks(host.kernel.base_tick.tick);
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelRegisterLibkernelAddresses, SceLibkernelAddresses *addresses) {
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
    return open_file(host.io, "tty0:", SCE_O_WRONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdin) {
    return open_file(host.io, "tty0:", SCE_O_RDONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdout) {
    return open_file(host.io, "tty0:", SCE_O_WRONLY, host.pref_path, export_name);
}

EXPORT(int, sceKernelIsCDialogAvailable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsGameBudget) {
    return UNIMPLEMENTED();
}

EXPORT(VitaTime, sceKernelLibcClock) {
    return static_cast<VitaTime>(rtc_get_ticks(host.kernel.base_tick.tick) - host.kernel.start_tick);
}

EXPORT(int, sceKernelLibcGettimeofday, VitaTimeval *timeAddr, VitaTimezone *tzAddr) {
    const auto ticks = rtc_get_ticks(host.kernel.base_tick.tick) - RTC_OFFSET;
    if (timeAddr != nullptr) {
        timeAddr->tv_sec = static_cast<std::uint32_t>(ticks / VITA_CLOCKS_PER_SEC);
        timeAddr->tv_usec = ticks % VITA_CLOCKS_PER_SEC;
    }
    if (tzAddr != nullptr) {
        std::time_t t = std::time(nullptr);

        tm localtime_tm = {};

        SAFE_LOCALTIME(&t, &localtime_tm);
        std::time_t lt = mktime(&localtime_tm);
        tzAddr->tz_minuteswest = static_cast<int>((lt - t) / 60);
    }
    return 0;
}

EXPORT(int, sceKernelLibcGmtime_r) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<struct tm>, sceKernelLibcLocaltime_r, const VitaTime *time, Ptr<struct tm> date) {
    const time_t plat_time = *time;

    SAFE_LOCALTIME(&plat_time, date.get(host.mem));

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

EXPORT(int, sceLibKernel_9F793F84) {
    //Gets a version from the process' SceKernelProcessParam. Used for PSN Auth in SceShell.
    auto p_process_param = CALL_EXPORT(sceKernelGetProcessParam, nullptr);
    auto process_param = p_process_param.get(host.mem);
    if (process_param && (process_param[1] == '2PSP') && (process_param[2] != 0)) {
        return process_param[3];
    } else {
        return 0;
    }
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
BRIDGE_IMPL(sceLibKernel_9F793F84)
