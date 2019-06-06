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

BRIDGE_DECL(_sceKernelPaAddArmTraceByKey)
BRIDGE_DECL(_sceKernelPaAddBusTraceByKey)
BRIDGE_DECL(_sceKernelPaAddCounterTraceByKey)
BRIDGE_DECL(_sceKernelPaAddGpuTraceByKey)
BRIDGE_DECL(_sceKernelPaGetGpuSampledData)
BRIDGE_DECL(_sceKernelPaSetupTraceBufferByKey)
BRIDGE_DECL(sceKernelPaGetIoBaseAddress)
BRIDGE_DECL(sceKernelPaGetSharedKey)
BRIDGE_DECL(sceKernelPaGetTimebaseFrequency)
BRIDGE_DECL(sceKernelPaGetTimebaseValue)
BRIDGE_DECL(sceKernelPaGetTraceBufferSize)
BRIDGE_DECL(sceKernelPaGetTraceBufferStatus)
BRIDGE_DECL(sceKernelPaGetWritePointer)
BRIDGE_DECL(sceKernelPaInsertBookmark)
BRIDGE_DECL(sceKernelPaRegister)
BRIDGE_DECL(sceKernelPaRegisterShared)
BRIDGE_DECL(sceKernelPaRemoveArmTraceByKey)
BRIDGE_DECL(sceKernelPaRemoveBusTraceByKey)
BRIDGE_DECL(sceKernelPaRemoveCounterTraceByKey)
BRIDGE_DECL(sceKernelPaRemoveGpuTraceByKey)
BRIDGE_DECL(sceKernelPaSetBookmarkChannelEnableByKey)
BRIDGE_DECL(sceKernelPaStartByKey)
BRIDGE_DECL(sceKernelPaStopByKey)
BRIDGE_DECL(sceKernelPaUnregister)
BRIDGE_DECL(sceKernelPerfArmPmonClose)
BRIDGE_DECL(sceKernelPerfArmPmonOpen)
BRIDGE_DECL(sceKernelPerfArmPmonReset)
BRIDGE_DECL(sceKernelPerfArmPmonSelectEvent)
BRIDGE_DECL(sceKernelPerfArmPmonSetCounterValue)
BRIDGE_DECL(sceKernelPerfArmPmonStart)
BRIDGE_DECL(sceKernelPerfArmPmonStop)
