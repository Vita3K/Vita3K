// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

BRIDGE_DECL(ksceRtcConvertDateTimeToUnixTime)
BRIDGE_DECL(ksceRtcConvertLocalTimeToUtc)
BRIDGE_DECL(ksceRtcConvertTickToDateTime)
BRIDGE_DECL(ksceRtcConvertUtcToLocal)
BRIDGE_DECL(ksceRtcFormatRFC2822)
BRIDGE_DECL(ksceRtcFormatRFC2822LocalTime)
BRIDGE_DECL(ksceRtcFormatRFC3339)
BRIDGE_DECL(ksceRtcFormatRFC3339LocalTime)
BRIDGE_DECL(ksceRtcGetAccumulativeTime)
BRIDGE_DECL(ksceRtcGetAlarmTick)
BRIDGE_DECL(ksceRtcGetCurrentAdNetworkTick)
BRIDGE_DECL(ksceRtcGetCurrentClock)
BRIDGE_DECL(ksceRtcGetCurrentClockLocalTime)
BRIDGE_DECL(ksceRtcGetCurrentDebugNetworkTick)
BRIDGE_DECL(ksceRtcGetCurrentGpsTick)
BRIDGE_DECL(ksceRtcGetCurrentNetworkTick)
BRIDGE_DECL(ksceRtcGetCurrentRetainedNetworkTick)
BRIDGE_DECL(ksceRtcGetCurrentSecureTick)
BRIDGE_DECL(ksceRtcGetCurrentTick)
BRIDGE_DECL(ksceRtcGetLastAdjustedTick)
BRIDGE_DECL(ksceRtcGetLastReincarnatedTick)
BRIDGE_DECL(ksceRtcGetSecureAlarmTick)
BRIDGE_DECL(ksceRtcIsAlarmed)
BRIDGE_DECL(ksceRtcIsSecureAlarmed)
BRIDGE_DECL(ksceRtcRegisterCallback)
BRIDGE_DECL(ksceRtcRegisterSecureAlarmCallback)
BRIDGE_DECL(ksceRtcSetAlarmTick)
BRIDGE_DECL(ksceRtcSetConf)
BRIDGE_DECL(ksceRtcSetCurrentDebugNetworkTick)
BRIDGE_DECL(ksceRtcSetCurrentNetworkTick)
BRIDGE_DECL(ksceRtcSetCurrentSecureTick)
BRIDGE_DECL(ksceRtcSetCurrentTick)
BRIDGE_DECL(ksceRtcSetSecureAlarmTick)
BRIDGE_DECL(ksceRtcUnregisterCallback)
BRIDGE_DECL(ksceRtcUnregisterSecureAlarmCallback)
