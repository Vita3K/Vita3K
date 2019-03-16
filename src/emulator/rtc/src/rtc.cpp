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

#include <rtc/rtc.h>

#include <util/log.h>

std::uint64_t rtc_base_ticks() {
    const auto now = std::chrono::system_clock::now();
    const auto now_timepoint = std::chrono::time_point_cast<VitaClocks>(now);
    const auto clocks_since_unix_time = now_timepoint.time_since_epoch().count();

    // host high_resolution_clock offset (implementations dependant)
    const auto host_clock_offset = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    return RTC_OFFSET + clocks_since_unix_time - host_clock_offset;
}

std::uint64_t rtc_get_ticks(uint64_t base_ticks) {
    const auto now = std::chrono::high_resolution_clock::now();
    const auto now_timepoint = std::chrono::time_point_cast<VitaClocks>(now);
    const uint64_t now_ticks = now_timepoint.time_since_epoch().count();

    return base_ticks + now_ticks;
}

#ifdef WIN32
std::uint64_t convert_filetime(const _FILETIME &filetime) {
    // Microseconds between 1601-01-01 00:00:00 UTC and 1970-01-01 00:00:00 UTC
    static const uint64_t EPOCH_DIFFERENCE_MICROS = 11644473600000000ull;

    // First convert 100-ns intervals to microseconds, then adjust for the
    // epoch difference
    uint64_t total_us = (((uint64_t)filetime.dwHighDateTime << 32) | (uint64_t)filetime.dwLowDateTime) / 10;
    total_us -= EPOCH_DIFFERENCE_MICROS;
    return total_us;
}
#else
std::uint64_t convert_timespec(const timespec &timespec) {
    return timespec.tv_sec * 1'000'000 + timespec.tv_nsec / 1'000;
}
#endif

// The following functions are from PPSSPP
// Copyright (c) 2012- PPSSPP Project.

#if defined(_WIN32)
time_t rtc_timegm(struct tm *tm) { return _mkgmtime(tm); }
#elif (defined(__GLIBC__) && !defined(__ANDROID__))
#define rtc_timegm timegm
#else
time_t rtc_timegm(struct tm *tm) {
    time_t ret;
    char *tz;
    std::string tzcopy;

    tz = getenv("TZ");
    if (tz)
        tzcopy = tz;

    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
        setenv("TZ", tzcopy.c_str(), 1);
    else
        unsetenv("TZ");
    tzset();
    return ret;
}
#endif

void __RtcPspTimeToTm(tm *val, const SceDateTime *pt) {
    val->tm_year = pt->year - 1900;
    val->tm_mon = pt->month - 1;
    val->tm_mday = pt->day;
    val->tm_wday = -1;
    val->tm_yday = -1;
    val->tm_hour = pt->hour;
    val->tm_min = pt->minute;
    val->tm_sec = pt->second;
    val->tm_isdst = 0;
}

void __RtcTicksToPspTime(SceDateTime *t, std::uint64_t ticks) {
    int numYearAdd = 0;
    if (ticks < 1000000ULL) {
        t->year = 1;
        t->month = 1;
        t->day = 1;
        t->hour = 0;
        t->minute = 0;
        t->second = 0;
        t->microsecond = ticks % 1000000ULL;
        return;
    } else if (ticks < RTC_OFFSET) {
        // Need to get a year past 1970 for gmtime
        // Add enough 400 year to pass over 1970.
        numYearAdd = (int)((RTC_OFFSET - ticks) / RTC_400_YEAR_TICKS + 1);
        ticks += RTC_400_YEAR_TICKS * numYearAdd;
    }

    while (ticks >= RTC_OFFSET + RTC_400_YEAR_TICKS) {
        ticks -= RTC_400_YEAR_TICKS;
        --numYearAdd;
    }

    time_t time = (ticks - RTC_OFFSET) / 1000000ULL;
    t->microsecond = ticks % 1000000ULL;

    tm *local = gmtime(&time);
    if (!local) {
        LOG_ERROR("Date is too high/low to handle, pretending to work.");
        return;
    }

    t->year = local->tm_year + 1900 - numYearAdd * 400;
    t->month = local->tm_mon + 1;
    t->day = local->tm_mday;
    t->hour = local->tm_hour;
    t->minute = local->tm_min;
    t->second = local->tm_sec;
}

std::uint64_t __RtcPspTimeToTicks(const SceDateTime *pt) {
    tm local;
    __RtcPspTimeToTm(&local, pt);

    std::int64_t tickOffset = 0;
    while (local.tm_year < 70) {
        tickOffset -= RTC_400_YEAR_TICKS;
        local.tm_year += 400;
    }
    while (local.tm_year >= 470) {
        tickOffset += RTC_400_YEAR_TICKS;
        local.tm_year -= 400;
    }

    time_t seconds = rtc_timegm(&local);
    std::uint64_t result = RTC_OFFSET + (std::uint64_t)seconds * 1000000ULL;
    result += pt->microsecond;
    return result + tickOffset;
}
