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

#pragma once

#include <module/module.h>

enum SceAppMgrErrorCode {
    SCE_APPMGR_ERROR_BUSY = 0x80802000, //!< Busy
    SCE_APPMGR_ERROR_STATE = 0x80802013, //!< Invalid state
    SCE_APPMGR_ERROR_NULL_POINTER = 0x80802016, //!< NULL pointer
    SCE_APPMGR_ERROR_INVALID = 0x8080201A, //!< Invalid param
    SCE_APPMGR_ERROR_TOO_LONG_ARGV = 0x8080201D, //!< argv is too long
    SCE_APPMGR_ERROR_INVALID_SELF_PATH = 0x8080201E, //!< Invalid SELF path
    SCE_APPMGR_ERROR_BGM_PORT_BUSY = 0x80803000 //!< BGM port was occupied and could not be secured
};

enum SceAppMgrSystemEventType {
    SCE_APPMGR_SYSTEMEVENT_ON_RESUME = 0x10000003, //!< Application resumed
    SCE_APPMGR_SYSTEMEVENT_ON_STORE_PURCHASE = 0x10000004, //!< Store checkout event arrived
    SCE_APPMGR_SYSTEMEVENT_ON_NP_MESSAGE_ARRIVED = 0x10000005, //!< NP message event arrived
    SCE_APPMGR_SYSTEMEVENT_ON_STORE_REDEMPTION = 0x10000006 //!< Promotion code redeemed at PlayStation�Store
};

typedef struct SceAppMgrSystemEvent {
    SceInt32 systemEvent; //!< System event ID
    SceUInt8 reserved[60]; //!< Reserved data
} SceAppMgrSystemEvent;

typedef struct SceAppMgrAppState {
    SceUInt32 systemEventNum; //!< Number of system events
    SceUInt32 appEventNum; //!< Number of application events
    SceBool isSystemUiOverlaid; //!< Truth-value of UI overlaid of system software
    SceUInt8 reserved[128 - sizeof(SceUInt32) * 2 - sizeof(SceBool)]; //!< Reserved area
} SceAppMgrAppState;

typedef struct SceAppMgrLoadExecOptParam {
    int reserved[256 / 4]; //!< Reserved area
} SceAppMgrLoadExecOptParam;

EXPORT(SceInt32, __sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, char *string, int length);
EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam);

