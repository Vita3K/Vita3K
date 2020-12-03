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

BRIDGE_DECL(sceMtpBeginCheckSameObjectExist)
BRIDGE_DECL(sceMtpBeginCreateHostDir)
BRIDGE_DECL(sceMtpBeginCreateHostFile)
BRIDGE_DECL(sceMtpBeginDeleteHostDir)
BRIDGE_DECL(sceMtpBeginDeleteObject)
BRIDGE_DECL(sceMtpBeginExportObjectWithCheck)
BRIDGE_DECL(sceMtpBeginGetHostStorageSize)
BRIDGE_DECL(sceMtpBeginGetNpAccountInfo)
BRIDGE_DECL(sceMtpBeginGetNumOfObject)
BRIDGE_DECL(sceMtpBeginGetObjectMetadata)
BRIDGE_DECL(sceMtpBeginGetObjectStatus2)
BRIDGE_DECL(sceMtpBeginGetObjectThumbnail)
BRIDGE_DECL(sceMtpBeginGetSystemSetting)
BRIDGE_DECL(sceMtpBeginGetTotalObjectSize)
BRIDGE_DECL(sceMtpBeginHandover)
BRIDGE_DECL(sceMtpBeginHttpGetDataWithUrl)
BRIDGE_DECL(sceMtpBeginHttpGetPropertyWithUrl)
BRIDGE_DECL(sceMtpBeginImportObject)
BRIDGE_DECL(sceMtpBeginMoveHostDir)
BRIDGE_DECL(sceMtpBeginNpDrmActivate)
BRIDGE_DECL(sceMtpBeginNpDrmDeactivate)
BRIDGE_DECL(sceMtpBeginNpDrmGetLicense)
BRIDGE_DECL(sceMtpBeginNpDrmGetRtc)
BRIDGE_DECL(sceMtpBeginReadObjectWithOffset)
BRIDGE_DECL(sceMtpBeginResumeExportObject)
BRIDGE_DECL(sceMtpBeginResumeImportObject)
BRIDGE_DECL(sceMtpBeginRpcNetOperationRecv)
BRIDGE_DECL(sceMtpBeginRpcNetOperationSend)
BRIDGE_DECL(sceMtpBeginSearchObject)
BRIDGE_DECL(sceMtpBeginSetSystemSetting)
BRIDGE_DECL(sceMtpBeginSpecifiedObjectMetadata)
BRIDGE_DECL(sceMtpBeginWriteObjectWithOffset)
BRIDGE_DECL(sceMtpCheckContextValid)
BRIDGE_DECL(sceMtpEnd)
BRIDGE_DECL(sceMtpEndImportObject)
BRIDGE_DECL(sceMtpGetBgTaskInfo)
BRIDGE_DECL(sceMtpGetCurrentInterface)
BRIDGE_DECL(sceMtpGetHostInfo)
BRIDGE_DECL(sceMtpInit)
BRIDGE_DECL(sceMtpPauseExportObject)
BRIDGE_DECL(sceMtpReset)
BRIDGE_DECL(sceMtpResume)
BRIDGE_DECL(sceMtpSetFinishCallback)
BRIDGE_DECL(sceMtpStart)
BRIDGE_DECL(sceMtpStop)
BRIDGE_DECL(sceMtpSuspend)
BRIDGE_DECL(sceMtpTerminateExecution)
