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

#include "SceRazorCapture.h"

EXPORT(int, sceRazorCaptureIsInProgress) {
    return unimplemented("sceRazorCaptureIsInProgress");
}

EXPORT(int, sceRazorCaptureSetTrigger) {
    return unimplemented("sceRazorCaptureSetTrigger");
}

EXPORT(int, sceRazorCaptureSetTriggerNextFrame) {
    return unimplemented("sceRazorCaptureSetTriggerNextFrame");
}

EXPORT(int, sceRazorGpuCaptureEnableSalvage) {
    return unimplemented("sceRazorGpuCaptureEnableSalvage");
}

EXPORT(int, sceRazorGpuCaptureIsInProgress) {
    return unimplemented("sceRazorGpuCaptureIsInProgress");
}

EXPORT(int, sceRazorGpuCaptureSetCaptureAllMemory) {
    return unimplemented("sceRazorGpuCaptureSetCaptureAllMemory");
}

EXPORT(int, sceRazorGpuCaptureSetCaptureBeforeKick) {
    return unimplemented("sceRazorGpuCaptureSetCaptureBeforeKick");
}

EXPORT(int, sceRazorGpuCaptureSetTrigger) {
    return unimplemented("sceRazorGpuCaptureSetTrigger");
}

EXPORT(int, sceRazorGpuCaptureSetTriggerNextFrame) {
    return unimplemented("sceRazorGpuCaptureSetTriggerNextFrame");
}

EXPORT(int, sceRazorGpuCaptureStartSalvageMode) {
    return unimplemented("sceRazorGpuCaptureStartSalvageMode");
}

BRIDGE_IMPL(sceRazorCaptureIsInProgress)
BRIDGE_IMPL(sceRazorCaptureSetTrigger)
BRIDGE_IMPL(sceRazorCaptureSetTriggerNextFrame)
BRIDGE_IMPL(sceRazorGpuCaptureEnableSalvage)
BRIDGE_IMPL(sceRazorGpuCaptureIsInProgress)
BRIDGE_IMPL(sceRazorGpuCaptureSetCaptureAllMemory)
BRIDGE_IMPL(sceRazorGpuCaptureSetCaptureBeforeKick)
BRIDGE_IMPL(sceRazorGpuCaptureSetTrigger)
BRIDGE_IMPL(sceRazorGpuCaptureSetTriggerNextFrame)
BRIDGE_IMPL(sceRazorGpuCaptureStartSalvageMode)
