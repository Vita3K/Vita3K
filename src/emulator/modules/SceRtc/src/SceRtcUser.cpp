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

#include <SceRtc/exports.h>

#include <psp2/rtc.h>

#include <ctime>

EXPORT(int, sceRtcCheckValid) {
    return unimplemented("sceRtcCheckValid");
}

EXPORT(int, sceRtcCompareTick) {
    return unimplemented("sceRtcCompareTick");
}

EXPORT(int, sceRtcConvertLocalTimeToUtc) {
    return unimplemented("sceRtcConvertLocalTimeToUtc");
}

EXPORT(int, sceRtcConvertUtcToLocalTime) {
    return unimplemented("sceRtcConvertUtcToLocalTime");
}

EXPORT(int, sceRtcFormatRFC2822) {
    return unimplemented("sceRtcFormatRFC2822");
}

EXPORT(int, sceRtcFormatRFC2822LocalTime) {
    return unimplemented("sceRtcFormatRFC2822LocalTime");
}

EXPORT(int, sceRtcFormatRFC3339) {
    return unimplemented("sceRtcFormatRFC3339");
}

EXPORT(int, sceRtcFormatRFC3339LocalTime) {
    return unimplemented("sceRtcFormatRFC3339LocalTime");
}

EXPORT(int, sceRtcGetCurrentClock) {
    return unimplemented("sceRtcGetCurrentClock");
}

EXPORT(int, sceRtcGetCurrentClockLocalTime) {
    return unimplemented("sceRtcGetCurrentClockLocalTime");
}

EXPORT(int, sceRtcGetCurrentNetworkTick) {
    return unimplemented("sceRtcGetCurrentNetworkTick");
}

EXPORT(int, sceRtcGetCurrentTick, SceRtcTick *tick) {
    assert(tick != nullptr);

    // TODO Assumes game handles varying CLOCKS_PER_SEC.
    tick->tick = clock();

    return 0;
}

EXPORT(int, sceRtcGetDayOfWeek) {
    return unimplemented("sceRtcGetDayOfWeek");
}

EXPORT(int, sceRtcGetDayOfYear) {
    return unimplemented("sceRtcGetDayOfYear");
}

EXPORT(int, sceRtcGetDaysInMonth) {
    return unimplemented("sceRtcGetDaysInMonth");
}

EXPORT(int, sceRtcGetDosTime) {
    return unimplemented("sceRtcGetDosTime");
}

EXPORT(int, sceRtcGetLastAdjustedTick) {
    return unimplemented("sceRtcGetLastAdjustedTick");
}

EXPORT(int, sceRtcGetLastReincarnatedTick) {
    return unimplemented("sceRtcGetLastReincarnatedTick");
}

EXPORT(int, sceRtcGetTick) {
    return unimplemented("sceRtcGetTick");
}

EXPORT(unsigned int, sceRtcGetTickResolution) {
    // TODO Check the Vita's value.
    return CLOCKS_PER_SEC;
}

EXPORT(int, sceRtcGetTime64_t) {
    return unimplemented("sceRtcGetTime64_t");
}

EXPORT(int, sceRtcGetTime_t) {
    return unimplemented("sceRtcGetTime_t");
}

EXPORT(int, sceRtcGetWin32FileTime) {
    return unimplemented("sceRtcGetWin32FileTime");
}

EXPORT(int, sceRtcIsLeapYear) {
    return unimplemented("sceRtcIsLeapYear");
}

EXPORT(int, sceRtcParseDateTime) {
    return unimplemented("sceRtcParseDateTime");
}

EXPORT(int, sceRtcParseRFC3339) {
    return unimplemented("sceRtcParseRFC3339");
}

EXPORT(int, sceRtcSetDosTime) {
    return unimplemented("sceRtcSetDosTime");
}

EXPORT(int, sceRtcSetTick) {
    return unimplemented("sceRtcSetTick");
}

EXPORT(int, sceRtcSetTime64_t) {
    return unimplemented("sceRtcSetTime64_t");
}

