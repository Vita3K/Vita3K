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

#include "ScePower.h"

#include <SDL_power.h>
#include <psp2/power.h>

#include <climits>

#define LOW_BATTERY_PERCENT 10

EXPORT(int, scePowerBatteryUpdateInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerCancelRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetArmClockFrequency) {
    return 444;
}

EXPORT(int, scePowerGetBatteryChargingStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryCycleCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryElec) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryFullCapacity) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryLifePercent) {
    int res;
    SDL_GetPowerInfo(NULL, &res);
    if (res == -1) {
        return 100;
    }
    return res;
}

EXPORT(int, scePowerGetBatteryLifeTime) {
    int res;
    SDL_GetPowerInfo(&res, NULL);
    if (res == -1) {
        return INT_MAX;
    }
    return res;
}

EXPORT(int, scePowerGetBatteryRemainCapacity) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryRemainLevel) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryRemainMaxLevel) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatterySOH) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryTemp) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBatteryVolt) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetBusClockFrequency) {
    return 222;
}

EXPORT(int, scePowerGetCaseTemp) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerGetGpuClockFrequency) {
    return 222;
}

EXPORT(int, scePowerGetGpuXbarClockFrequency) {
    return 166;
}

EXPORT(int, scePowerGetUsingWireless) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerIsBatteryCharging) {
    SDL_PowerState info = SDL_GetPowerInfo(NULL, NULL);
    return (info == SDL_POWERSTATE_CHARGING);
}

EXPORT(int, scePowerIsBatteryExist) {
    SDL_PowerState info = SDL_GetPowerInfo(NULL, NULL);
    return (info != SDL_POWERSTATE_NO_BATTERY);
}

EXPORT(int, scePowerIsLowBattery) {
    int res;
    SDL_GetPowerInfo(NULL, &res);
    if (res <= LOW_BATTERY_PERCENT) {
        return SCE_TRUE;
    }
    return SCE_FALSE;
}

EXPORT(int, scePowerIsLowBatteryInhibitUpdateDownload) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerIsLowBatteryInhibitUpdateReboot) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerIsPowerOnline) {
    SDL_PowerState info = SDL_GetPowerInfo(NULL, NULL);
    return ((info != SDL_POWERSTATE_UNKNOWN) && (info != SDL_POWERSTATE_ON_BATTERY));
}

EXPORT(int, scePowerIsRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerIsSuspendRequired) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRegisterCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRequestColdReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRequestDisplayOff) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRequestDisplayOn) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRequestStandby) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerRequestSuspend) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerSetArmClockFrequency, int freq) {
    if (freq < 0) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerSetBusClockFrequency, int freq) {
    if (freq < 0) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerSetConfigurationMode, int mode) {
    if (mode != 0x80 && mode != 0x800 && mode != 0x10880) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerSetGpuClockFrequency, int freq) {
    if (freq < 0) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerSetGpuXbarClockFrequency, int freq) {
    if (freq < 0) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerSetIdleTimerCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePowerSetUsingWireless, SceBool enabled) {
    if (enabled != SCE_TRUE && enabled != SCE_FALSE) {
        return SCE_POWER_ERROR_INVALID_VALUE;
    }
    return 0;
}

EXPORT(int, scePowerUnregisterCallback) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(scePowerBatteryUpdateInfo)
BRIDGE_IMPL(scePowerCancelRequest)
BRIDGE_IMPL(scePowerGetArmClockFrequency)
BRIDGE_IMPL(scePowerGetBatteryChargingStatus)
BRIDGE_IMPL(scePowerGetBatteryCycleCount)
BRIDGE_IMPL(scePowerGetBatteryElec)
BRIDGE_IMPL(scePowerGetBatteryFullCapacity)
BRIDGE_IMPL(scePowerGetBatteryLifePercent)
BRIDGE_IMPL(scePowerGetBatteryLifeTime)
BRIDGE_IMPL(scePowerGetBatteryRemainCapacity)
BRIDGE_IMPL(scePowerGetBatteryRemainLevel)
BRIDGE_IMPL(scePowerGetBatteryRemainMaxLevel)
BRIDGE_IMPL(scePowerGetBatterySOH)
BRIDGE_IMPL(scePowerGetBatteryTemp)
BRIDGE_IMPL(scePowerGetBatteryVolt)
BRIDGE_IMPL(scePowerGetBusClockFrequency)
BRIDGE_IMPL(scePowerGetCaseTemp)
BRIDGE_IMPL(scePowerGetGpuClockFrequency)
BRIDGE_IMPL(scePowerGetGpuXbarClockFrequency)
BRIDGE_IMPL(scePowerGetUsingWireless)
BRIDGE_IMPL(scePowerIsBatteryCharging)
BRIDGE_IMPL(scePowerIsBatteryExist)
BRIDGE_IMPL(scePowerIsLowBattery)
BRIDGE_IMPL(scePowerIsLowBatteryInhibitUpdateDownload)
BRIDGE_IMPL(scePowerIsLowBatteryInhibitUpdateReboot)
BRIDGE_IMPL(scePowerIsPowerOnline)
BRIDGE_IMPL(scePowerIsRequest)
BRIDGE_IMPL(scePowerIsSuspendRequired)
BRIDGE_IMPL(scePowerRegisterCallback)
BRIDGE_IMPL(scePowerRequestColdReset)
BRIDGE_IMPL(scePowerRequestDisplayOff)
BRIDGE_IMPL(scePowerRequestDisplayOn)
BRIDGE_IMPL(scePowerRequestStandby)
BRIDGE_IMPL(scePowerRequestSuspend)
BRIDGE_IMPL(scePowerSetArmClockFrequency)
BRIDGE_IMPL(scePowerSetBusClockFrequency)
BRIDGE_IMPL(scePowerSetConfigurationMode)
BRIDGE_IMPL(scePowerSetGpuClockFrequency)
BRIDGE_IMPL(scePowerSetGpuXbarClockFrequency)
BRIDGE_IMPL(scePowerSetIdleTimerCount)
BRIDGE_IMPL(scePowerSetUsingWireless)
BRIDGE_IMPL(scePowerUnregisterCallback)
