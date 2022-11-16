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
#include <kernel/state.h>
#include <rtc/rtc.h>

#include <util/safe_time.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceProcessmgr);

template <>
std::string to_debug_str<SceKernelPowerTickType>(const MemState &mem, SceKernelPowerTickType type) {
    switch (type) {
    case SCE_KERNEL_POWER_TICK_DEFAULT: return "SCE_KERNEL_POWER_TICK_DEFAULT";
    case SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND: return "SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND";
    case SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF: return "SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF";
    case SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING: return "SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING";
    }
    return std::to_string(type);
}

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
    TRACY_FUNC(_sceKernelExitProcessForUser);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimer5Reg, Ptr<uint64_t> *timer) {
    TRACY_FUNC(_sceKernelGetTimer5Reg, timer);
    *timer = alloc<uint64_t>(emuenv.mem, "timer5reg");
    *(*timer).get(emuenv.mem) = rtc_get_ticks(emuenv.kernel.base_tick.tick);
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelRegisterLibkernelAddresses, SceLibkernelAddresses *addresses) {
    TRACY_FUNC(_sceKernelRegisterLibkernelAddresses, addresses);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCDialogSessionClose) {
    TRACY_FUNC(sceKernelCDialogSessionClose);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCDialogSetLeaseLimit) {
    TRACY_FUNC(sceKernelCDialogSetLeaseLimit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallAbortHandler) {
    TRACY_FUNC(sceKernelCallAbortHandler);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetCurrentProcess) {
    TRACY_FUNC(sceKernelGetCurrentProcess);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetExtraTty) {
    TRACY_FUNC(sceKernelGetExtraTty);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessName) {
    TRACY_FUNC(sceKernelGetProcessName);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<uint32_t>, sceKernelGetProcessParam, void *args) {
    TRACY_FUNC(sceKernelGetProcessParam, args);
    return emuenv.kernel.process_param;
}

EXPORT(int, sceKernelGetProcessTimeCore) {
    TRACY_FUNC(sceKernelGetProcessTimeCore);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTimeLowCore) {
    TRACY_FUNC(sceKernelGetProcessTimeLowCore);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTimeWideCore) {
    TRACY_FUNC(sceKernelGetProcessTimeWideCore);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTitleId) {
    TRACY_FUNC(sceKernelGetProcessTitleId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetRemoteProcessTime) {
    TRACY_FUNC(sceKernelGetRemoteProcessTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetStderr) {
    TRACY_FUNC(sceKernelGetStderr);
    return open_file(emuenv.io, "tty0:", SCE_O_WRONLY, emuenv.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdin) {
    TRACY_FUNC(sceKernelGetStdin);
    return open_file(emuenv.io, "tty0:", SCE_O_RDONLY, emuenv.pref_path, export_name);
}

EXPORT(int, sceKernelGetStdout) {
    TRACY_FUNC(sceKernelGetStdout);
    return open_file(emuenv.io, "tty0:", SCE_O_WRONLY, emuenv.pref_path, export_name);
}

EXPORT(int, sceKernelIsCDialogAvailable) {
    TRACY_FUNC(sceKernelIsCDialogAvailable);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsGameBudget) {
    TRACY_FUNC(sceKernelIsGameBudget);
    return UNIMPLEMENTED();
}

EXPORT(VitaTime, sceKernelLibcClock) {
    TRACY_FUNC(sceKernelLibcClock);
    return static_cast<VitaTime>(rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick);
}

EXPORT(int, sceKernelLibcGettimeofday, VitaTimeval *timeAddr, VitaTimezone *tzAddr) {
    TRACY_FUNC(sceKernelLibcGettimeofday, timeAddr, tzAddr);
    const auto ticks = rtc_get_ticks(emuenv.kernel.base_tick.tick) - RTC_OFFSET;
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

EXPORT(Ptr<struct tm>, sceKernelLibcGmtime_r, const VitaTime *time, Ptr<struct tm> date) {
    TRACY_FUNC(sceKernelLibcGmtime_r, time, date);
    const time_t plat_time = *time;

    SAFE_GMTIME(&plat_time, date.get(emuenv.mem));

    return date;
}

EXPORT(Ptr<struct tm>, sceKernelLibcLocaltime_r, const VitaTime *time, Ptr<struct tm> date) {
    TRACY_FUNC(sceKernelLibcLocaltime_r, time, date);
    const time_t plat_time = *time;

    SAFE_LOCALTIME(&plat_time, date.get(emuenv.mem));

    return date;
}

EXPORT(int, sceKernelLibcMktime) {
    TRACY_FUNC(sceKernelLibcMktime);
    return UNIMPLEMENTED();
}

EXPORT(VitaTime, sceKernelLibcTime, VitaTime *time) {
    TRACY_FUNC(sceKernelLibcTime, time);
    const auto secs = (rtc_get_ticks(emuenv.kernel.base_tick.tick) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC;

    if (time) {
        *time = static_cast<VitaTime>(secs);
    }

    return static_cast<VitaTime>(secs);
}

EXPORT(int, sceKernelPowerLock) {
    TRACY_FUNC(sceKernelPowerLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPowerTick, SceKernelPowerTickType type) {
    TRACY_FUNC(sceKernelPowerTick, type);
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelPowerUnlock) {
    TRACY_FUNC(sceKernelPowerUnlock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelRegisterProcessTerminationCallback) {
    TRACY_FUNC(sceKernelRegisterProcessTerminationCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterProcessTerminationCallback) {
    TRACY_FUNC(sceKernelUnregisterProcessTerminationCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibKernel_9F793F84) {
    TRACY_FUNC(sceLibKernel_9F793F84);
    // Gets a version from the process' SceKernelProcessParam. Used for PSN Auth in SceShell.
    auto p_process_param = CALL_EXPORT(sceKernelGetProcessParam, nullptr);
    auto process_param = p_process_param.get(emuenv.mem);
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
