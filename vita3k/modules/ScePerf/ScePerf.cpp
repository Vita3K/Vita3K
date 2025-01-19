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

#include <module/module.h>

VAR_EXPORT(_pLibPerfCaptureFlagPtr) {
    auto ptr = Ptr<uint32_t>(alloc(emuenv.mem, 4, "_pLibPerfCaptureFlagPtr"));
    auto flag = Ptr<uint32_t>(alloc(emuenv.mem, 4, "_pLibPerfCaptureFlag"));
    *ptr.get(emuenv.mem) = flag.address();
    *flag.get(emuenv.mem) = 0;
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
