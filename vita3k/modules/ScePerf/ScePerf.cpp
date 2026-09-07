// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <chrono>

// Rate at which the timebase counter advances.
//
// On hardware scePerfGetTimebaseFrequency is a tail-call into ScePamgr's
// sceKernelPaGetTimebaseFrequency, which returns 333 (movw r0, #0x14d in
// os0/kd/pamgr.elf on prototype firmware 1.691.011). The unit is not stated
// anywhere and 333 Hz would be far too coarse for the counter it describes,
// so this reads it as MHz, which lines up with the Vita's 333 MHz nominal
// CPU clock. That reading is a hypothesis, not a measured fact.
//
// Retail firmware ships no ScePamgr at all, so on a real console the
// frequency call lands on an unresolved import stub and returns -1 while the
// value call keeps working; callers that need the rate there have to measure
// it. Reporting a usable frequency here is a deliberate divergence, since
// reproducing that gap would only break software the emulator can serve.
static constexpr uint64_t VITA_PERF_TIMEBASE_HZ = 333000000;

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

EXPORT(SceUInt32, scePerfGetTimebaseFrequency) {
    return static_cast<SceUInt32>(VITA_PERF_TIMEBASE_HZ);
}

EXPORT(SceUInt64, scePerfGetTimebaseValue) {
    // Hardware exposes a free-running 64-bit counter; anything reading it
    // needs monotonicity above all, so this counts from the first call
    // rather than from a wall clock that can step.
    static const auto origin = std::chrono::steady_clock::now();
    const uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - origin)
                            .count();

    // Split rather than (ns * hz) / 1e9: at this frequency the product
    // overflows 64 bits after a few minutes, which would wrap the counter
    // backwards mid-session.
    return (ns / 1000000000ull) * VITA_PERF_TIMEBASE_HZ
        + (ns % 1000000000ull) * VITA_PERF_TIMEBASE_HZ / 1000000000ull;
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
