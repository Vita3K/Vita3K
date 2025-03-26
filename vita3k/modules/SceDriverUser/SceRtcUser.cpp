// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "../SceRtc/SceRtc.h"
#include <module/module.h>

#include <rtc/rtc.h>

#include <util/safe_time.h>

#include <chrono>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceRtcUser);

#define VITA_CLOCKS_PER_SEC 1000000

EXPORT(int, sceRtcCheckValid, const SceDateTime *pTime) {
    TRACY_FUNC(sceRtcCheckValid, pTime);
    if (pTime == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }
    if (pTime->month < 1 || pTime->month > 12) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (pTime->day < 1) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
    }
    switch (pTime->month) {
    case 4: // April
    case 6: // June
    case 9: // September
    case 11: // November
        if (pTime->day > 30) {
            return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    case 2: // February
        if ((pTime->year % 400 == 0) || (pTime->year % 100 != 0 && pTime->year % 4 == 0)) {
            if (pTime->day > 29) {
                return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
            }
        } else {
            if (pTime->day > 28) {
                return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
            }
        }
        break;
    default:
        if (pTime->day > 31) {
            return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    }
    if (pTime->hour > 23) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_HOUR);
    }
    if (pTime->minute > 59) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_MINUTE);
    }
    if (pTime->second > 59) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_SECOND);
    }
    if (pTime->microsecond > 99999) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_MICROSECOND);
    }
    return 0;
}

EXPORT(int, sceRtcCompareTick, const SceRtcTick *tick1, const SceRtcTick *tick2) {
    TRACY_FUNC(sceRtcCompareTick, tick1->tick, tick2->tick);
    return (tick1->tick < tick2->tick) ? -1 : ((tick1->tick > tick2->tick) ? 1 : 0);
}

EXPORT(int, sceRtcConvertLocalTimeToUtc, const SceRtcTick *pLocalTime, SceRtcTick *pUtc) {
    TRACY_FUNC(sceRtcConvertLocalTimeToUtc, pLocalTime, pUtc);
    return CALL_EXPORT(_sceRtcConvertLocalTimeToUtc, pLocalTime, pUtc);
}

EXPORT(int, sceRtcConvertUtcToLocalTime, const SceRtcTick *pUtc, SceRtcTick *pLocalTime) {
    TRACY_FUNC(sceRtcConvertUtcToLocalTime, pUtc, pLocalTime);
    return CALL_EXPORT(_sceRtcConvertUtcToLocalTime, pUtc, pLocalTime);
}

EXPORT(int, sceRtcFormatRFC2822) {
    TRACY_FUNC(sceRtcFormatRFC2822);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcFormatRFC2822LocalTime, char *pszDateTime, const SceRtcTick *utc) {
    TRACY_FUNC(sceRtcFormatRFC2822LocalTime, pszDateTime, utc);
    // The following code is from PPSSPP
    // Copyright (c) 2012- PPSSPP Project.

    // Get timezone difference
    std::time_t epoch_plus_11h = 60 * 60 * 11;
    tm epoch_localtime = {};
    tm epoch_gmtime = {};
    SAFE_LOCALTIME(&epoch_plus_11h, &epoch_localtime);
    SAFE_GMTIME(&epoch_plus_11h, &epoch_gmtime);

    auto local_tz_hour = epoch_localtime.tm_hour;
    const auto local_tz_minute = epoch_localtime.tm_min;
    const auto gmt_tz_hour = epoch_gmtime.tm_hour;
    const auto gmt_tz_minute = epoch_gmtime.tm_min;
    const auto tz_minute_diff = local_tz_minute - gmt_tz_minute;
    if (tz_minute_diff != 0 && gmt_tz_hour > local_tz_hour)
        local_tz_hour++;
    const auto tz_hour_diff = local_tz_hour - gmt_tz_hour;

    if (utc) { // format utc in localtime
        SceDateTime date;
        memset(&date, 0, sizeof(date));
        tm gmt = {};
        __RtcTicksToPspTime(&date, utc->tick);
        __RtcPspTimeToTm(&gmt, &date);
        while (gmt.tm_year < 70)
            gmt.tm_year += 400;
        while (gmt.tm_year >= 470)
            gmt.tm_year -= 400;

        time_t time = rtc_timegm(&gmt);
        tm current_localtime = {};
        SAFE_LOCALTIME(&time, &current_localtime);

        char *end = pszDateTime + 32;
        pszDateTime += strftime(pszDateTime, end - pszDateTime, "%a, %d %b ", &current_localtime);
        pszDateTime += snprintf(pszDateTime, end - pszDateTime, "%04d", date.year);
        pszDateTime += strftime(pszDateTime, end - pszDateTime, " %H:%M:%S ", &current_localtime);

        if (local_tz_hour < gmt_tz_hour || current_localtime.tm_mday < gmt.tm_mday) {
            pszDateTime += snprintf(pszDateTime, end - pszDateTime, "-%02d%02d", abs(tz_hour_diff), abs(tz_minute_diff));
        } else {
            pszDateTime += snprintf(pszDateTime, end - pszDateTime, "+%02d%02d", abs(tz_hour_diff), abs(tz_minute_diff));
        }
    } else { // format current time
        time_t time = std::time(nullptr);
        tm local_time = {};
        tm gmt = {};

        SAFE_LOCALTIME(&time, &local_time);
        SAFE_GMTIME(&time, &gmt);

        char *end = pszDateTime + 32;
        pszDateTime += strftime(pszDateTime, end - pszDateTime, "%a, %d %b ", &local_time);
        pszDateTime += snprintf(pszDateTime, end - pszDateTime, "%04d", local_time.tm_year + 1900);
        pszDateTime += strftime(pszDateTime, end - pszDateTime, " %H:%M:%S ", &local_time);

        if (local_tz_hour < gmt_tz_hour || local_time.tm_mday < gmt.tm_mday) {
            pszDateTime += snprintf(pszDateTime, end - pszDateTime, "-%02d%02d", abs(tz_hour_diff), abs(tz_minute_diff));
        } else {
            pszDateTime += snprintf(pszDateTime, end - pszDateTime, "+%02d%02d", abs(tz_hour_diff), abs(tz_minute_diff));
        }
    }
    return 0;
}

