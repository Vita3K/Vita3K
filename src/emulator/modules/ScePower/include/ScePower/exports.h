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

#pragma once

#include <module/module.h>

// ScePower
BRIDGE_DECL(scePowerBatteryUpdateInfo)
BRIDGE_DECL(scePowerCancelRequest)
BRIDGE_DECL(scePowerGetArmClockFrequency)
BRIDGE_DECL(scePowerGetBatteryChargingStatus)
BRIDGE_DECL(scePowerGetBatteryCycleCount)
BRIDGE_DECL(scePowerGetBatteryElec)
BRIDGE_DECL(scePowerGetBatteryFullCapacity)
BRIDGE_DECL(scePowerGetBatteryLifePercent)
BRIDGE_DECL(scePowerGetBatteryLifeTime)
BRIDGE_DECL(scePowerGetBatteryRemainCapacity)
BRIDGE_DECL(scePowerGetBatterySOH)
BRIDGE_DECL(scePowerGetBatteryTemp)
BRIDGE_DECL(scePowerGetBatteryVolt)
BRIDGE_DECL(scePowerGetBusClockFrequency)
BRIDGE_DECL(scePowerGetCaseTemp)
BRIDGE_DECL(scePowerGetGpuClockFrequency)
BRIDGE_DECL(scePowerGetGpuXbarClockFrequency)
BRIDGE_DECL(scePowerGetUsingWireless)
BRIDGE_DECL(scePowerIsBatteryCharging)
BRIDGE_DECL(scePowerIsBatteryExist)
BRIDGE_DECL(scePowerIsLowBattery)
BRIDGE_DECL(scePowerIsPowerOnline)
BRIDGE_DECL(scePowerIsRequest)
BRIDGE_DECL(scePowerIsSuspendRequired)
BRIDGE_DECL(scePowerRegisterCallback)
BRIDGE_DECL(scePowerRequestColdReset)
BRIDGE_DECL(scePowerRequestDisplayOff)
BRIDGE_DECL(scePowerRequestDisplayOn)
BRIDGE_DECL(scePowerRequestStandby)
BRIDGE_DECL(scePowerRequestSuspend)
BRIDGE_DECL(scePowerSetArmClockFrequency)
BRIDGE_DECL(scePowerSetBusClockFrequency)
BRIDGE_DECL(scePowerSetConfigurationMode)
BRIDGE_DECL(scePowerSetGpuClockFrequency)
BRIDGE_DECL(scePowerSetGpuXbarClockFrequency)
BRIDGE_DECL(scePowerSetIdleTimerCount)
BRIDGE_DECL(scePowerSetUsingWireless)
BRIDGE_DECL(scePowerUnregisterCallback)
