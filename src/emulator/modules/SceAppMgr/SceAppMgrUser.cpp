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
#include <psp2/appmgr.h>

EXPORT(int, _sceAppMgrGetAppState) {
    return unimplemented("_sceAppMgrGetAppState");
}

EXPORT(int, sceAppMgrAcidDirSet) {
    return unimplemented("sceAppMgrAcidDirSet");
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive3) {
    return unimplemented("sceAppMgrAcquireSoundOutExclusive3");
}

EXPORT(int, sceAppMgrAddContAddMount) {
    return unimplemented("sceAppMgrAddContAddMount");
}

EXPORT(int, sceAppMgrAddContMount) {
    return unimplemented("sceAppMgrAddContMount");
}

EXPORT(int, sceAppMgrAppDataMount) {
    return unimplemented("sceAppMgrAppDataMount");
}

EXPORT(int, sceAppMgrAppDataMountById) {
    return unimplemented("sceAppMgrAppDataMountById");
}

EXPORT(int, sceAppMgrAppMount) {
    return unimplemented("sceAppMgrAppMount");
}

EXPORT(int, sceAppMgrAppParamGetInt) {
    return unimplemented("sceAppMgrAppParamGetInt");
}

EXPORT(int, sceAppMgrAppParamGetString, int pid, int param, char *string, int length) {
    switch (param) {
    case 12:
    #ifdef WIN32
        strcpy_s(string, length, host.title_id.c_str());
    #else
        strncpy(string, host.title_id.c_str(), length);
    #endif
        break;
    default:
        return error(__func__, SCE_APPMGR_ERROR_INVALID);
    }
    return 0;
}

EXPORT(int, sceAppMgrAppParamSetString) {
    return unimplemented("sceAppMgrAppParamSetString");
}

EXPORT(int, sceAppMgrAppUmount) {
    return unimplemented("sceAppMgrAppUmount");
}

EXPORT(int, sceAppMgrBgdlGetQueueStatus) {
    return unimplemented("sceAppMgrBgdlGetQueueStatus");
}

EXPORT(int, sceAppMgrCaptureFrameBufDMACByAppId) {
    return unimplemented("sceAppMgrCaptureFrameBufDMACByAppId");
}

EXPORT(int, sceAppMgrCaptureFrameBufIFTUByAppId) {
    return unimplemented("sceAppMgrCaptureFrameBufIFTUByAppId");
}

EXPORT(int, sceAppMgrCheckRifGD) {
    return unimplemented("sceAppMgrCheckRifGD");
}

EXPORT(int, sceAppMgrContentInstallPeriodStart) {
    return unimplemented("sceAppMgrContentInstallPeriodStart");
}

EXPORT(int, sceAppMgrContentInstallPeriodStop) {
    return unimplemented("sceAppMgrContentInstallPeriodStop");
}

EXPORT(int, sceAppMgrConvertVs0UserDrivePath) {
    return unimplemented("sceAppMgrConvertVs0UserDrivePath");
}

EXPORT(int, sceAppMgrDeclareShellProcess2) {
    return unimplemented("sceAppMgrDeclareShellProcess2");
}

EXPORT(int, sceAppMgrDestroyAppByName) {
    return unimplemented("sceAppMgrDestroyAppByName");
}

EXPORT(int, sceAppMgrDrmClose) {
    return unimplemented("sceAppMgrDrmClose");
}

EXPORT(int, sceAppMgrDrmOpen) {
    return unimplemented("sceAppMgrDrmOpen");
}

EXPORT(int, sceAppMgrForceUmount) {
    return unimplemented("sceAppMgrForceUmount");
}

EXPORT(int, sceAppMgrGameDataMount) {
    return unimplemented("sceAppMgrGameDataMount");
}

EXPORT(int, sceAppMgrGetAppInfo) {
    return unimplemented("sceAppMgrGetAppInfo");
}

EXPORT(int, sceAppMgrGetAppMgrState) {
    return unimplemented("sceAppMgrGetAppMgrState");
}

EXPORT(int, sceAppMgrGetAppParam) {
    return unimplemented("sceAppMgrGetAppParam");
}

EXPORT(int, sceAppMgrGetAppParam2) {
    return unimplemented("sceAppMgrGetAppParam2");
}

EXPORT(int, sceAppMgrGetBootParam) {
    return unimplemented("sceAppMgrGetBootParam");
}