EXPORT(int, sceRtcFormatRFC3339) {
    TRACY_FUNC(sceRtcFormatRFC3339);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcFormatRFC3339LocalTime) {
    TRACY_FUNC(sceRtcFormatRFC3339LocalTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetCurrentClock, SceDateTime *datePtr, int iTimeZone) {
    TRACY_FUNC(sceRtcGetCurrentClock, datePtr, iTimeZone);
    return CALL_EXPORT(_sceRtcGetCurrentClock, datePtr, iTimeZone);
}

EXPORT(int, sceRtcGetCurrentClockLocalTime, SceDateTime *datePtr) {
    TRACY_FUNC(sceRtcGetCurrentClockLocalTime, datePtr);
    return CALL_EXPORT(_sceRtcGetCurrentClockLocalTime, datePtr);
}

EXPORT(int, sceRtcGetCurrentAdNetworkTick) {
    TRACY_FUNC(sceRtcGetCurrentAdNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetCurrentNetworkTick, SceRtcTick *tick) {
    TRACY_FUNC(sceRtcGetCurrentNetworkTick, tick);
    return CALL_EXPORT(_sceRtcGetCurrentNetworkTick, tick);
}

EXPORT(int, sceRtcGetCurrentDebugNetworkTick) {
    TRACY_FUNC(sceRtcGetCurrentDebugNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetCurrentTick, SceRtcTick *tick) {
    TRACY_FUNC(sceRtcGetCurrentTick, tick);
    return CALL_EXPORT(_sceRtcGetCurrentTick, tick);
}

EXPORT(int, sceRtcGetCurrentGpsTick) {
    TRACY_FUNC(sceRtcGetCurrentGpsTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetCurrentRetainedNetworkTick) {
    TRACY_FUNC(sceRtcGetCurrentRetainedNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetDayOfWeek, int year, int month, int day) {
    TRACY_FUNC(sceRtcGetDayOfWeek, year, month, day);
    if (month < 1 || month > 12) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (year < 0) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_YEAR);
    }
    if (day < 1) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
    }
    switch (month) {
    case 4: // April
    case 6: // June
    case 9: // September
    case 11: // November
        if (day > 30) {
            return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    case 2: // February
        if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)) {
            if (day > 29) {
                return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
            }
        } else {
            if (day > 28) {
                return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
            }
        }
        break;
    default:
        if (day > 31) {
            return RET_ERROR(SCE_RTC_ERROR_INVALID_DAY);
        }
        break;
    }

    // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Implementation-dependent_methods
    day += month < 3 ? year-- : year - 2;
    int weekday = (23 * month / 9 + day + 4 + year / 4 - year / 100 + year / 400) % 7;
    return weekday;
}

EXPORT(int, sceRtcGetDayOfYear) {
    TRACY_FUNC(sceRtcGetDayOfYear);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetDaysInMonth, int year, int month) {
    TRACY_FUNC(sceRtcGetDaysInMonth, year, month);
    if (month < 1 || month > 12) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_MONTH);
    }
    if (year < 0) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_YEAR);
    }

    switch (month) {
    case 4: // April
    case 6: // June
    case 9: // September
    case 11: // November
        return 30;
    case 2: // February
        if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)) {
            return 29;
        } else {
            return 28;
        }
    }
    return 31;
}

