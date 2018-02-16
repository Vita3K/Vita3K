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

BRIDGE_DECL(sceRtcCheckValid)
BRIDGE_DECL(sceRtcCompareTick)
BRIDGE_DECL(sceRtcConvertLocalTimeToUtc)
BRIDGE_DECL(sceRtcConvertUtcToLocalTime)
BRIDGE_DECL(sceRtcFormatRFC2822)
BRIDGE_DECL(sceRtcFormatRFC2822LocalTime)
BRIDGE_DECL(sceRtcFormatRFC3339)
BRIDGE_DECL(sceRtcFormatRFC3339LocalTime)
BRIDGE_DECL(sceRtcGetCurrentClock)
BRIDGE_DECL(sceRtcGetCurrentClockLocalTime)
BRIDGE_DECL(sceRtcGetCurrentNetworkTick)
BRIDGE_DECL(sceRtcGetCurrentTick)
BRIDGE_DECL(sceRtcGetDayOfWeek)
BRIDGE_DECL(sceRtcGetDayOfYear)
BRIDGE_DECL(sceRtcGetDaysInMonth)
BRIDGE_DECL(sceRtcGetDosTime)
BRIDGE_DECL(sceRtcGetLastAdjustedTick)
BRIDGE_DECL(sceRtcGetLastReincarnatedTick)
BRIDGE_DECL(sceRtcGetTick)
BRIDGE_DECL(sceRtcGetTickResolution)
BRIDGE_DECL(sceRtcGetTime64_t)
BRIDGE_DECL(sceRtcGetTime_t)
BRIDGE_DECL(sceRtcGetWin32FileTime)
BRIDGE_DECL(sceRtcIsLeapYear)
BRIDGE_DECL(sceRtcParseDateTime)
BRIDGE_DECL(sceRtcParseRFC3339)
BRIDGE_DECL(sceRtcSetDosTime)
BRIDGE_DECL(sceRtcSetTick)
BRIDGE_DECL(sceRtcSetTime64_t)
BRIDGE_DECL(sceRtcSetTime_t)
BRIDGE_DECL(sceRtcSetWin32FileTime)
BRIDGE_DECL(sceRtcTickAddDays)
BRIDGE_DECL(sceRtcTickAddHours)
BRIDGE_DECL(sceRtcTickAddMicroseconds)
BRIDGE_DECL(sceRtcTickAddMinutes)
BRIDGE_DECL(sceRtcTickAddMonths)
BRIDGE_DECL(sceRtcTickAddSeconds)
BRIDGE_DECL(sceRtcTickAddTicks)
BRIDGE_DECL(sceRtcTickAddWeeks)
BRIDGE_DECL(sceRtcTickAddYears)
