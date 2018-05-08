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

#include "SceAppMgrUser.h"

#include <host/sfo.h>
#include <psp2/appmgr.h>

EXPORT(int, _sceAppMgrGetAppState, void *appState, uint32_t len, uint32_t version) {
    memset(appState, 0, len);
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAcidDirSet) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive3) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAddContAddMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAddContMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppDataMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppDataMountById) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppParamGetInt) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppParamGetString, int pid, int param, char *string, int length) {
    std::string res;
    if (!get_data(res, host.sfo_handle, param)) {
        return RET_ERROR(export_name, SCE_APPMGR_ERROR_INVALID);
    } else {
        res.copy(string, length);
        return 0;
    }
}

EXPORT(int, sceAppMgrAppParamSetString) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrAppUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrBgdlGetQueueStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrCaptureFrameBufDMACByAppId) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrCaptureFrameBufIFTUByAppId) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrCheckRifGD) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrContentInstallPeriodStart) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrContentInstallPeriodStop) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrConvertVs0UserDrivePath) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrDeclareShellProcess2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrDestroyAppByName) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrDrmClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrDrmOpen) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrForceUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGameDataMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetAppInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetAppMgrState) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetAppParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetAppParam2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetBootParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetBudgetInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetCoredumpStateForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetCurrentBgmState) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetCurrentBgmState2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetDevInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetFgAppInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetIdByName) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetMediaTypeFromDrive) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetMediaTypeFromDriveByPid) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetMountProcessNum) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetNameById) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetPfsDrive) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetPidListForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetRawPath) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByPidForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetRecommendedScreenOrientation) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetRunningAppIdListForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetSaveDataInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetSaveDataInfoForSpecialExport) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetStatusByAppId) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetStatusById) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetStatusByName) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetSystemDataFilePlayReady) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetUserDirPath) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetVs0UserDataDrive) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrGetVs0UserModuleDrive) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrInitSafeMemoryById) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrInstallDirMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrIsCameraActive) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByName) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByName2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByName2ForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByName2ndStage) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByNameForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByPath4) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByUri) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchAppByUri2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLaunchVideoStreamingApp) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLoadExec) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLoadSaveDataSystemFile) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLoopBackFormat) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrLoopBackMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrMmsMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrOverwriteLaunchParamForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPeekLaunchParamForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPhotoMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPhotoUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPspSaveDataGetParams) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPspSaveDataRead) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrPspSaveDataRootMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrReceiveEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrReceiveEventNum) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrReceiveNotificationRequestForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrReceiveShellEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrReceiveSystemEvent, SceAppMgrSystemEvent *systemEvent) {
    systemEvent->systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataAddMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataDataRemove) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataDataSave) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataGetQuota) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotCreate) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotDelete) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotFileClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotFileGetParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotFileOpen) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotGetParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotSetParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataSlotSetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSaveDataUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSendNotificationRequest) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSendParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSendSystemEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSendSystemEvent2) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetBackRenderPortOwner) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetBgmProxyApp) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetPowerSaveMode) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationForShell) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetShellScreenOrientation) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetSystemDataFile) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSetSystemDataFilePlayReady) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSystemParamDateTimeGetConf) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSystemParamGetInt) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrSystemParamGetString) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrTrophyMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrTrophyMountById) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrUmountByPid) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrUpdateSaveDataParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrWorkDirMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppMgrWorkDirMountById) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(_sceAppMgrGetAppState)