EXPORT(int, sceAppMgrGetBudgetInfo) {
    return unimplemented("sceAppMgrGetBudgetInfo");
}

EXPORT(int, sceAppMgrGetCoredumpStateForShell) {
    return unimplemented("sceAppMgrGetCoredumpStateForShell");
}

EXPORT(int, sceAppMgrGetCurrentBgmState) {
    return unimplemented("sceAppMgrGetCurrentBgmState");
}

EXPORT(int, sceAppMgrGetCurrentBgmState2) {
    return unimplemented("sceAppMgrGetCurrentBgmState2");
}

EXPORT(int, sceAppMgrGetDevInfo) {
    return unimplemented("sceAppMgrGetDevInfo");
}

EXPORT(int, sceAppMgrGetFgAppInfo) {
    return unimplemented("sceAppMgrGetFgAppInfo");
}

EXPORT(int, sceAppMgrGetIdByName) {
    return unimplemented("sceAppMgrGetIdByName");
}

EXPORT(int, sceAppMgrGetMediaTypeFromDrive) {
    return unimplemented("sceAppMgrGetMediaTypeFromDrive");
}

EXPORT(int, sceAppMgrGetMediaTypeFromDriveByPid) {
    return unimplemented("sceAppMgrGetMediaTypeFromDriveByPid");
}

EXPORT(int, sceAppMgrGetMountProcessNum) {
    return unimplemented("sceAppMgrGetMountProcessNum");
}

EXPORT(int, sceAppMgrGetNameById) {
    return unimplemented("sceAppMgrGetNameById");
}

EXPORT(int, sceAppMgrGetPfsDrive) {
    return unimplemented("sceAppMgrGetPfsDrive");
}

EXPORT(int, sceAppMgrGetPidListForShell) {
    return unimplemented("sceAppMgrGetPidListForShell");
}

EXPORT(int, sceAppMgrGetRawPath) {
    return unimplemented("sceAppMgrGetRawPath");
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    return unimplemented("sceAppMgrGetRawPathOfApp0ByAppIdForShell");
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByPidForShell) {
    return unimplemented("sceAppMgrGetRawPathOfApp0ByPidForShell");
}

EXPORT(int, sceAppMgrGetRecommendedScreenOrientation) {
    return unimplemented("sceAppMgrGetRecommendedScreenOrientation");
}

EXPORT(int, sceAppMgrGetRunningAppIdListForShell) {
    return unimplemented("sceAppMgrGetRunningAppIdListForShell");
}

EXPORT(int, sceAppMgrGetSaveDataInfo) {
    return unimplemented("sceAppMgrGetSaveDataInfo");
}

EXPORT(int, sceAppMgrGetSaveDataInfoForSpecialExport) {
    return unimplemented("sceAppMgrGetSaveDataInfoForSpecialExport");
}

EXPORT(int, sceAppMgrGetStatusByAppId) {
    return unimplemented("sceAppMgrGetStatusByAppId");
}

EXPORT(int, sceAppMgrGetStatusById) {
    return unimplemented("sceAppMgrGetStatusById");
}

EXPORT(int, sceAppMgrGetStatusByName) {
    return unimplemented("sceAppMgrGetStatusByName");
}

EXPORT(int, sceAppMgrGetSystemDataFilePlayReady) {
    return unimplemented("sceAppMgrGetSystemDataFilePlayReady");
}

EXPORT(int, sceAppMgrGetUserDirPath) {
    return unimplemented("sceAppMgrGetUserDirPath");
}

EXPORT(int, sceAppMgrGetVs0UserDataDrive) {
    return unimplemented("sceAppMgrGetVs0UserDataDrive");
}

EXPORT(int, sceAppMgrGetVs0UserModuleDrive) {
    return unimplemented("sceAppMgrGetVs0UserModuleDrive");
}

EXPORT(int, sceAppMgrInitSafeMemoryById) {
    return unimplemented("sceAppMgrInitSafeMemoryById");
}

EXPORT(int, sceAppMgrInstallDirMount) {
    return unimplemented("sceAppMgrInstallDirMount");
}

EXPORT(int, sceAppMgrIsCameraActive) {
    return unimplemented("sceAppMgrIsCameraActive");
}

EXPORT(int, sceAppMgrLaunchAppByName) {
    return unimplemented("sceAppMgrLaunchAppByName");
}

