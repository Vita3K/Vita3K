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

#include "SceLibMtp.h"

EXPORT(int, sceMtpBeginCheckSameObjectExist) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginCreateHostDir) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginCreateHostFile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginDeleteHostDir) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginDeleteObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginExportObjectWithCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetHostStorageSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetNpAccountInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetNumOfObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetObjectMetadata) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetObjectStatus2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetObjectThumbnail) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetSystemSetting) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginGetTotalObjectSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginHandover) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginHttpGetDataWithUrl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginHttpGetPropertyWithUrl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginImportObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginMoveHostDir) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginNpDrmActivate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginNpDrmDeactivate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginNpDrmGetLicense) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginNpDrmGetRtc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginReadObjectWithOffset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginResumeExportObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginResumeImportObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginRpcNetOperationRecv) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginRpcNetOperationSend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginSearchObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginSetSystemSetting) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginSpecifiedObjectMetadata) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpBeginWriteObjectWithOffset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpCheckContextValid) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpEnd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpEndImportObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpGetBgTaskInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpGetCurrentInterface) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpGetHostInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpPauseExportObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpResume) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpSetFinishCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpSuspend) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpTerminateExecution) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceMtpBeginCheckSameObjectExist)
BRIDGE_IMPL(sceMtpBeginCreateHostDir)
BRIDGE_IMPL(sceMtpBeginCreateHostFile)
BRIDGE_IMPL(sceMtpBeginDeleteHostDir)
BRIDGE_IMPL(sceMtpBeginDeleteObject)
BRIDGE_IMPL(sceMtpBeginExportObjectWithCheck)
BRIDGE_IMPL(sceMtpBeginGetHostStorageSize)
BRIDGE_IMPL(sceMtpBeginGetNpAccountInfo)
BRIDGE_IMPL(sceMtpBeginGetNumOfObject)
BRIDGE_IMPL(sceMtpBeginGetObjectMetadata)
BRIDGE_IMPL(sceMtpBeginGetObjectStatus2)
BRIDGE_IMPL(sceMtpBeginGetObjectThumbnail)
BRIDGE_IMPL(sceMtpBeginGetSystemSetting)
BRIDGE_IMPL(sceMtpBeginGetTotalObjectSize)
BRIDGE_IMPL(sceMtpBeginHandover)
BRIDGE_IMPL(sceMtpBeginHttpGetDataWithUrl)
BRIDGE_IMPL(sceMtpBeginHttpGetPropertyWithUrl)
BRIDGE_IMPL(sceMtpBeginImportObject)
BRIDGE_IMPL(sceMtpBeginMoveHostDir)
BRIDGE_IMPL(sceMtpBeginNpDrmActivate)
BRIDGE_IMPL(sceMtpBeginNpDrmDeactivate)
BRIDGE_IMPL(sceMtpBeginNpDrmGetLicense)
BRIDGE_IMPL(sceMtpBeginNpDrmGetRtc)
BRIDGE_IMPL(sceMtpBeginReadObjectWithOffset)
BRIDGE_IMPL(sceMtpBeginResumeExportObject)
BRIDGE_IMPL(sceMtpBeginResumeImportObject)
BRIDGE_IMPL(sceMtpBeginRpcNetOperationRecv)
BRIDGE_IMPL(sceMtpBeginRpcNetOperationSend)
BRIDGE_IMPL(sceMtpBeginSearchObject)
BRIDGE_IMPL(sceMtpBeginSetSystemSetting)
BRIDGE_IMPL(sceMtpBeginSpecifiedObjectMetadata)
BRIDGE_IMPL(sceMtpBeginWriteObjectWithOffset)
BRIDGE_IMPL(sceMtpCheckContextValid)
BRIDGE_IMPL(sceMtpEnd)
BRIDGE_IMPL(sceMtpEndImportObject)
BRIDGE_IMPL(sceMtpGetBgTaskInfo)
BRIDGE_IMPL(sceMtpGetCurrentInterface)
BRIDGE_IMPL(sceMtpGetHostInfo)
BRIDGE_IMPL(sceMtpInit)
BRIDGE_IMPL(sceMtpPauseExportObject)
BRIDGE_IMPL(sceMtpReset)
BRIDGE_IMPL(sceMtpResume)
BRIDGE_IMPL(sceMtpSetFinishCallback)
BRIDGE_IMPL(sceMtpStart)
BRIDGE_IMPL(sceMtpStop)
BRIDGE_IMPL(sceMtpSuspend)
BRIDGE_IMPL(sceMtpTerminateExecution)
