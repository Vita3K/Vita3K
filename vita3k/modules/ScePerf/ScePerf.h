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

VAR_BRIDGE_DECL(_pLibPerfCaptureFlagPtr)
BRIDGE_DECL(_sceCpuRazorPopFiberUserMarker)
BRIDGE_DECL(_sceCpuRazorPushFiberUserMarker)
BRIDGE_DECL(_sceRazorCpuInit)
BRIDGE_DECL(_sceRazorCpuWriteFiberUltPkt)
BRIDGE_DECL(scePerfArmPmonGetCounterValue)
BRIDGE_DECL(scePerfArmPmonReset)
BRIDGE_DECL(scePerfArmPmonSelectEvent)
BRIDGE_DECL(scePerfArmPmonSetCounterValue)
BRIDGE_DECL(scePerfArmPmonSoftwareIncrement)
BRIDGE_DECL(scePerfArmPmonStart)
BRIDGE_DECL(scePerfArmPmonStop)
BRIDGE_DECL(scePerfGetTimebaseFrequency)
BRIDGE_DECL(scePerfGetTimebaseValue)
BRIDGE_DECL(sceRazorCpuGetActivityMonitorTraceBuffer)
BRIDGE_DECL(sceRazorCpuGetUserMarkerTraceBuffer)
BRIDGE_DECL(sceRazorCpuIsCapturing)
BRIDGE_DECL(sceRazorCpuPopMarker)
BRIDGE_DECL(sceRazorCpuPushMarker)
BRIDGE_DECL(sceRazorCpuPushMarkerWithHud)
BRIDGE_DECL(sceRazorCpuStartActivityMonitor)
BRIDGE_DECL(sceRazorCpuStartCapture)
BRIDGE_DECL(sceRazorCpuStartUserMarkerTrace)
BRIDGE_DECL(sceRazorCpuStopActivityMonitor)
BRIDGE_DECL(sceRazorCpuStopCapture)
BRIDGE_DECL(sceRazorCpuStopUserMarkerTrace)
BRIDGE_DECL(sceRazorCpuSync)
