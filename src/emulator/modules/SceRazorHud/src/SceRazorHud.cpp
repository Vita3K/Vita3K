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

#include <SceRazorHud/exports.h>

EXPORT(int, sceRazorCpuHudSetUserMarkerTraceBuffer) {
    return unimplemented("sceRazorCpuHudSetUserMarkerTraceBuffer");
}

EXPORT(int, sceRazorCpuHudUserMarkerEnableAllThreads) {
    return unimplemented("sceRazorCpuHudUserMarkerEnableAllThreads");
}

EXPORT(int, sceRazorCpuHudUserMarkerEnableThreadId) {
    return unimplemented("sceRazorCpuHudUserMarkerEnableThreadId");
}

EXPORT(int, sceRazorGpuLiveSetBuffer) {
    return unimplemented("sceRazorGpuLiveSetBuffer");
}

EXPORT(int, sceRazorGpuLiveSetMetricsGroup) {
    return unimplemented("sceRazorGpuLiveSetMetricsGroup");
}

EXPORT(int, sceRazorGpuLiveStart) {
    return unimplemented("sceRazorGpuLiveStart");
}

EXPORT(int, sceRazorGpuLiveStop) {
    return unimplemented("sceRazorGpuLiveStop");
}

EXPORT(int, sceRazorGpuLiveTriggerDebugDump) {
    return unimplemented("sceRazorGpuLiveTriggerDebugDump");
}

EXPORT(int, sceRazorGpuPerfGetMode) {
    return unimplemented("sceRazorGpuPerfGetMode");
}

EXPORT(int, sceRazorGpuTraceRingBufferSize) {
    return unimplemented("sceRazorGpuTraceRingBufferSize");
}

EXPORT(int, sceRazorGpuTraceRingBufferUsed) {
    return unimplemented("sceRazorGpuTraceRingBufferUsed");
}

EXPORT(int, sceRazorGpuTraceSetFilename) {
    return unimplemented("sceRazorGpuTraceSetFilename");
}

EXPORT(int, sceRazorGpuTraceSetMetricsGroup) {
    return unimplemented("sceRazorGpuTraceSetMetricsGroup");
}

EXPORT(int, sceRazorGpuTraceTrigger) {
    return unimplemented("sceRazorGpuTraceTrigger");
}

EXPORT(int, sceRazorHudReloadSettings) {
    return unimplemented("sceRazorHudReloadSettings");
}

EXPORT(int, sceRazorHudSetDisplayEnabled) {
    return unimplemented("sceRazorHudSetDisplayEnabled");
}

EXPORT(int, sceRazorHudSetDisplayFrameCount) {
    return unimplemented("sceRazorHudSetDisplayFrameCount");
}

BRIDGE_IMPL(sceRazorCpuHudSetUserMarkerTraceBuffer)
BRIDGE_IMPL(sceRazorCpuHudUserMarkerEnableAllThreads)
BRIDGE_IMPL(sceRazorCpuHudUserMarkerEnableThreadId)
BRIDGE_IMPL(sceRazorGpuLiveSetBuffer)
BRIDGE_IMPL(sceRazorGpuLiveSetMetricsGroup)
BRIDGE_IMPL(sceRazorGpuLiveStart)
BRIDGE_IMPL(sceRazorGpuLiveStop)
BRIDGE_IMPL(sceRazorGpuLiveTriggerDebugDump)
BRIDGE_IMPL(sceRazorGpuPerfGetMode)
BRIDGE_IMPL(sceRazorGpuTraceRingBufferSize)
BRIDGE_IMPL(sceRazorGpuTraceRingBufferUsed)
BRIDGE_IMPL(sceRazorGpuTraceSetFilename)
BRIDGE_IMPL(sceRazorGpuTraceSetMetricsGroup)
BRIDGE_IMPL(sceRazorGpuTraceTrigger)
BRIDGE_IMPL(sceRazorHudReloadSettings)
BRIDGE_IMPL(sceRazorHudSetDisplayEnabled)
BRIDGE_IMPL(sceRazorHudSetDisplayFrameCount)
