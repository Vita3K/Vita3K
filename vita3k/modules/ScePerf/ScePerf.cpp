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

#include "ScePerf.h"

VAR_EXPORT(_pLibPerfCaptureFlagPtr) {
    auto ptr = Ptr<uint32_t>(alloc(host.mem, 4, "_pLibPerfCaptureFlagPtr"));
    auto flag = Ptr<uint32_t>(alloc(host.mem, 4, "_pLibPerfCaptureFlag"));
    *ptr.get(host.mem) = flag.address();
    *flag.get(host.mem) = 0;
    return ptr.address();
}

EXPORT(int, _sceCpuRazorPopFiberUserMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCpuRazorPushFiberUserMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRazorCpuInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceRazorCpuWriteFiberUltPkt) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonGetCounterValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonSelectEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonSetCounterValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonSoftwareIncrement) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfArmPmonStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfGetTimebaseFrequency) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePerfGetTimebaseValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuGetActivityMonitorTraceBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuGetUserMarkerTraceBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuIsCapturing) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuPopMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuPushMarker) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuPushMarkerWithHud) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStartActivityMonitor) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStartCapture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStartUserMarkerTrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStopActivityMonitor) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStopCapture) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuStopUserMarkerTrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuSync) {
    return UNIMPLEMENTED();
}

VAR_BRIDGE_IMPL(_pLibPerfCaptureFlagPtr)
BRIDGE_IMPL(_sceCpuRazorPopFiberUserMarker)
BRIDGE_IMPL(_sceCpuRazorPushFiberUserMarker)
BRIDGE_IMPL(_sceRazorCpuInit)
BRIDGE_IMPL(_sceRazorCpuWriteFiberUltPkt)
BRIDGE_IMPL(scePerfArmPmonGetCounterValue)
BRIDGE_IMPL(scePerfArmPmonReset)
BRIDGE_IMPL(scePerfArmPmonSelectEvent)
BRIDGE_IMPL(scePerfArmPmonSetCounterValue)
BRIDGE_IMPL(scePerfArmPmonSoftwareIncrement)
BRIDGE_IMPL(scePerfArmPmonStart)
BRIDGE_IMPL(scePerfArmPmonStop)
BRIDGE_IMPL(scePerfGetTimebaseFrequency)
BRIDGE_IMPL(scePerfGetTimebaseValue)
BRIDGE_IMPL(sceRazorCpuGetActivityMonitorTraceBuffer)
BRIDGE_IMPL(sceRazorCpuGetUserMarkerTraceBuffer)
BRIDGE_IMPL(sceRazorCpuIsCapturing)
BRIDGE_IMPL(sceRazorCpuPopMarker)
BRIDGE_IMPL(sceRazorCpuPushMarker)
BRIDGE_IMPL(sceRazorCpuPushMarkerWithHud)
BRIDGE_IMPL(sceRazorCpuStartActivityMonitor)
BRIDGE_IMPL(sceRazorCpuStartCapture)
BRIDGE_IMPL(sceRazorCpuStartUserMarkerTrace)
BRIDGE_IMPL(sceRazorCpuStopActivityMonitor)
BRIDGE_IMPL(sceRazorCpuStopCapture)
BRIDGE_IMPL(sceRazorCpuStopUserMarkerTrace)
BRIDGE_IMPL(sceRazorCpuSync)
