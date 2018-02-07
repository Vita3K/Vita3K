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

// SceNpDrm
BRIDGE_DECL(_sceNpDrmCheckActData)
BRIDGE_DECL(_sceNpDrmCheckDrmReset)
BRIDGE_DECL(_sceNpDrmGetFixedRifName)
BRIDGE_DECL(_sceNpDrmGetRifInfo)
BRIDGE_DECL(_sceNpDrmGetRifName)
BRIDGE_DECL(_sceNpDrmGetRifNameForInstall)
BRIDGE_DECL(_sceNpDrmPresetRifProvisionalFlag)
BRIDGE_DECL(_sceNpDrmRemoveActData)

// SceNpDrmPackage
BRIDGE_DECL(_sceNpDrmPackageCheck)
BRIDGE_DECL(_sceNpDrmPackageDecrypt)
BRIDGE_DECL(_sceNpDrmPackageInstallFinished)
BRIDGE_DECL(_sceNpDrmPackageInstallStarted)
BRIDGE_DECL(_sceNpDrmPackageTransform)
BRIDGE_DECL(_sceNpDrmPackageUninstallFinished)
BRIDGE_DECL(_sceNpDrmPackageUninstallStarted)
BRIDGE_DECL(_sceNpDrmSaveDataFormatFinished)
BRIDGE_DECL(_sceNpDrmSaveDataFormatStarted)
BRIDGE_DECL(_sceNpDrmSaveDataInstallFinished)
BRIDGE_DECL(_sceNpDrmSaveDataInstallStarted)
BRIDGE_DECL(sceNpDrmPackageInstallOngoing)
BRIDGE_DECL(sceNpDrmPackageIsGameExist)
BRIDGE_DECL(sceNpDrmPackageUninstallOngoing)
BRIDGE_DECL(sceNpDrmSaveDataFormatOngoing)
BRIDGE_DECL(sceNpDrmSaveDataInstallOngoing)
