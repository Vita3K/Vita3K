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

#include "SceCodecEngineWrapper.h"

EXPORT(int, _sceCodecEngineAllocMemoryFromUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineChangeNumWorkerCores) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineChangeNumWorkerCoresDefault) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineChangeNumWorkerCoresMax) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineCloseUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineFreeMemoryFromUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineGetMemoryState) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineGetNumRpcCalled) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineGetProcessorLoad) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineOpenUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEnginePmonGetProcessorLoad) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEnginePmonReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEnginePmonStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEnginePmonStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineResetNumRpcCalled) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceCodecEngineSetClockFrequency) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceCodecEngineAllocMemoryFromUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineChangeNumWorkerCores)
BRIDGE_IMPL(_sceCodecEngineChangeNumWorkerCoresDefault)
BRIDGE_IMPL(_sceCodecEngineChangeNumWorkerCoresMax)
BRIDGE_IMPL(_sceCodecEngineCloseUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineFreeMemoryFromUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineGetMemoryState)
BRIDGE_IMPL(_sceCodecEngineGetNumRpcCalled)
BRIDGE_IMPL(_sceCodecEngineGetProcessorLoad)
BRIDGE_IMPL(_sceCodecEngineOpenUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEnginePmonGetProcessorLoad)
BRIDGE_IMPL(_sceCodecEnginePmonReset)
BRIDGE_IMPL(_sceCodecEnginePmonStart)
BRIDGE_IMPL(_sceCodecEnginePmonStop)
BRIDGE_IMPL(_sceCodecEngineResetNumRpcCalled)
BRIDGE_IMPL(_sceCodecEngineSetClockFrequency)
