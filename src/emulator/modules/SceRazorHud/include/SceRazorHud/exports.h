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

// SceRazorHud
BRIDGE_DECL(sceRazorCpuHudSetUserMarkerTraceBuffer)
BRIDGE_DECL(sceRazorCpuHudUserMarkerEnableAllThreads)
BRIDGE_DECL(sceRazorCpuHudUserMarkerEnableThreadId)
BRIDGE_DECL(sceRazorGpuLiveSetBuffer)
BRIDGE_DECL(sceRazorGpuLiveSetMetricsGroup)
BRIDGE_DECL(sceRazorGpuLiveStart)
BRIDGE_DECL(sceRazorGpuLiveStop)
BRIDGE_DECL(sceRazorGpuLiveTriggerDebugDump)
BRIDGE_DECL(sceRazorGpuPerfGetMode)
BRIDGE_DECL(sceRazorGpuTraceRingBufferSize)
BRIDGE_DECL(sceRazorGpuTraceRingBufferUsed)
BRIDGE_DECL(sceRazorGpuTraceSetFilename)
BRIDGE_DECL(sceRazorGpuTraceSetMetricsGroup)
BRIDGE_DECL(sceRazorGpuTraceTrigger)
BRIDGE_DECL(sceRazorHudReloadSettings)
BRIDGE_DECL(sceRazorHudSetDisplayEnabled)
BRIDGE_DECL(sceRazorHudSetDisplayFrameCount)
