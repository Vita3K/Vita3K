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

BRIDGE_DECL(_sceRtcConvertLocalTimeToUtc)
BRIDGE_DECL(_sceRtcConvertUtcToLocalTime)
BRIDGE_DECL(_sceRtcFormatRFC2822)
BRIDGE_DECL(_sceRtcFormatRFC2822LocalTime)
BRIDGE_DECL(_sceRtcFormatRFC3339)
BRIDGE_DECL(_sceRtcFormatRFC3339LocalTime)
BRIDGE_DECL(_sceRtcGetCurrentAdNetworkTick)
BRIDGE_DECL(_sceRtcGetCurrentClock)
BRIDGE_DECL(_sceRtcGetCurrentClockLocalTime)
BRIDGE_DECL(_sceRtcGetCurrentDebugNetworkTick)
BRIDGE_DECL(_sceRtcGetCurrentGpsTick)
BRIDGE_DECL(_sceRtcGetCurrentNetworkTick)
BRIDGE_DECL(_sceRtcGetCurrentRetainedNetworkTick)
BRIDGE_DECL(_sceRtcGetCurrentTick)
BRIDGE_DECL(_sceRtcGetLastAdjustedTick)
BRIDGE_DECL(_sceRtcGetLastReincarnatedTick)
BRIDGE_DECL(sceRtcGetAccumulativeTime)