SceInt32 __sceAppMgrGetAppState(SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
SceInt32 _sceAppMgrLoadExec(const char *appPath, char *const argv[], const SceAppMgrLoadExecOptParam *optParam);

BRIDGE_DECL(__sceAppMgrGetAppState)
BRIDGE_DECL(_sceAppMgrAcidDirSet)
BRIDGE_DECL(_sceAppMgrAcquireSoundOutExclusive3)
BRIDGE_DECL(_sceAppMgrAddContAddMount)
BRIDGE_DECL(_sceAppMgrAddContMount)
BRIDGE_DECL(_sceAppMgrAppDataMount)
BRIDGE_DECL(_sceAppMgrAppDataMountById)
BRIDGE_DECL(_sceAppMgrAppMount)
BRIDGE_DECL(_sceAppMgrAppParamGetInt)
BRIDGE_DECL(_sceAppMgrAppParamGetString)
BRIDGE_DECL(_sceAppMgrAppParamSetString)
BRIDGE_DECL(_sceAppMgrAppUmount)
BRIDGE_DECL(_sceAppMgrBgdlGetQueueStatus)
BRIDGE_DECL(_sceAppMgrCaptureFrameBufDMACByAppId)
BRIDGE_DECL(_sceAppMgrCaptureFrameBufIFTUByAppId)
BRIDGE_DECL(_sceAppMgrCheckRifGD)
BRIDGE_DECL(_sceAppMgrContentInstallPeriodStart)
BRIDGE_DECL(_sceAppMgrContentInstallPeriodStop)
BRIDGE_DECL(_sceAppMgrConvertVs0UserDrivePath)
BRIDGE_DECL(_sceAppMgrDeclareShellProcess2)
BRIDGE_DECL(_sceAppMgrDestroyAppByName)
BRIDGE_DECL(_sceAppMgrDrmClose)
BRIDGE_DECL(_sceAppMgrDrmOpen)
BRIDGE_DECL(_sceAppMgrForceUmount)
BRIDGE_DECL(_sceAppMgrGameDataMount)
BRIDGE_DECL(_sceAppMgrGetAppInfo)
BRIDGE_DECL(_sceAppMgrGetAppMgrState)
BRIDGE_DECL(_sceAppMgrGetAppParam)
BRIDGE_DECL(_sceAppMgrGetAppParam2)
BRIDGE_DECL(_sceAppMgrGetBootParam)
BRIDGE_DECL(_sceAppMgrGetBudgetInfo)
BRIDGE_DECL(_sceAppMgrGetCoredumpStateForShell)
BRIDGE_DECL(_sceAppMgrGetCurrentBgmState)
BRIDGE_DECL(_sceAppMgrGetCurrentBgmState2)
BRIDGE_DECL(_sceAppMgrGetDevInfo)
BRIDGE_DECL(_sceAppMgrGetFgAppInfo)
BRIDGE_DECL(_sceAppMgrGetIdByName)
BRIDGE_DECL(_sceAppMgrGetMediaTypeFromDrive)
BRIDGE_DECL(_sceAppMgrGetMediaTypeFromDriveByPid)
BRIDGE_DECL(_sceAppMgrGetMountProcessNum)
BRIDGE_DECL(_sceAppMgrGetNameById)
BRIDGE_DECL(_sceAppMgrGetPfsDrive)
BRIDGE_DECL(_sceAppMgrGetPidListForShell)
BRIDGE_DECL(_sceAppMgrGetRawPath)
BRIDGE_DECL(_sceAppMgrGetRawPathOfApp0ByAppIdForShell)
BRIDGE_DECL(_sceAppMgrGetRawPathOfApp0ByPidForShell)
BRIDGE_DECL(_sceAppMgrGetRecommendedScreenOrientation)
BRIDGE_DECL(_sceAppMgrGetRunningAppIdListForShell)
BRIDGE_DECL(_sceAppMgrGetSaveDataInfo)
BRIDGE_DECL(_sceAppMgrGetSaveDataInfoForSpecialExport)
BRIDGE_DECL(_sceAppMgrGetStatusByAppId)
BRIDGE_DECL(_sceAppMgrGetStatusById)
BRIDGE_DECL(_sceAppMgrGetStatusByName)
BRIDGE_DECL(_sceAppMgrGetSystemDataFilePlayReady)
BRIDGE_DECL(_sceAppMgrGetUserDirPath)
BRIDGE_DECL(_sceAppMgrGetUserDirPathById)
BRIDGE_DECL(_sceAppMgrGetVs0UserDataDrive)
BRIDGE_DECL(_sceAppMgrGetVs0UserModuleDrive)
BRIDGE_DECL(_sceAppMgrInitSafeMemoryById)
BRIDGE_DECL(_sceAppMgrInstallDirMount)
BRIDGE_DECL(_sceAppMgrIsCameraActive)
BRIDGE_DECL(_sceAppMgrLaunchAppByName)
BRIDGE_DECL(_sceAppMgrLaunchAppByName2)
BRIDGE_DECL(_sceAppMgrLaunchAppByName2ForShell)
BRIDGE_DECL(_sceAppMgrLaunchAppByName2ndStage)
BRIDGE_DECL(_sceAppMgrLaunchAppByNameForShell)
BRIDGE_DECL(_sceAppMgrLaunchAppByPath4)
BRIDGE_DECL(_sceAppMgrLaunchAppByUri)
BRIDGE_DECL(_sceAppMgrLaunchAppByUri2)
BRIDGE_DECL(_sceAppMgrLaunchVideoStreamingApp)
BRIDGE_DECL(_sceAppMgrLoadExec)
BRIDGE_DECL(_sceAppMgrLoadSaveDataSystemFile)
BRIDGE_DECL(_sceAppMgrLoopBackFormat)
BRIDGE_DECL(_sceAppMgrLoopBackMount)
BRIDGE_DECL(_sceAppMgrMmsMount)
BRIDGE_DECL(_sceAppMgrOverwriteLaunchParamForShell)
BRIDGE_DECL(_sceAppMgrPeekLaunchParamForShell)
BRIDGE_DECL(_sceAppMgrPhotoMount)
BRIDGE_DECL(_sceAppMgrPhotoUmount)
BRIDGE_DECL(_sceAppMgrPspSaveDataGetParams)
BRIDGE_DECL(_sceAppMgrPspSaveDataRead)
BRIDGE_DECL(_sceAppMgrPspSaveDataRootMount)
BRIDGE_DECL(_sceAppMgrReceiveEvent)
BRIDGE_DECL(_sceAppMgrReceiveEventNum)
BRIDGE_DECL(_sceAppMgrReceiveNotificationRequestForShell)
BRIDGE_DECL(_sceAppMgrReceiveShellEvent)
BRIDGE_DECL(_sceAppMgrReceiveSystemEvent)
BRIDGE_DECL(_sceAppMgrSaveDataAddMount)
BRIDGE_DECL(_sceAppMgrSaveDataDataRemove)
BRIDGE_DECL(_sceAppMgrSaveDataDataRemove2)
BRIDGE_DECL(_sceAppMgrSaveDataDataSave)
BRIDGE_DECL(_sceAppMgrSaveDataDataSave2)
BRIDGE_DECL(_sceAppMgrSaveDataGetQuota)
BRIDGE_DECL(_sceAppMgrSaveDataMount)
BRIDGE_DECL(_sceAppMgrSaveDataSlotCreate)
BRIDGE_DECL(_sceAppMgrSaveDataSlotDelete)
BRIDGE_DECL(_sceAppMgrSaveDataSlotFileClose)
BRIDGE_DECL(_sceAppMgrSaveDataSlotFileGetParam)
BRIDGE_DECL(_sceAppMgrSaveDataSlotFileOpen)
BRIDGE_DECL(_sceAppMgrSaveDataSlotGetParam)
BRIDGE_DECL(_sceAppMgrSaveDataSlotGetStatus)
BRIDGE_DECL(_sceAppMgrSaveDataSlotInit)
BRIDGE_DECL(_sceAppMgrSaveDataSlotSetParam)
BRIDGE_DECL(_sceAppMgrSaveDataSlotSetStatus)
BRIDGE_DECL(_sceAppMgrSaveDataUmount)
BRIDGE_DECL(_sceAppMgrSendNotificationRequest)
BRIDGE_DECL(_sceAppMgrSendParam)
BRIDGE_DECL(_sceAppMgrSendSystemEvent)
BRIDGE_DECL(_sceAppMgrSendSystemEvent2)
BRIDGE_DECL(_sceAppMgrSetBackRenderPortOwner)
BRIDGE_DECL(_sceAppMgrSetBgmProxyApp)
BRIDGE_DECL(_sceAppMgrSetNetworkDisconnectionWarningDialogState)
BRIDGE_DECL(_sceAppMgrSetPowerSaveMode)
BRIDGE_DECL(_sceAppMgrSetRecommendedScreenOrientationForShell)
BRIDGE_DECL(_sceAppMgrSetShellScreenOrientation)
BRIDGE_DECL(_sceAppMgrSetSystemDataFile)
BRIDGE_DECL(_sceAppMgrSetSystemDataFilePlayReady)
BRIDGE_DECL(_sceAppMgrSystemParamDateTimeGetConf)
BRIDGE_DECL(_sceAppMgrSystemParamGetInt)
BRIDGE_DECL(_sceAppMgrSystemParamGetString)
BRIDGE_DECL(_sceAppMgrThemeDataMount)
BRIDGE_DECL(_sceAppMgrTrophyMount)
BRIDGE_DECL(_sceAppMgrTrophyMountById)
BRIDGE_DECL(_sceAppMgrUmount)
BRIDGE_DECL(_sceAppMgrUmountByPid)
BRIDGE_DECL(_sceAppMgrUpdateSaveDataParam)
BRIDGE_DECL(_sceAppMgrWorkDirMount)
BRIDGE_DECL(_sceAppMgrWorkDirMountById)
BRIDGE_DECL(sceAppMgrAcquireBgmPort)
BRIDGE_DECL(sceAppMgrAcquireBgmPortForMusicPlayer)
BRIDGE_DECL(sceAppMgrAcquireBgmPortWithPriority)
BRIDGE_DECL(sceAppMgrAcquireBtrm)
BRIDGE_DECL(sceAppMgrAcquireSoundOutExclusive)
BRIDGE_DECL(sceAppMgrAcquireSoundOutExclusive2)
BRIDGE_DECL(sceAppMgrActivateApp)
BRIDGE_DECL(sceAppMgrDeactivateApp)
BRIDGE_DECL(sceAppMgrDeclareSystemChatApp)
BRIDGE_DECL(sceAppMgrDestroyAppByAppId)
BRIDGE_DECL(sceAppMgrDestroyOtherApp)
BRIDGE_DECL(sceAppMgrDestroyOtherAppByAppIdForShell)
BRIDGE_DECL(sceAppMgrDestroyOtherAppByPidForShell)
BRIDGE_DECL(sceAppMgrDump)
BRIDGE_DECL(sceAppMgrEnableCoredumpForTest)
BRIDGE_DECL(sceAppMgrEnableDuckingOnSystemChat)
BRIDGE_DECL(sceAppMgrEnablePrioritizingSystemChat)
BRIDGE_DECL(sceAppMgrExitToLiveboardForGameApp)
BRIDGE_DECL(sceAppMgrFinishCoredumpForShell)
BRIDGE_DECL(sceAppMgrGetAppIdByAppId)
BRIDGE_DECL(sceAppMgrGetExtraAppParam)
BRIDGE_DECL(sceAppMgrGetProcessIdByAppIdForShell)
BRIDGE_DECL(sceAppMgrGetSystemDataFile)
BRIDGE_DECL(sceAppMgrGrowMemory)
BRIDGE_DECL(sceAppMgrGrowMemory3)
BRIDGE_DECL(sceAppMgrIsDevelopmentMode)
BRIDGE_DECL(sceAppMgrIsGameBudgetAppPresent)
BRIDGE_DECL(sceAppMgrIsGameProgram)
BRIDGE_DECL(sceAppMgrIsNonGameProgram)
BRIDGE_DECL(sceAppMgrIsOtherAppPresent)
BRIDGE_DECL(sceAppMgrIsPidShellAndCrashed)
BRIDGE_DECL(sceAppMgrIsPsNowClient)
BRIDGE_DECL(sceAppMgrLaunchAppCancel)
BRIDGE_DECL(sceAppMgrLoadSafeMemory)
BRIDGE_DECL(sceAppMgrNotifyLiveBoardModeForShell)
BRIDGE_DECL(sceAppMgrQuitApp)
BRIDGE_DECL(sceAppMgrQuitForNonSuspendableApp)
BRIDGE_DECL(sceAppMgrReceiveShellEventNum)
BRIDGE_DECL(sceAppMgrReleaseBgmPort)
BRIDGE_DECL(sceAppMgrReleaseBtrm)
BRIDGE_DECL(sceAppMgrReleaseSoundOutExclusive)
BRIDGE_DECL(sceAppMgrReleaseSoundOutExclusive2)
BRIDGE_DECL(sceAppMgrReleaseSoundOutExclusive3)
BRIDGE_DECL(sceAppMgrRestoreBgmSettingForShell)
BRIDGE_DECL(sceAppMgrRestoreDisplaySettingForShell)
BRIDGE_DECL(sceAppMgrResumeBgAppByShell)
BRIDGE_DECL(sceAppMgrReturnLiveAreaOperationResultForShell)
BRIDGE_DECL(sceAppMgrSaveDataGetCachedRequiredSizeKiB)
BRIDGE_DECL(sceAppMgrSaveSafeMemory)
BRIDGE_DECL(sceAppMgrSendLiveBoardMode)
BRIDGE_DECL(sceAppMgrSetAppProtectionModeOnMemoryShortage)
BRIDGE_DECL(sceAppMgrSetBgmSubPriority)
BRIDGE_DECL(sceAppMgrSetBgmSubPriorityForSystemChat)
BRIDGE_DECL(sceAppMgrSetDisplayMergeConf)
BRIDGE_DECL(sceAppMgrSetFakeSettingBug51800)
BRIDGE_DECL(sceAppMgrSetInfobarState)
BRIDGE_DECL(sceAppMgrSetInfobarStateForCommonDialog)
BRIDGE_DECL(sceAppMgrSetInfobarStateForShellByAppId)
BRIDGE_DECL(sceAppMgrSetRecommendedScreenOrientationActivated)
BRIDGE_DECL(sceAppMgrSetSystemImposeState)
BRIDGE_DECL(sceAppMgrSetSystemImposeState2)
BRIDGE_DECL(sceAppMgrSuspendBgAppByShell)
BRIDGE_DECL(sceAppMgrSuspendUntilActivated)
