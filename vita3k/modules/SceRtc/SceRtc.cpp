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

#include "SceRtc.h"

#include <rtc/rtc.h>

#include <util/safe_time.h>

#include <chrono>
#include <ctime>

#define VITA_CLOCKS_PER_SEC 1000000

EXPORT(int, _sceRtcConvertLocalTimeToUtc, const SceRtcTick *pLocalTime, SceRtcTick *pUtc) {
    if (pUtc == nullptr || pLocalTime == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }
    std::time_t t = std::time(nullptr);

    tm local_tm = {};
    tm gmt_tm = {};

    SAFE_LOCALTIME(&t, &local_tm);
    SAFE_GMTIME(&t, &gmt_tm);

    std::time_t local = std::mktime(&local_tm);
    std::time_t gmt = std::mktime(&gmt_tm);
    pUtc->tick = pLocalTime->tick - (local - gmt) * VITA_CLOCKS_PER_SEC;

    return 0;
}

EXPORT(int, _sceRtcConvertUtcToLocalTime, const SceRtcTick *pUtc, SceRtcTick *pLocalTime) {
    if (pUtc == nullptr || pLocalTime == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    std::time_t t = std::time(nullptr);

    tm local_tm = {};
    tm gmt_tm = {};

    SAFE_LOCALTIME(&t, &local_tm);
    SAFE_GMTIME(&t, &gmt_tm);

    std::time_t local = std::mktime(&local_tm);
    std::time_t gmt = std::mktime(&gmt_tm);
    pLocalTime->tick = pUtc->tick + (local - gmt) * VITA_CLOCKS_PER_SEC;
    return 0;
}

EXPORT(int, _sceRtcFormatRFC2822) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC2822LocalTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC3339) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC3339LocalTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentAdNetworkTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentClock, SceDateTime *datePtr, int iTimeZone) {
    if (datePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    uint64_t tick = rtc_get_ticks(host.kernel.base_tick.tick) + iTimeZone * 60 * 60 * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, tick);

    return 0;
}

EXPORT(int, _sceRtcGetCurrentClockLocalTime, SceDateTime *datePtr) {
    if (datePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    std::time_t t = std::time(nullptr);

    tm local_tm = {};
    tm gmt_tm = {};

    SAFE_LOCALTIME(&t, &local_tm);
    SAFE_GMTIME(&t, &gmt_tm);

    std::time_t local = std::mktime(&local_tm);
    std::time_t gmt = std::mktime(&gmt_tm);
    uint64_t tick = rtc_get_ticks(host.kernel.base_tick.tick) + (local - gmt) * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, tick);
    return 0;
}

EXPORT(int, _sceRtcGetCurrentDebugNetworkTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentGpsTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentNetworkTick, SceRtcTick *tick) {
    if (tick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    tick->tick = rtc_get_ticks(host.kernel.base_tick.tick);

    return 0;
}

EXPORT(int, _sceRtcGetCurrentRetainedNetworkTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentTick, SceRtcTick *tick) {
    if (tick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    tick->tick = rtc_get_ticks(host.kernel.base_tick.tick);

    return 0;
}

EXPORT(int, _sceRtcGetLastAdjustedTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetLastReincarnatedTick) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRtcGetAccumulativeTime) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceRtcConvertLocalTimeToUtc)
BRIDGE_IMPL(_sceRtcConvertUtcToLocalTime)
BRIDGE_IMPL(_sceRtcFormatRFC2822)
BRIDGE_IMPL(_sceRtcFormatRFC2822LocalTime)
BRIDGE_IMPL(_sceRtcFormatRFC3339)
BRIDGE_IMPL(_sceRtcFormatRFC3339LocalTime)
BRIDGE_IMPL(_sceRtcGetCurrentAdNetworkTick)
BRIDGE_IMPL(_sceRtcGetCurrentClock)
BRIDGE_IMPL(_sceRtcGetCurrentClockLocalTime)
BRIDGE_IMPL(_sceRtcGetCurrentDebugNetworkTick)
BRIDGE_IMPL(_sceRtcGetCurrentGpsTick)
BRIDGE_IMPL(_sceRtcGetCurrentNetworkTick)
BRIDGE_IMPL(_sceRtcGetCurrentRetainedNetworkTick)
BRIDGE_IMPL(_sceRtcGetCurrentTick)
BRIDGE_IMPL(_sceRtcGetLastAdjustedTick)
BRIDGE_IMPL(_sceRtcGetLastReincarnatedTick)
BRIDGE_IMPL(sceRtcGetAccumulativeTime)
