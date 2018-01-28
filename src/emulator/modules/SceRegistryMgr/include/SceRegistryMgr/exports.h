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

// SceRegMgr
BRIDGE_DECL(sceRegMgrAddRegistryCallback)
BRIDGE_DECL(sceRegMgrDbBackup)
BRIDGE_DECL(sceRegMgrDbRestore)
BRIDGE_DECL(sceRegMgrGetInitVals)
BRIDGE_DECL(sceRegMgrGetKeyBin)
BRIDGE_DECL(sceRegMgrGetKeyInt)
BRIDGE_DECL(sceRegMgrGetKeyStr)
BRIDGE_DECL(sceRegMgrGetKeys)
BRIDGE_DECL(sceRegMgrGetKeysInfo)
BRIDGE_DECL(sceRegMgrGetRegVersion)
BRIDGE_DECL(sceRegMgrIsBlueScreen)
BRIDGE_DECL(sceRegMgrRegisterCallback)
BRIDGE_DECL(sceRegMgrRegisterDrvErrCallback)
BRIDGE_DECL(sceRegMgrResetRegistryLv)
BRIDGE_DECL(sceRegMgrSetKeyBin)
BRIDGE_DECL(sceRegMgrSetKeyInt)
BRIDGE_DECL(sceRegMgrSetKeyStr)
BRIDGE_DECL(sceRegMgrSetKeys)
BRIDGE_DECL(sceRegMgrStartCallback)
BRIDGE_DECL(sceRegMgrStopCallback)
BRIDGE_DECL(sceRegMgrUnregisterCallback)
BRIDGE_DECL(sceRegMgrUnregisterDrvErrCallback)

// SceRegMgrForGame
BRIDGE_DECL(sceRegMgrSystemParamGetBin)
BRIDGE_DECL(sceRegMgrSystemParamGetInt)
BRIDGE_DECL(sceRegMgrSystemParamGetStr)
BRIDGE_DECL(sceRegMgrSystemParamSetBin)
BRIDGE_DECL(sceRegMgrSystemParamSetInt)
BRIDGE_DECL(sceRegMgrSystemParamSetStr)

// SceRegMgrForSDK
BRIDGE_DECL(sceRegMgrUtilityGetBin)
BRIDGE_DECL(sceRegMgrUtilityGetInt)
BRIDGE_DECL(sceRegMgrUtilityGetStr)
BRIDGE_DECL(sceRegMgrUtilitySetBin)
BRIDGE_DECL(sceRegMgrUtilitySetInt)
BRIDGE_DECL(sceRegMgrUtilitySetStr)

// SceRegMgrService
BRIDGE_DECL(sceRegMgrSrvCnvRegionInt)
BRIDGE_DECL(sceRegMgrSrvCnvRegionPsCode)
BRIDGE_DECL(sceRegMgrSrvCnvRegionStr)
BRIDGE_DECL(sceRegMgrSrvGetRegion)
BRIDGE_DECL(sceRegMgrSrvGetRegionStr)