EXPORT(int, sceAppMgrLaunchAppByName2) {
    return unimplemented("sceAppMgrLaunchAppByName2");
}

EXPORT(int, sceAppMgrLaunchAppByName2ForShell) {
    return unimplemented("sceAppMgrLaunchAppByName2ForShell");
}

EXPORT(int, sceAppMgrLaunchAppByName2ndStage) {
    return unimplemented("sceAppMgrLaunchAppByName2ndStage");
}

EXPORT(int, sceAppMgrLaunchAppByNameForShell) {
    return unimplemented("sceAppMgrLaunchAppByNameForShell");
}

EXPORT(int, sceAppMgrLaunchAppByPath4) {
    return unimplemented("sceAppMgrLaunchAppByPath4");
}

EXPORT(int, sceAppMgrLaunchAppByUri) {
    return unimplemented("sceAppMgrLaunchAppByUri");
}

EXPORT(int, sceAppMgrLaunchAppByUri2) {
    return unimplemented("sceAppMgrLaunchAppByUri2");
}

EXPORT(int, sceAppMgrLaunchVideoStreamingApp) {
    return unimplemented("sceAppMgrLaunchVideoStreamingApp");
}

EXPORT(int, sceAppMgrLoadExec) {
    return unimplemented("sceAppMgrLoadExec");
}

EXPORT(int, sceAppMgrLoopBackFormat) {
    return unimplemented("sceAppMgrLoopBackFormat");
}

EXPORT(int, sceAppMgrLoopBackMount) {
    return unimplemented("sceAppMgrLoopBackMount");
}

EXPORT(int, sceAppMgrMmsMount) {
    return unimplemented("sceAppMgrMmsMount");
}

EXPORT(int, sceAppMgrOverwriteLaunchParamForShell) {
    return unimplemented("sceAppMgrOverwriteLaunchParamForShell");
}

EXPORT(int, sceAppMgrPeekLaunchParamForShell) {
    return unimplemented("sceAppMgrPeekLaunchParamForShell");
}

EXPORT(int, sceAppMgrPhotoMount) {
    return unimplemented("sceAppMgrPhotoMount");
}

EXPORT(int, sceAppMgrPhotoUmount) {
    return unimplemented("sceAppMgrPhotoUmount");
}

EXPORT(int, sceAppMgrPspSaveDataGetParams) {
    return unimplemented("sceAppMgrPspSaveDataGetParams");
}

EXPORT(int, sceAppMgrPspSaveDataRead) {
    return unimplemented("sceAppMgrPspSaveDataRead");
}

EXPORT(int, sceAppMgrPspSaveDataRootMount) {
    return unimplemented("sceAppMgrPspSaveDataRootMount");
}

EXPORT(int, sceAppMgrReceiveEvent) {
    return unimplemented("sceAppMgrReceiveEvent");
}

EXPORT(int, sceAppMgrReceiveEventNum) {
    return unimplemented("sceAppMgrReceiveEventNum");
}

EXPORT(int, sceAppMgrReceiveNotificationRequestForShell) {
    return unimplemented("sceAppMgrReceiveNotificationRequestForShell");
}

EXPORT(int, sceAppMgrReceiveShellEvent) {
    return unimplemented("sceAppMgrReceiveShellEvent");
}

EXPORT(int, sceAppMgrReceiveSystemEvent) {
    return unimplemented("sceAppMgrReceiveSystemEvent");
}

EXPORT(int, sceAppMgrSaveDataAddMount) {
    return unimplemented("sceAppMgrSaveDataAddMount");
}

EXPORT(int, sceAppMgrSaveDataDataRemove) {
    return unimplemented("sceAppMgrSaveDataDataRemove");
}

EXPORT(int, sceAppMgrSaveDataDataSave) {
    return unimplemented("sceAppMgrSaveDataDataSave");
}

EXPORT(int, sceAppMgrSaveDataGetQuota) {
    return unimplemented("sceAppMgrSaveDataGetQuota");
}

EXPORT(int, sceAppMgrSaveDataMount) {
    return unimplemented("sceAppMgrSaveDataMount");
}

EXPORT(int, sceAppMgrSaveDataSlotCreate) {
    return unimplemented("sceAppMgrSaveDataSlotCreate");
}

EXPORT(int, sceAppMgrSaveDataSlotDelete) {
    return unimplemented("sceAppMgrSaveDataSlotDelete");
}

