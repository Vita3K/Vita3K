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

#include "SceRtcUser.h"

#include <host/rtc.h>

#include <chrono>

#define VITA_CLOCKS_PER_SEC 1000000

EXPORT(int, sceRtcCheckValid, const SceDateTime *pTime) {
    if (pTime == nullptr){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_POINTER);
    }
    if (pTime->month < 1 || pTime->month > 12){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (pTime->day < 1){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_DAY);
    }
    switch (pTime->month){
    case 4:  // April
    case 6:  // June
    case 9:  // September
    case 11: // November
        if (pTime->day > 30){
            return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    case 2: // February
        if ((pTime->year % 400 == 0) || (pTime->year % 100 != 0 && pTime->year % 4 == 0)){
            if (pTime->day > 29){
                return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_DAY);
            }
        }else{
            if (pTime->day > 28){
                return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_DAY);
            }
        }
        break;
    default:
        if (pTime->day > 31){
            return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    }
    if (pTime->hour > 23){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_HOUR);
    }
    if (pTime->minute > 59){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_MINUTE);
    }
    if (pTime->second > 59){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_SECOND);
    }
    if (pTime->microsecond > 99999){
        return error("sceRtcCheckValid", SCE_RTC_ERROR_INVALID_MICROSECOND);
    }
    return 0;
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
    if (tick == nullptr){
        return error("sceRtcGetCurrentTick", SCE_RTC_ERROR_INVALID_POINTER);
    }

    tick->tick = rtc_get_ticks(host);

    return 0;
}

EXPORT(int, sceRtcGetDayOfWeek, int year, int month, int day) {
    if (month < 1 || month > 12){
        return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (year < 0){
        return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_YEAR);
    }
    if (day < 1){
        return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_DAY);
    }
    switch (month){
    case 4:  // April
    case 6:  // June
    case 9:  // September
    case 11: // November
        if (day > 30){
            return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    case 2: // February
        if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)){
            if (day > 29){
                return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_DAY);
            }
        }else{
            if (day > 28){
                return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_DAY);
            }
        }
        break;
    default:
        if (day > 31){
            return error("sceRtcGetDayOfWeek", SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    }

    // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Implementation-dependent_methods
    int weekday  = (day += month < 3 ? year-- : year - 2, 23*month/9 + day + 4 + year/4- year/100 + year/400)%7;
    return weekday;
}

EXPORT(int, sceRtcGetDayOfYear) {
    return unimplemented("sceRtcGetDayOfYear");
}

EXPORT(int, sceRtcGetDaysInMonth, int year, int month) {
    if (month < 1 || month > 12){
        return error("sceRtcGetDaysInMonth", SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (year < 0){
        return error("sceRtcGetDaysInMonth", SCE_RTC_ERROR_INVALID_YEAR);
    }

    switch (month){
    case 4:  // April
    case 6:  // June
    case 9:  // September
    case 11: // November
        return 30;
    case 2: // February
        if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)){
            return 29;
        }else{
            return 28;
        }
    }
    return 31;
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
    return VITA_CLOCKS_PER_SEC;
}

template<typename TimePtrType>
int sceRtcGetTime(MemState& mem, Ptr<SceDateTime> datePtr, TimePtrType timePtr) {
    if (datePtr && timePtr) {
        const auto datetime = *datePtr.get(mem);
        const auto ticks = __RtcPspTimeToTicks(datetime);

        std::uint32_t result = ticks - RTC_OFFSET / VITA_CLOCKS_PER_SEC;

        *timePtr.get(mem) = result;
    } else {
        return 1;
    }
    return 0;
}

EXPORT(int, sceRtcGetTime_t, Ptr<SceDateTime> datePtr, Ptr<std::uint32_t> timePtr) {
    return sceRtcGetTime(host.mem, datePtr, timePtr);
}

EXPORT(int, sceRtcGetTime64_t, Ptr<SceDateTime> datePtr, Ptr<std::uint64_t> timePtr) {
    return sceRtcGetTime(host.mem, datePtr, timePtr);
}

EXPORT(int, sceRtcGetWin32FileTime) {
    return unimplemented("sceRtcGetWin32FileTime");
}

EXPORT(int, sceRtcIsLeapYear, int year) {
    if (year < 0){
        return error("sceRtcIsLeapYear", SCE_RTC_ERROR_INVALID_YEAR);
    }

    if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)){
        return 1;
    }
    return 0;
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

EXPORT(int, sceRtcTickAddDays, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddDays", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 86400);
    return 0;
}

EXPORT(int, sceRtcTickAddHours, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddHours", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 3600);
    return 0;
}

EXPORT(int, sceRtcTickAddMicroseconds, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddMicroseconds", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC / 1000);
    return 0;
}

EXPORT(int, sceRtcTickAddMinutes, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddMinutes", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 60);
    return 0;
}

EXPORT(int, sceRtcTickAddMonths) {
    return unimplemented("sceRtcTickAddMonths");
}

EXPORT(int, sceRtcTickAddSeconds, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddSeconds", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * VITA_CLOCKS_PER_SEC;
    return 0;
}

EXPORT(int, sceRtcTickAddTicks, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddTicks", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd;
    return 0;
}

EXPORT(int, sceRtcTickAddWeeks, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    if (pTick0 == nullptr || pTick1 == nullptr){
        return error("sceRtcTickAddWeeks", SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 604800);
    return 0;
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