EXPORT(int, sceRtcGetDosTime, const SceDateTime *datePtr, uint32_t *dosTimePtr) {
    TRACY_FUNC(sceRtcGetDosTime, datePtr, dosTimePtr);
    // The following code is from PPSSPP
    // Copyright (c) 2012- PPSSPP Project.
    if (datePtr->year < 1980) {
        *dosTimePtr = 0;
        return RET_ERROR(SCE_RTC_ERROR_INVALID_VALUE);
    } else if (datePtr->year >= 2108) {
        *dosTimePtr = 0xFF9FBF7D;
        return RET_ERROR(SCE_RTC_ERROR_INVALID_VALUE);
    }

    int year = ((datePtr->year - 1980) & 0x7F) << 9;
    int month = (datePtr->month & 0xF) << 5;
    int hour = (datePtr->hour & 0x1F) << 11;
    int minute = (datePtr->minute & 0x3F) << 5;
    int day = datePtr->day & 0x1F;
    int second = (datePtr->second >> 1) & 0x1F;
    int ymd = year | month | day;
    int hms = hour | minute | second;

    *dosTimePtr = (ymd << 16) | hms;

    return 0;
}

EXPORT(int, sceRtcGetLastAdjustedTick) {
    TRACY_FUNC(sceRtcGetLastAdjustedTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetLastReincarnatedTick) {
    TRACY_FUNC(sceRtcGetLastReincarnatedTick);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetTick, SceDateTime *datePtr, SceRtcTick *pTick) {
    TRACY_FUNC(sceRtcGetTick, datePtr, pTick);
    if (datePtr == nullptr || pTick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick->tick = __RtcPspTimeToTicks(datePtr);

    return 0;
}

EXPORT(unsigned int, sceRtcGetTickResolution) {
    TRACY_FUNC(sceRtcGetTickResolution);
    return VITA_CLOCKS_PER_SEC;
}

EXPORT(int, sceRtcGetTime_t, SceDateTime *datePtr, uint32_t *timePtr) {
    TRACY_FUNC(sceRtcGetTime_t, datePtr, timePtr);
    if (datePtr == nullptr || timePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    *timePtr = static_cast<std::uint32_t>((__RtcPspTimeToTicks(datePtr) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC);
    return 0;
}

EXPORT(int, sceRtcGetTime64_t, SceDateTime *datePtr, uint64_t *timePtr) {
    TRACY_FUNC(sceRtcGetTime64_t, datePtr, timePtr);
    if (datePtr == nullptr || timePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    *timePtr = (__RtcPspTimeToTicks(datePtr) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC;
    return 0;
}

// This is the # of microseconds between January 1, 0001 and January 1, 1601 (for Win32 FILETIME.)
constexpr uint64_t rtcFiletimeOffset = 50491123200000000ULL;

EXPORT(int, sceRtcGetWin32FileTime, const SceDateTime *datePtr, SceUInt64 *win32TimePtr) {
    TRACY_FUNC(sceRtcGetWin32FileTime, datePtr, win32TimePtr);
    if (!datePtr || !win32TimePtr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    auto ticks = __RtcPspTimeToTicks(datePtr);
    if (ticks < rtcFiletimeOffset) {
        *win32TimePtr = 0;
        return RET_ERROR(SCE_RTC_ERROR_INVALID_VALUE);
    }
    *win32TimePtr = (ticks - rtcFiletimeOffset) * 10;
    return 0;
}

EXPORT(int, sceRtcIsLeapYear, int year) {
    TRACY_FUNC(sceRtcIsLeapYear, year);
    if (year < 0) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_YEAR);
    }

    if ((year % 400 == 0) || (year % 100 != 0 && year % 4 == 0)) {
        return 1;
    }
    return 0;
}

EXPORT(int, sceRtcParseDateTime) {
    TRACY_FUNC(sceRtcParseDateTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcParseRFC3339) {
    TRACY_FUNC(sceRtcParseRFC3339);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcSetDosTime) {
    TRACY_FUNC(sceRtcSetDosTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcSetTick, SceDateTime *datePtr, const SceRtcTick *pTick) {
    TRACY_FUNC(sceRtcSetTick, datePtr, pTick);
    if (datePtr == nullptr || pTick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    __RtcTicksToPspTime(datePtr, pTick->tick);
    return 0;
}

EXPORT(int, sceRtcSetTime64_t, SceDateTime *datePtr, uint64_t iTime) {
    TRACY_FUNC(sceRtcSetTime64_t, datePtr, iTime);
    if (datePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    uint64_t ticks = RTC_OFFSET + iTime * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, ticks);
    return 0;
}

EXPORT(int, sceRtcSetTime_t, SceDateTime *datePtr, uint32_t iTime) {
    TRACY_FUNC(sceRtcSetTime_t, datePtr, iTime);
    if (datePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    uint64_t ticks = RTC_OFFSET + iTime * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, ticks);
    return 0;
}

EXPORT(int, sceRtcSetWin32FileTime) {
    TRACY_FUNC(sceRtcSetWin32FileTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcTickAddDays, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceInt lAdd) {
    TRACY_FUNC(sceRtcTickAddDays, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 86400ull);
    return 0;
}

EXPORT(int, sceRtcTickAddHours, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceInt lAdd) {
    TRACY_FUNC(sceRtcTickAddHours, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 3600ull);
    return 0;
}

EXPORT(int, sceRtcTickAddMicroseconds, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    TRACY_FUNC(sceRtcTickAddMicroseconds, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC / 1000);
    return 0;
}

EXPORT(int, sceRtcTickAddMinutes, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    TRACY_FUNC(sceRtcTickAddMinutes, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 60);
    return 0;
}

EXPORT(int, sceRtcTickAddMonths, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceInt iAdd) {
    TRACY_FUNC(sceRtcTickAddMonths, pTick0, pTick1, iAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }
    SceDateTime t{};
    __RtcTicksToPspTime(&t, pTick1->tick);
    if (t.day == 0) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_VALUE);
    }
    int months = t.year * 12 + t.month - 1 + iAdd;
    t.year = months / 12;
    t.month = months % 12 + 1;
    if (t.year == 0)
        return RET_ERROR(SCE_RTC_ERROR_INVALID_YEAR);
    int days_in_month = CALL_EXPORT(sceRtcGetDaysInMonth, t.year, t.month);
    if (t.day > days_in_month)
        t.day = days_in_month;
    pTick0->tick = __RtcPspTimeToTicks(&t);
    return 0;
}

EXPORT(int, sceRtcTickAddSeconds, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    TRACY_FUNC(sceRtcTickAddSeconds, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * VITA_CLOCKS_PER_SEC;
    return 0;
}

EXPORT(int, sceRtcTickAddTicks, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceLong64 lAdd) {
    TRACY_FUNC(sceRtcTickAddTicks, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd;
    return 0;
}

EXPORT(int, sceRtcTickAddWeeks, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceInt lAdd) {
    TRACY_FUNC(sceRtcTickAddWeeks, pTick0, pTick1, lAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    pTick0->tick = pTick1->tick + lAdd * (VITA_CLOCKS_PER_SEC * 604800ull);
    return 0;
}

EXPORT(int, sceRtcTickAddYears, SceRtcTick *pTick0, const SceRtcTick *pTick1, SceInt iAdd) {
    TRACY_FUNC(sceRtcTickAddYears, pTick0, pTick1, iAdd);
    if (pTick0 == nullptr || pTick1 == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }
    SceDateTime t{};
    __RtcTicksToPspTime(&t, pTick1->tick);
    if (t.day == 0) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_VALUE);
    }
    t.year += iAdd;
    if (t.year == 0)
        return RET_ERROR(SCE_RTC_ERROR_INVALID_YEAR);
    int days_in_month = CALL_EXPORT(sceRtcGetDaysInMonth, t.year, t.month);
    if (t.day > days_in_month)
        t.day = days_in_month;
    pTick0->tick = __RtcPspTimeToTicks(&t);
    return 0;
}
