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

// SceCompat
BRIDGE_DECL(sceCompatAllocCdramWithHole)
BRIDGE_DECL(sceCompatAvailableColorSpaceSetting)
BRIDGE_DECL(sceCompatCache)
BRIDGE_DECL(sceCompatCheckPocketStation)
BRIDGE_DECL(sceCompatDatRead)
BRIDGE_DECL(sceCompatFrameBufferInit)
BRIDGE_DECL(sceCompatGetCurrentSecureTick)
BRIDGE_DECL(sceCompatGetDevInf)
BRIDGE_DECL(sceCompatGetPeripheralState)
BRIDGE_DECL(sceCompatGetPrimaryHead)
BRIDGE_DECL(sceCompatGetPspSystemSoftwareVersion)
BRIDGE_DECL(sceCompatGetStatus)
BRIDGE_DECL(sceCompatGetTitleList)
BRIDGE_DECL(sceCompatGetUpdateState)
BRIDGE_DECL(sceCompatIdStorageLookup)
BRIDGE_DECL(sceCompatInitEx)
BRIDGE_DECL(sceCompatInterrupt)
BRIDGE_DECL(sceCompatIsPocketStationTitle)
BRIDGE_DECL(sceCompatLCDCSync)
BRIDGE_DECL(sceCompatReadShared32)
BRIDGE_DECL(sceCompatReturnValueEx)
BRIDGE_DECL(sceCompatSetDisplayConfig)
BRIDGE_DECL(sceCompatSetRif)
BRIDGE_DECL(sceCompatSetSuspendSema)
BRIDGE_DECL(sceCompatSetUpdateState)
BRIDGE_DECL(sceCompatStart)
BRIDGE_DECL(sceCompatStop)
BRIDGE_DECL(sceCompatSuspendResume)
BRIDGE_DECL(sceCompatUninit)
BRIDGE_DECL(sceCompatWaitAndGetRequest)
BRIDGE_DECL(sceCompatWaitIntr)
BRIDGE_DECL(sceCompatWaitSpecialRequest)
BRIDGE_DECL(sceCompatWriteShared32)
BRIDGE_DECL(sceCompatWriteSharedCtrl)
