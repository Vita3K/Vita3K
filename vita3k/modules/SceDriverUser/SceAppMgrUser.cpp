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

#include "SceAppMgrUser.h"

#include <emuenv/app_util.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <packages/functions.h>

#include <modules/module_parent.h>
#include <util/find.h>

struct SceAppMgrSaveDataDataDelete {
    int size;
    unsigned int slotId;
    Ptr<SceAppUtilSaveDataSlotParam> slotParam;
    uint8_t reserved[32];
    Ptr<SceAppUtilSaveDataDataSaveItem> files;
    int fileNum;
    SceAppUtilMountPoint *mountPoint;
};

struct SceAppMgrSaveDataData {
    int size;
    unsigned int slotId;
    Ptr<SceAppUtilSaveDataSlotParam> slotParam;
    uint8_t reserved[32];
    Ptr<SceAppUtilSaveDataDataSaveItem> files;
    int fileNum;
    SceAppUtilMountPoint *mountPoint;
    unsigned int *requiredSizeKiB;
};

EXPORT(SceInt32, _sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion) {
    return CALL_EXPORT(__sceAppMgrGetAppState, appState, sizeofSceAppMgrAppState, buildVersion);
}

EXPORT(int, sceAppMgrAcidDirSet) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive3) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAddContAddMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAddContMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppDataMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppDataMountById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppParamGetInt) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrAppParamGetString, int pid, int param, char *string, int length) {
    return CALL_EXPORT(_sceAppMgrAppParamGetString, pid, param, string, length);
}

EXPORT(int, sceAppMgrAppParamSetString) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrBgdlGetQueueStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCaptureFrameBufDMACByAppId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCaptureFrameBufIFTUByAppId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCheckRifGD) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrContentInstallPeriodStart) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrContentInstallPeriodStop) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrConvertVs0UserDrivePath) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDeclareShellProcess2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyAppByName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDrmClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDrmOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrForceUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGameDataMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppMgrState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppParam2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetBootParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetBudgetInfo) {
    STUBBED("not system mode");
    return -1;
}

EXPORT(int, sceAppMgrGetCoredumpStateForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetCurrentBgmState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetCurrentBgmState2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetDevInfo, const char *dev, uint64_t *max_size, uint64_t *free_size) {
    if (dev == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }

    auto device = device::get_device(dev);
    if (device == VitaIoDevice::_INVALID) {
        LOG_ERROR("Cannot find device for path: {}", dev);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }

    fs::path dev_path = device._to_string();
    fs::path path = emuenv.pref_path / dev_path;
    fs::space_info space = fs::space(path);

    // TODO: Use free or available?
    // free = free space available on the whole partition
    // available = free space available to a non-privileged process
    // Using available in case the drive is nearly full and the game tries to write since available will always be smaller
    *free_size = space.available;
    *max_size = space.capacity;

    return 0;
}

EXPORT(int, sceAppMgrGetFgAppInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetIdByName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMediaTypeFromDrive) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMediaTypeFromDriveByPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMountProcessNum) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetNameById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetPfsDrive) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetPidListForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRawPath) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByPidForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRecommendedScreenOrientation) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRunningAppIdListForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSaveDataInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSaveDataInfoForSpecialExport) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusByAppId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusByName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSystemDataFilePlayReady) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetUserDirPath) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetUserDirPathById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetVs0UserDataDrive) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetVs0UserModuleDrive) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrInitSafeMemoryById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrInstallDirMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsCameraActive) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2ForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2ndStage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByNameForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByPath4) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByUri) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByUri2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchVideoStreamingApp) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam) {
    return CALL_EXPORT(_sceAppMgrLoadExec, appPath, argv, optParam);
}

EXPORT(int, sceAppMgrLoadSaveDataSystemFile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLoopBackFormat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLoopBackMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrMmsMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrOverwriteLaunchParamForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPeekLaunchParamForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPhotoMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPhotoUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataGetParams) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataRootMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveEventNum) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveNotificationRequestForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveShellEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveSystemEvent, SceAppMgrSystemEvent *systemEvent) {
    systemEvent->systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataAddMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataRemove, Ptr<SceAppMgrSaveDataDataDelete> data) {
    for (int i = 0; i < data.get(emuenv.mem)->fileNum; i++) {
        const auto file = fs::path(construct_savedata0_path(data.get(emuenv.mem)->files.get(emuenv.mem)[i].dataPath.get(emuenv.mem)));
        if (fs::is_regular_file(file)) {
            remove_file(emuenv.io, file.string().c_str(), emuenv.pref_path, export_name);
        } else
            remove_dir(emuenv.io, file.string().c_str(), emuenv.pref_path, export_name);
    }

    return 0;
}

EXPORT(int, sceAppMgrSaveDataDataRemove2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataSave, Ptr<SceAppMgrSaveDataData> data) {
    SceUID fd;

    for (int i = 0; i < data.get(emuenv.mem)->fileNum; i++) {
        auto files = data.get(emuenv.mem)->files.get(emuenv.mem);

        const auto file_path = construct_savedata0_path(files[i].dataPath.get(emuenv.mem));
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY:
            create_dir(emuenv.io, file_path.c_str(), 0777, emuenv.pref_path, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE:
            if (files[i].buf) {
                fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path, export_name);
                seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, emuenv.io, export_name);
                write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
                close_file(emuenv.io, fd, export_name);
            }
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_APPEND | SCE_O_TRUNC, emuenv.pref_path, export_name);
            truncate_file(fd, files[i].bufSize + files[i].offset, emuenv.io, export_name);
            close_file(emuenv.io, fd, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE:
        default:
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path, export_name);
            seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, emuenv.io, export_name);
            if (files[i].buf.get(emuenv.mem)) {
                write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
            }
            close_file(emuenv.io, fd, export_name);
            break;
        }
    }
    return 0;
}

EXPORT(int, sceAppMgrSaveDataDataSave2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataGetQuota) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotCreate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotDelete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileGetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotGetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotSetParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotSetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendNotificationRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendSystemEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendSystemEvent2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBackRenderPortOwner) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmProxyApp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetPowerSaveMode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationForShell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetShellScreenOrientation) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemDataFile) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemDataFilePlayReady) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamDateTimeGetConf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamGetInt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamGetString) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrThemeDataMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrTrophyMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrTrophyMountById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUmountByPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUpdateSaveDataParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrWorkDirMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrWorkDirMountById) {
    return UNIMPLEMENTED();
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
BRIDGE_IMPL(sceAppMgrGetUserDirPathById)
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
BRIDGE_IMPL(sceAppMgrSaveDataDataRemove2)
BRIDGE_IMPL(sceAppMgrSaveDataDataSave)
BRIDGE_IMPL(sceAppMgrSaveDataDataSave2)
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
BRIDGE_IMPL(sceAppMgrThemeDataMount)
BRIDGE_IMPL(sceAppMgrTrophyMount)
BRIDGE_IMPL(sceAppMgrTrophyMountById)
BRIDGE_IMPL(sceAppMgrUmount)
BRIDGE_IMPL(sceAppMgrUmountByPid)
BRIDGE_IMPL(sceAppMgrUpdateSaveDataParam)
BRIDGE_IMPL(sceAppMgrWorkDirMount)
BRIDGE_IMPL(sceAppMgrWorkDirMountById)