BRIDGE_IMPL(sceAppMgrAcidDirSet)
BRIDGE_IMPL(sceAppMgrAcquireSoundOutExclusive3)
BRIDGE_IMPL(sceAppMgrAddContAddMount)
BRIDGE_IMPL(sceAppMgrAddContMount)
BRIDGE_IMPL(sceAppMgrAppDataMount)
BRIDGE_IMPL(sceAppMgrAppDataMountById)
BRIDGE_IMPL(sceAppMgrAppMount)
BRIDGE_IMPL(sceAppMgrAppParamGetInt)
BRIDGE_IMPL(sceAppMgrAppParamGetString)
BRIDGE_IMPL(sceAppMgrAppParamSetString)
BRIDGE_IMPL(sceAppMgrAppUmount)
BRIDGE_IMPL(sceAppMgrBgdlGetQueueStatus)
BRIDGE_IMPL(sceAppMgrCaptureFrameBufDMACByAppId)
BRIDGE_IMPL(sceAppMgrCaptureFrameBufIFTUByAppId)
BRIDGE_IMPL(sceAppMgrCheckRifGD)
BRIDGE_IMPL(sceAppMgrContentInstallPeriodStart)
BRIDGE_IMPL(sceAppMgrContentInstallPeriodStop)
BRIDGE_IMPL(sceAppMgrConvertVs0UserDrivePath)
BRIDGE_IMPL(sceAppMgrDeclareShellProcess2)
BRIDGE_IMPL(sceAppMgrDestroyAppByName)
BRIDGE_IMPL(sceAppMgrDrmClose)
BRIDGE_IMPL(sceAppMgrDrmOpen)
BRIDGE_IMPL(sceAppMgrForceUmount)
BRIDGE_IMPL(sceAppMgrGameDataMount)
BRIDGE_IMPL(sceAppMgrGetAppInfo)
BRIDGE_IMPL(sceAppMgrGetAppMgrState)
BRIDGE_IMPL(sceAppMgrGetAppParam)
BRIDGE_IMPL(sceAppMgrGetAppParam2)
BRIDGE_IMPL(sceAppMgrGetBootParam)
BRIDGE_IMPL(sceAppMgrGetBudgetInfo)
BRIDGE_IMPL(sceAppMgrGetCoredumpStateForShell)
BRIDGE_IMPL(sceAppMgrGetCurrentBgmState)
BRIDGE_IMPL(sceAppMgrGetCurrentBgmState2)
BRIDGE_IMPL(sceAppMgrGetDevInfo)
BRIDGE_IMPL(sceAppMgrGetFgAppInfo)
BRIDGE_IMPL(sceAppMgrGetIdByName)
BRIDGE_IMPL(sceAppMgrGetMediaTypeFromDrive)
BRIDGE_IMPL(sceAppMgrGetMediaTypeFromDriveByPid)
BRIDGE_IMPL(sceAppMgrGetMountProcessNum)
BRIDGE_IMPL(sceAppMgrGetNameById)
BRIDGE_IMPL(sceAppMgrGetPfsDrive)
BRIDGE_IMPL(sceAppMgrGetPidListForShell)
BRIDGE_IMPL(sceAppMgrGetRawPath)
BRIDGE_IMPL(sceAppMgrGetRawPathOfApp0ByAppIdForShell)
BRIDGE_IMPL(sceAppMgrGetRawPathOfApp0ByPidForShell)
BRIDGE_IMPL(sceAppMgrGetRecommendedScreenOrientation)
BRIDGE_IMPL(sceAppMgrGetRunningAppIdListForShell)
BRIDGE_IMPL(sceAppMgrGetSaveDataInfo)
BRIDGE_IMPL(sceAppMgrGetSaveDataInfoForSpecialExport)
BRIDGE_IMPL(sceAppMgrGetStatusByAppId)
BRIDGE_IMPL(sceAppMgrGetStatusById)
BRIDGE_IMPL(sceAppMgrGetStatusByName)
BRIDGE_IMPL(sceAppMgrGetSystemDataFilePlayReady)
BRIDGE_IMPL(sceAppMgrGetUserDirPath)
BRIDGE_IMPL(sceAppMgrGetVs0UserDataDrive)
BRIDGE_IMPL(sceAppMgrGetVs0UserModuleDrive)
BRIDGE_IMPL(sceAppMgrInitSafeMemoryById)
BRIDGE_IMPL(sceAppMgrInstallDirMount)
BRIDGE_IMPL(sceAppMgrIsCameraActive)
BRIDGE_IMPL(sceAppMgrLaunchAppByName)
BRIDGE_IMPL(sceAppMgrLaunchAppByName2)
BRIDGE_IMPL(sceAppMgrLaunchAppByName2ForShell)
BRIDGE_IMPL(sceAppMgrLaunchAppByName2ndStage)
BRIDGE_IMPL(sceAppMgrLaunchAppByNameForShell)
BRIDGE_IMPL(sceAppMgrLaunchAppByPath4)
BRIDGE_IMPL(sceAppMgrLaunchAppByUri)
BRIDGE_IMPL(sceAppMgrLaunchAppByUri2)
BRIDGE_IMPL(sceAppMgrLaunchVideoStreamingApp)
BRIDGE_IMPL(sceAppMgrLoadExec)
BRIDGE_IMPL(sceAppMgrLoadSaveDataSystemFile)
BRIDGE_IMPL(sceAppMgrLoopBackFormat)
BRIDGE_IMPL(sceAppMgrLoopBackMount)
BRIDGE_IMPL(sceAppMgrMmsMount)
BRIDGE_IMPL(sceAppMgrOverwriteLaunchParamForShell)
BRIDGE_IMPL(sceAppMgrPeekLaunchParamForShell)
BRIDGE_IMPL(sceAppMgrPhotoMount)
BRIDGE_IMPL(sceAppMgrPhotoUmount)
BRIDGE_IMPL(sceAppMgrPspSaveDataGetParams)
BRIDGE_IMPL(sceAppMgrPspSaveDataRead)
BRIDGE_IMPL(sceAppMgrPspSaveDataRootMount)
BRIDGE_IMPL(sceAppMgrReceiveEvent)
BRIDGE_IMPL(sceAppMgrReceiveEventNum)
BRIDGE_IMPL(sceAppMgrReceiveNotificationRequestForShell)
BRIDGE_IMPL(sceAppMgrReceiveShellEvent)
BRIDGE_IMPL(sceAppMgrReceiveSystemEvent)
BRIDGE_IMPL(sceAppMgrSaveDataAddMount)
BRIDGE_IMPL(sceAppMgrSaveDataDataRemove)
BRIDGE_IMPL(sceAppMgrSaveDataDataSave)
BRIDGE_IMPL(sceAppMgrSaveDataGetQuota)
BRIDGE_IMPL(sceAppMgrSaveDataMount)
BRIDGE_IMPL(sceAppMgrSaveDataSlotCreate)
BRIDGE_IMPL(sceAppMgrSaveDataSlotDelete)
BRIDGE_IMPL(sceAppMgrSaveDataSlotFileClose)
BRIDGE_IMPL(sceAppMgrSaveDataSlotFileGetParam)
BRIDGE_IMPL(sceAppMgrSaveDataSlotFileOpen)
BRIDGE_IMPL(sceAppMgrSaveDataSlotGetParam)
BRIDGE_IMPL(sceAppMgrSaveDataSlotGetStatus)
BRIDGE_IMPL(sceAppMgrSaveDataSlotInit)
BRIDGE_IMPL(sceAppMgrSaveDataSlotSetParam)
BRIDGE_IMPL(sceAppMgrSaveDataSlotSetStatus)
BRIDGE_IMPL(sceAppMgrSaveDataUmount)
BRIDGE_IMPL(sceAppMgrSendNotificationRequest)
BRIDGE_IMPL(sceAppMgrSendParam)
BRIDGE_IMPL(sceAppMgrSendSystemEvent)
BRIDGE_IMPL(sceAppMgrSendSystemEvent2)
BRIDGE_IMPL(sceAppMgrSetBackRenderPortOwner)
BRIDGE_IMPL(sceAppMgrSetBgmProxyApp)
BRIDGE_IMPL(sceAppMgrSetNetworkDisconnectionWarningDialogState)
BRIDGE_IMPL(sceAppMgrSetPowerSaveMode)
BRIDGE_IMPL(sceAppMgrSetRecommendedScreenOrientationForShell)
BRIDGE_IMPL(sceAppMgrSetShellScreenOrientation)
BRIDGE_IMPL(sceAppMgrSetSystemDataFile)
BRIDGE_IMPL(sceAppMgrSetSystemDataFilePlayReady)
BRIDGE_IMPL(sceAppMgrSystemParamDateTimeGetConf)
BRIDGE_IMPL(sceAppMgrSystemParamGetInt)
BRIDGE_IMPL(sceAppMgrSystemParamGetString)
BRIDGE_IMPL(sceAppMgrTrophyMount)
BRIDGE_IMPL(sceAppMgrTrophyMountById)
BRIDGE_IMPL(sceAppMgrUmount)
BRIDGE_IMPL(sceAppMgrUmountByPid)
BRIDGE_IMPL(sceAppMgrUpdateSaveDataParam)
BRIDGE_IMPL(sceAppMgrWorkDirMount)
BRIDGE_IMPL(sceAppMgrWorkDirMountById)
