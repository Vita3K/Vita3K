// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

using VitaTime = uint32_t;
struct VitaTM {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

static_assert(sizeof(VitaTM) == 36);
static_assert(sizeof(VitaTM) <= sizeof(struct tm));

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

EXPORT(int, sceKernelCallAbortHandler, uint32_t param1, uint32_t param2) {
    TRACY_FUNC(sceKernelCallAbortHandler, param1, param2);
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

EXPORT(int, sceKernelGetProcessName, char *process_name, uint32_t len) {
    TRACY_FUNC(sceKernelGetProcessName, process_name, len);
    if (!process_name || len > 32 || len == 0)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    strncpy(process_name, emuenv.kernel.process_param.get(emuenv.mem)->process_name.get(emuenv.mem), len);
    return 0;
}

EXPORT(Ptr<SceProcessParam>, sceKernelGetProcessParam, void *args) {
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

EXPORT(int, sceKernelGetProcessTitleId, char *title_id, uint32_t len) {
    TRACY_FUNC(sceKernelGetProcessTitleId, title_id, len);
    if (!title_id || len > 32 || len == 0)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    strncpy(title_id, emuenv.io.title_id.c_str(), len);
    return 0;
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

EXPORT(Ptr<VitaTM>, sceKernelLibcGmtime_r, const VitaTime *time, Ptr<VitaTM> date) {
    TRACY_FUNC(sceKernelLibcGmtime_r, time, date);
    const time_t plat_time = *time;

    auto dateIn = date.get(emuenv.mem);

    tm host_tm = {};
    SAFE_GMTIME(&plat_time, &host_tm);
    memcpy(dateIn, &host_tm, sizeof(VitaTM));

    return date;
}

EXPORT(Ptr<VitaTM>, sceKernelLibcLocaltime_r, const VitaTime *time, Ptr<VitaTM> date) {
    TRACY_FUNC(sceKernelLibcLocaltime_r, time, date);
    const time_t plat_time = *time;
    auto dateIn = date.get(emuenv.mem);

    tm host_tm = {};
    SAFE_LOCALTIME(&plat_time, &host_tm);
    memcpy(dateIn, &host_tm, sizeof(VitaTM));

    return date;
}

EXPORT(int, sceKernelLibcMktime, VitaTM *date, VitaTime *time, uint64_t *param_3) {
    TRACY_FUNC(sceKernelLibcMktime, date, time);
    // param_3 - result, 8 bytes, unused
    if (!date) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    bool year_1900 = false;
    if (date->tm_year >= 1900) {
        date->tm_year -= 1900;
        year_1900 = true;
    }
    tm host_tm = {};
    // Copy the input date to host_tm and use that on mktime instead of the input directly
    // to avoid stack corruption on systems where tm size is different
    memcpy(&host_tm, date, sizeof(VitaTM));
    auto time_local = mktime(&host_tm);
    memcpy(date, &host_tm, sizeof(VitaTM));
    if (year_1900) {
        date->tm_year += 1900;
    }
    if (time)
        *time = static_cast<VitaTime>(time_local);
    if (param_3)
        *param_3 = time_local;
    return 0;
}

EXPORT(VitaTime, sceKernelLibcTime, VitaTime *time) {
    TRACY_FUNC(sceKernelLibcTime, time);
    const auto secs = (rtc_get_ticks(emuenv.kernel.base_tick.tick) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC;

    if (time) {
        *time = static_cast<VitaTime>(secs);
    }

    return static_cast<VitaTime>(secs);
}

EXPORT(int, sceKernelPowerLock, int unk) {
    TRACY_FUNC(sceKernelPowerLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPowerTick, SceKernelPowerTickType type) {
    TRACY_FUNC(sceKernelPowerTick, type);
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelPowerUnlock, int unk) {
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

EXPORT(int, sceKernelGetMainModuleSdkVersion) {
    TRACY_FUNC(sceKernelGetMainModuleSdkVersion);
    SceProcessParam *process_param = emuenv.kernel.process_param.get(emuenv.mem);
    if (process_param && (process_param->magic == '2PSP') && (process_param->version != 0)) {
        return process_param->fw_version;
    } else {
        LOG_DEBUG("return 0");
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
BRIDGE_IMPL(sceKernelGetMainModuleSdkVersion)
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