EXPORT(int, sceAppMgrSaveDataSlotFileClose) {
    return unimplemented("sceAppMgrSaveDataSlotFileClose");
}

EXPORT(int, sceAppMgrSaveDataSlotFileGetParam) {
    return unimplemented("sceAppMgrSaveDataSlotFileGetParam");
}

EXPORT(int, sceAppMgrSaveDataSlotFileOpen) {
    return unimplemented("sceAppMgrSaveDataSlotFileOpen");
}

EXPORT(int, sceAppMgrSaveDataSlotGetParam) {
    return unimplemented("sceAppMgrSaveDataSlotGetParam");
}

EXPORT(int, sceAppMgrSaveDataSlotGetStatus) {
    return unimplemented("sceAppMgrSaveDataSlotGetStatus");
}

EXPORT(int, sceAppMgrSaveDataSlotInit) {
    return unimplemented("sceAppMgrSaveDataSlotInit");
}

EXPORT(int, sceAppMgrSaveDataSlotSetParam) {
    return unimplemented("sceAppMgrSaveDataSlotSetParam");
}

EXPORT(int, sceAppMgrSaveDataSlotSetStatus) {
    return unimplemented("sceAppMgrSaveDataSlotSetStatus");
}

EXPORT(int, sceAppMgrSaveDataUmount) {
    return unimplemented("sceAppMgrSaveDataUmount");
}

EXPORT(int, sceAppMgrSendNotificationRequest) {
    return unimplemented("sceAppMgrSendNotificationRequest");
}

EXPORT(int, sceAppMgrSendParam) {
    return unimplemented("sceAppMgrSendParam");
}

EXPORT(int, sceAppMgrSendSystemEvent) {
    return unimplemented("sceAppMgrSendSystemEvent");
}

EXPORT(int, sceAppMgrSendSystemEvent2) {
    return unimplemented("sceAppMgrSendSystemEvent2");
}

EXPORT(int, sceAppMgrSetBackRenderPortOwner) {
    return unimplemented("sceAppMgrSetBackRenderPortOwner");
}

EXPORT(int, sceAppMgrSetBgmProxyApp) {
    return unimplemented("sceAppMgrSetBgmProxyApp");
}

EXPORT(int, sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    return unimplemented("sceAppMgrSetNetworkDisconnectionWarningDialogState");
}

EXPORT(int, sceAppMgrSetPowerSaveMode) {
    return unimplemented("sceAppMgrSetPowerSaveMode");
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationForShell) {
    return unimplemented("sceAppMgrSetRecommendedScreenOrientationForShell");
}

EXPORT(int, sceAppMgrSetShellScreenOrientation) {
    return unimplemented("sceAppMgrSetShellScreenOrientation");
}

EXPORT(int, sceAppMgrSetSystemDataFile) {
    return unimplemented("sceAppMgrSetSystemDataFile");
}

EXPORT(int, sceAppMgrSetSystemDataFilePlayReady) {
    return unimplemented("sceAppMgrSetSystemDataFilePlayReady");
}

EXPORT(int, sceAppMgrSystemParamDateTimeGetConf) {
    return unimplemented("sceAppMgrSystemParamDateTimeGetConf");
}

EXPORT(int, sceAppMgrSystemParamGetInt) {
    return unimplemented("sceAppMgrSystemParamGetInt");
}

EXPORT(int, sceAppMgrSystemParamGetString) {
    return unimplemented("sceAppMgrSystemParamGetString");
}

EXPORT(int, sceAppMgrTrophyMount) {
    return unimplemented("sceAppMgrTrophyMount");
}

EXPORT(int, sceAppMgrTrophyMountById) {
    return unimplemented("sceAppMgrTrophyMountById");
}

EXPORT(int, sceAppMgrUmount) {
    return unimplemented("sceAppMgrUmount");
}

EXPORT(int, sceAppMgrUmountByPid) {
    return unimplemented("sceAppMgrUmountByPid");
}

EXPORT(int, sceAppMgrUpdateSaveDataParam) {
    return unimplemented("sceAppMgrUpdateSaveDataParam");
}

EXPORT(int, sceAppMgrWorkDirMount) {
    return unimplemented("sceAppMgrWorkDirMount");
}

EXPORT(int, sceAppMgrWorkDirMountById) {
    return unimplemented("sceAppMgrWorkDirMountById");
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