EXPORT(int, sceRtcSetTime_t) {
    return unimplemented("sceRtcSetTime_t");
}

EXPORT(int, sceRtcSetWin32FileTime) {
    return unimplemented("sceRtcSetWin32FileTime");
}

EXPORT(int, sceRtcTickAddDays) {
    return unimplemented("sceRtcTickAddDays");
}

EXPORT(int, sceRtcTickAddHours) {
    return unimplemented("sceRtcTickAddHours");
}

EXPORT(int, sceRtcTickAddMicroseconds) {
    return unimplemented("sceRtcTickAddMicroseconds");
}

EXPORT(int, sceRtcTickAddMinutes) {
    return unimplemented("sceRtcTickAddMinutes");
}

EXPORT(int, sceRtcTickAddMonths) {
    return unimplemented("sceRtcTickAddMonths");
}

EXPORT(int, sceRtcTickAddSeconds) {
    return unimplemented("sceRtcTickAddSeconds");
}

EXPORT(int, sceRtcTickAddTicks) {
    return unimplemented("sceRtcTickAddTicks");
}

EXPORT(int, sceRtcTickAddWeeks) {
    return unimplemented("sceRtcTickAddWeeks");
}

EXPORT(int, sceRtcTickAddYears) {
    return unimplemented("sceRtcTickAddYears");
}

BRIDGE_IMPL(sceRtcCheckValid)
BRIDGE_IMPL(sceRtcCompareTick)
BRIDGE_IMPL(sceRtcConvertLocalTimeToUtc)
BRIDGE_IMPL(sceRtcConvertUtcToLocalTime)
BRIDGE_IMPL(sceRtcFormatRFC2822)
BRIDGE_IMPL(sceRtcFormatRFC2822LocalTime)
BRIDGE_IMPL(sceRtcFormatRFC3339)
BRIDGE_IMPL(sceRtcFormatRFC3339LocalTime)
BRIDGE_IMPL(sceRtcGetCurrentClock)
BRIDGE_IMPL(sceRtcGetCurrentClockLocalTime)
BRIDGE_IMPL(sceRtcGetCurrentNetworkTick)
BRIDGE_IMPL(sceRtcGetCurrentTick)
BRIDGE_IMPL(sceRtcGetDayOfWeek)
BRIDGE_IMPL(sceRtcGetDayOfYear)
BRIDGE_IMPL(sceRtcGetDaysInMonth)
BRIDGE_IMPL(sceRtcGetDosTime)
BRIDGE_IMPL(sceRtcGetLastAdjustedTick)
BRIDGE_IMPL(sceRtcGetLastReincarnatedTick)
BRIDGE_IMPL(sceRtcGetTick)
BRIDGE_IMPL(sceRtcGetTickResolution)
BRIDGE_IMPL(sceRtcGetTime64_t)
BRIDGE_IMPL(sceRtcGetTime_t)
BRIDGE_IMPL(sceRtcGetWin32FileTime)
BRIDGE_IMPL(sceRtcIsLeapYear)
BRIDGE_IMPL(sceRtcParseDateTime)
BRIDGE_IMPL(sceRtcParseRFC3339)
BRIDGE_IMPL(sceRtcSetDosTime)
BRIDGE_IMPL(sceRtcSetTick)
BRIDGE_IMPL(sceRtcSetTime64_t)
BRIDGE_IMPL(sceRtcSetTime_t)
BRIDGE_IMPL(sceRtcSetWin32FileTime)
BRIDGE_IMPL(sceRtcTickAddDays)
BRIDGE_IMPL(sceRtcTickAddHours)
BRIDGE_IMPL(sceRtcTickAddMicroseconds)
BRIDGE_IMPL(sceRtcTickAddMinutes)
BRIDGE_IMPL(sceRtcTickAddMonths)
BRIDGE_IMPL(sceRtcTickAddSeconds)
BRIDGE_IMPL(sceRtcTickAddTicks)
BRIDGE_IMPL(sceRtcTickAddWeeks)
BRIDGE_IMPL(sceRtcTickAddYears)
