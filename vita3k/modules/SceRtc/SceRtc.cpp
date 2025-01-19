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

#include "SceRtc.h"

#include <kernel/state.h>
#include <rtc/rtc.h>
#include <util/tracy.h>

#include <util/safe_time.h>

#include <chrono>
#include <ctime>

TRACY_MODULE_NAME(SceRtc);

EXPORT(int, _sceRtcConvertLocalTimeToUtc, const SceRtcTick *pLocalTime, SceRtcTick *pUtc) {
    TRACY_FUNC(_sceRtcConvertLocalTimeToUtc, pLocalTime, pUtc);
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
    TRACY_FUNC(_sceRtcConvertUtcToLocalTime, pUtc, pLocalTime);
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
    TRACY_FUNC(_sceRtcFormatRFC2822);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC2822LocalTime) {
    TRACY_FUNC(_sceRtcFormatRFC2822LocalTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC3339) {
    TRACY_FUNC(_sceRtcFormatRFC3339);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcFormatRFC3339LocalTime) {
    TRACY_FUNC(_sceRtcFormatRFC3339LocalTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentAdNetworkTick) {
    TRACY_FUNC(_sceRtcGetCurrentAdNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentClock, SceDateTime *datePtr, int iTimeZone) {
    TRACY_FUNC(_sceRtcGetCurrentClock, datePtr, iTimeZone);
    if (datePtr == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    uint64_t tick = rtc_get_ticks(emuenv.kernel.base_tick.tick) + iTimeZone * 60 * 60 * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, tick);

    return 0;
}

EXPORT(int, _sceRtcGetCurrentClockLocalTime, SceDateTime *datePtr) {
    TRACY_FUNC(_sceRtcGetCurrentClockLocalTime, datePtr);
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
    uint64_t tick = rtc_get_ticks(emuenv.kernel.base_tick.tick) + (local - gmt) * VITA_CLOCKS_PER_SEC;
    __RtcTicksToPspTime(datePtr, tick);
    return 0;
}

EXPORT(int, _sceRtcGetCurrentDebugNetworkTick) {
    TRACY_FUNC(_sceRtcGetCurrentDebugNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentGpsTick) {
    TRACY_FUNC(_sceRtcGetCurrentGpsTick);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentNetworkTick, SceRtcTick *tick) {
    TRACY_FUNC(_sceRtcGetCurrentNetworkTick, tick);
    if (tick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    tick->tick = rtc_get_ticks(emuenv.kernel.base_tick.tick);

    return 0;
}

EXPORT(int, _sceRtcGetCurrentRetainedNetworkTick) {
    TRACY_FUNC(_sceRtcGetCurrentRetainedNetworkTick);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetCurrentTick, SceRtcTick *tick) {
    TRACY_FUNC(_sceRtcGetCurrentTick, tick);
    if (tick == nullptr) {
        return RET_ERROR(SCE_RTC_ERROR_INVALID_POINTER);
    }

    tick->tick = rtc_get_ticks(emuenv.kernel.base_tick.tick);

    return 0;
}

EXPORT(int, _sceRtcGetLastAdjustedTick) {
    TRACY_FUNC(_sceRtcGetLastAdjustedTick);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRtcGetLastReincarnatedTick) {
    TRACY_FUNC(_sceRtcGetLastReincarnatedTick);
    return UNIMPLEMENTED();
}

EXPORT(SceULong64, sceRtcGetAccumulativeTime) {
    TRACY_FUNC(sceRtcGetAccumulativeTime);
    STUBBED("sceRtcGetAccumulativeTime");
    return rtc_get_ticks(emuenv.kernel.base_tick.tick);
}
