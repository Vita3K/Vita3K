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

#include "ScePerf.h"

EXPORT(int, _pLibPerfCaptureFlagPtr) {
    return unimplemented("_pLibPerfCaptureFlagPtr");
}

EXPORT(int, _sceCpuRazorPopFiberUserMarker) {
    return unimplemented("_sceCpuRazorPopFiberUserMarker");
}

EXPORT(int, _sceCpuRazorPushFiberUserMarker) {
    return unimplemented("_sceCpuRazorPushFiberUserMarker");
}

EXPORT(int, _sceRazorCpuInit) {
    return unimplemented("_sceRazorCpuInit");
}

EXPORT(int, _sceRazorCpuWriteFiberUltPkt) {
    return unimplemented("_sceRazorCpuWriteFiberUltPkt");
}

EXPORT(int, scePerfArmPmonGetCounterValue) {
    return unimplemented("scePerfArmPmonGetCounterValue");
}

EXPORT(int, scePerfArmPmonReset) {
    return unimplemented("scePerfArmPmonReset");
}

EXPORT(int, scePerfArmPmonSelectEvent) {
    return unimplemented("scePerfArmPmonSelectEvent");
}

EXPORT(int, scePerfArmPmonSetCounterValue) {
    return unimplemented("scePerfArmPmonSetCounterValue");
}

EXPORT(int, scePerfArmPmonSoftwareIncrement) {
    return unimplemented("scePerfArmPmonSoftwareIncrement");
}

EXPORT(int, scePerfArmPmonStart) {
    return unimplemented("scePerfArmPmonStart");
}

EXPORT(int, scePerfArmPmonStop) {
    return unimplemented("scePerfArmPmonStop");
}

EXPORT(int, scePerfGetTimebaseFrequency) {
    return unimplemented("scePerfGetTimebaseFrequency");
}

EXPORT(int, scePerfGetTimebaseValue) {
    return unimplemented("scePerfGetTimebaseValue");
}

EXPORT(int, sceRazorCpuGetActivityMonitorTraceBuffer) {
    return unimplemented("sceRazorCpuGetActivityMonitorTraceBuffer");
}

EXPORT(int, sceRazorCpuGetUserMarkerTraceBuffer) {
    return unimplemented("sceRazorCpuGetUserMarkerTraceBuffer");
}

EXPORT(int, sceRazorCpuIsCapturing) {
    return unimplemented("sceRazorCpuIsCapturing");
}

EXPORT(int, sceRazorCpuPopMarker) {
    return unimplemented("sceRazorCpuPopMarker");
}

EXPORT(int, sceRazorCpuPushMarker) {
    return unimplemented("sceRazorCpuPushMarker");
}

EXPORT(int, sceRazorCpuPushMarkerWithHud) {
    return unimplemented("sceRazorCpuPushMarkerWithHud");
}

EXPORT(int, sceRazorCpuStartActivityMonitor) {
    return unimplemented("sceRazorCpuStartActivityMonitor");
}

EXPORT(int, sceRazorCpuStartCapture) {
    return unimplemented("sceRazorCpuStartCapture");
}

EXPORT(int, sceRazorCpuStartUserMarkerTrace) {
    return unimplemented("sceRazorCpuStartUserMarkerTrace");
}

EXPORT(int, sceRazorCpuStopActivityMonitor) {
    return unimplemented("sceRazorCpuStopActivityMonitor");
}

EXPORT(int, sceRazorCpuStopCapture) {
    return unimplemented("sceRazorCpuStopCapture");
}

EXPORT(int, sceRazorCpuStopUserMarkerTrace) {
    return unimplemented("sceRazorCpuStopUserMarkerTrace");
}

EXPORT(int, sceRazorCpuSync) {
    return unimplemented("sceRazorCpuSync");
}

BRIDGE_IMPL(_pLibPerfCaptureFlagPtr)
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
