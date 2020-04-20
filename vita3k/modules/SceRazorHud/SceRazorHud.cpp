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

#include "SceRazorHud.h"

EXPORT(int, sceRazorCpuHudSetUserMarkerTraceBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuHudUserMarkerEnableAllThreads) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorCpuHudUserMarkerEnableThreadId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuLiveSetBuffer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuLiveSetMetricsGroup) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuLiveStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuLiveStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuLiveTriggerDebugDump) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuPerfGetMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuTraceRingBufferSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuTraceRingBufferUsed) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuTraceSetFilename) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuTraceSetMetricsGroup) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorGpuTraceTrigger) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorHudReloadSettings) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorHudSetDisplayEnabled) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRazorHudSetDisplayFrameCount) {
    return UNIMPLEMENTED();
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
