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
    return unimplemented(export_name);
}

EXPORT(int, _sceCpuRazorPopFiberUserMarker) {
    return unimplemented(export_name);
}

EXPORT(int, _sceCpuRazorPushFiberUserMarker) {
    return unimplemented(export_name);
}

EXPORT(int, _sceRazorCpuInit) {
    return unimplemented(export_name);
}

EXPORT(int, _sceRazorCpuWriteFiberUltPkt) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonGetCounterValue) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonReset) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonSelectEvent) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonSetCounterValue) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonSoftwareIncrement) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonStart) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfArmPmonStop) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfGetTimebaseFrequency) {
    return unimplemented(export_name);
}

EXPORT(int, scePerfGetTimebaseValue) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuGetActivityMonitorTraceBuffer) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuGetUserMarkerTraceBuffer) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuIsCapturing) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuPopMarker) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuPushMarker) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuPushMarkerWithHud) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStartActivityMonitor) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStartCapture) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStartUserMarkerTrace) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStopActivityMonitor) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStopCapture) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuStopUserMarkerTrace) {
    return unimplemented(export_name);
}

EXPORT(int, sceRazorCpuSync) {
    return unimplemented(export_name);
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
