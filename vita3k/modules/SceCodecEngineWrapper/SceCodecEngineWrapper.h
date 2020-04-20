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

BRIDGE_DECL(_sceCodecEngineAllocMemoryFromUnmapMemBlock)
BRIDGE_DECL(_sceCodecEngineChangeNumWorkerCores)
BRIDGE_DECL(_sceCodecEngineChangeNumWorkerCoresDefault)
BRIDGE_DECL(_sceCodecEngineChangeNumWorkerCoresMax)
BRIDGE_DECL(_sceCodecEngineCloseUnmapMemBlock)
BRIDGE_DECL(_sceCodecEngineFreeMemoryFromUnmapMemBlock)
BRIDGE_DECL(_sceCodecEngineGetMemoryState)
BRIDGE_DECL(_sceCodecEngineGetNumRpcCalled)
BRIDGE_DECL(_sceCodecEngineGetProcessorLoad)
BRIDGE_DECL(_sceCodecEngineOpenUnmapMemBlock)
BRIDGE_DECL(_sceCodecEnginePmonGetProcessorLoad)
BRIDGE_DECL(_sceCodecEnginePmonReset)
BRIDGE_DECL(_sceCodecEnginePmonStart)
BRIDGE_DECL(_sceCodecEnginePmonStop)
BRIDGE_DECL(_sceCodecEngineResetNumRpcCalled)
BRIDGE_DECL(_sceCodecEngineSetClockFrequency)
