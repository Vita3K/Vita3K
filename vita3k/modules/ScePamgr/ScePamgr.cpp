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

#include "ScePamgr.h"

EXPORT(int, _sceKernelPaAddArmTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPaAddBusTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPaAddCounterTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPaAddGpuTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPaGetGpuSampledData) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPaSetupTraceBufferByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetIoBaseAddress) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetSharedKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetTimebaseFrequency) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetTimebaseValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetTraceBufferSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetTraceBufferStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaGetWritePointer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaInsertBookmark) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRegister) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRegisterShared) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRemoveArmTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRemoveBusTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRemoveCounterTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaRemoveGpuTraceByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaSetBookmarkChannelEnableByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaStartByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaStopByKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPaUnregister) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonSelectEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonSetCounterValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPerfArmPmonStop) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceKernelPaAddArmTraceByKey)
BRIDGE_IMPL(_sceKernelPaAddBusTraceByKey)
BRIDGE_IMPL(_sceKernelPaAddCounterTraceByKey)
BRIDGE_IMPL(_sceKernelPaAddGpuTraceByKey)
BRIDGE_IMPL(_sceKernelPaGetGpuSampledData)
BRIDGE_IMPL(_sceKernelPaSetupTraceBufferByKey)
BRIDGE_IMPL(sceKernelPaGetIoBaseAddress)
BRIDGE_IMPL(sceKernelPaGetSharedKey)
BRIDGE_IMPL(sceKernelPaGetTimebaseFrequency)
BRIDGE_IMPL(sceKernelPaGetTimebaseValue)
BRIDGE_IMPL(sceKernelPaGetTraceBufferSize)
BRIDGE_IMPL(sceKernelPaGetTraceBufferStatus)
BRIDGE_IMPL(sceKernelPaGetWritePointer)
BRIDGE_IMPL(sceKernelPaInsertBookmark)
BRIDGE_IMPL(sceKernelPaRegister)
BRIDGE_IMPL(sceKernelPaRegisterShared)
BRIDGE_IMPL(sceKernelPaRemoveArmTraceByKey)
BRIDGE_IMPL(sceKernelPaRemoveBusTraceByKey)
BRIDGE_IMPL(sceKernelPaRemoveCounterTraceByKey)
BRIDGE_IMPL(sceKernelPaRemoveGpuTraceByKey)
BRIDGE_IMPL(sceKernelPaSetBookmarkChannelEnableByKey)
BRIDGE_IMPL(sceKernelPaStartByKey)
BRIDGE_IMPL(sceKernelPaStopByKey)
BRIDGE_IMPL(sceKernelPaUnregister)
BRIDGE_IMPL(sceKernelPerfArmPmonClose)
BRIDGE_IMPL(sceKernelPerfArmPmonOpen)
BRIDGE_IMPL(sceKernelPerfArmPmonReset)
BRIDGE_IMPL(sceKernelPerfArmPmonSelectEvent)
BRIDGE_IMPL(sceKernelPerfArmPmonSetCounterValue)
BRIDGE_IMPL(sceKernelPerfArmPmonStart)
BRIDGE_IMPL(sceKernelPerfArmPmonStop)
