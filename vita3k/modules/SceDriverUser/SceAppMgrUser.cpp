// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <util/tracy.h>
TRACY_MODULE_NAME(SceAppMgrUser);

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
    TRACY_FUNC(_sceAppMgrGetAppState, appState, sizeofSceAppMgrAppState, buildVersion);
    return CALL_EXPORT(__sceAppMgrGetAppState, appState, sizeofSceAppMgrAppState, buildVersion);
}

EXPORT(int, sceAppMgrAcidDirSet) {
    TRACY_FUNC(sceAppMgrAcidDirSet);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive3) {
    TRACY_FUNC(sceAppMgrAcquireSoundOutExclusive3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAddContAddMount) {
    TRACY_FUNC(sceAppMgrAddContAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAddContMount) {
    TRACY_FUNC(sceAppMgrAddContMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppDataMount, int mountId, char *mountPoint) {
    TRACY_FUNC(sceAppMgrAppDataMount, mountId, mountPoint);
    STUBBED("Using strcpy");
    switch (mountId) {
    case 0x64: // photo0:
        strcpy(mountPoint, "ux0:picture");
        break;
    case 0x65: // psnfriend
        fmt::format_to(mountPoint, "ur0:user/{}/psnfriend{}", emuenv.io.user_id, '\0');
        break;
    case 0x66: // psnmsg
        fmt::format_to(mountPoint, "ur0:user/{}/psnmsg{}", emuenv.io.user_id, '\0');
        break;
    case 0x67: // near
        fmt::format_to(mountPoint, "ur0:user/{}/near{}", emuenv.io.user_id, '\0');
        break;
    case 0x69: // music0:
        strcpy(mountPoint, "ux0:music");
        break;
    case 0x6C: // calendar
        strcpy(mountPoint, "ux0:calendar");
        break;
    case 0x6D: // video0:
        strcpy(mountPoint, "ux0:video");
        break;
    default:
        LOG_WARN("Unknown mountId {}", log_hex(mountId));
        break;
    }

    return 0;
}

EXPORT(int, sceAppMgrAppDataMountById) {
    TRACY_FUNC(sceAppMgrAppDataMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppMount) {
    TRACY_FUNC(sceAppMgrAppMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppParamGetInt) {
    TRACY_FUNC(sceAppMgrAppParamGetInt);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrAppParamGetString, int pid, int param, char *string, int length) {
    TRACY_FUNC(sceAppMgrAppParamGetString, pid, param, string, length);
    return CALL_EXPORT(_sceAppMgrAppParamGetString, pid, param, string, length);
}

EXPORT(int, sceAppMgrAppParamSetString) {
    TRACY_FUNC(sceAppMgrAppParamSetString);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAppUmount) {
    TRACY_FUNC(sceAppMgrAppUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrBgdlGetQueueStatus) {
    TRACY_FUNC(sceAppMgrBgdlGetQueueStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCaptureFrameBufDMACByAppId) {
    TRACY_FUNC(sceAppMgrCaptureFrameBufDMACByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCaptureFrameBufIFTUByAppId) {
    TRACY_FUNC(sceAppMgrCaptureFrameBufIFTUByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrCheckRifGD) {
    TRACY_FUNC(sceAppMgrCheckRifGD);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrContentInstallPeriodStart) {
    TRACY_FUNC(sceAppMgrContentInstallPeriodStart);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrContentInstallPeriodStop) {
    TRACY_FUNC(sceAppMgrContentInstallPeriodStop);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrConvertVs0UserDrivePath, char *source_path, char *dest_path, int dest_len) {
    TRACY_FUNC(sceAppMgrConvertVs0UserDrivePath, source_path, dest_path, dest_len);
    STUBBED("Using strncpy");
    strncpy(dest_path, source_path, dest_len);
    return 0;
}

EXPORT(int, sceAppMgrDeclareShellProcess2) {
    TRACY_FUNC(sceAppMgrDeclareShellProcess2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyAppByName) {
    TRACY_FUNC(sceAppMgrDestroyAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDrmClose) {
    TRACY_FUNC(sceAppMgrDrmClose);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDrmOpen) {
    TRACY_FUNC(sceAppMgrDrmOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrForceUmount) {
    TRACY_FUNC(sceAppMgrForceUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGameDataMount) {
    TRACY_FUNC(sceAppMgrGameDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppInfo) {
    TRACY_FUNC(sceAppMgrGetAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppMgrState) {
    TRACY_FUNC(sceAppMgrGetAppMgrState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppParam) {
    TRACY_FUNC(sceAppMgrGetAppParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppParam2) {
    TRACY_FUNC(sceAppMgrGetAppParam2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetBootParam) {
    TRACY_FUNC(sceAppMgrGetBootParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetBudgetInfo) {
    TRACY_FUNC(sceAppMgrGetBudgetInfo);
    STUBBED("not system mode");
    return -1;
}

EXPORT(int, sceAppMgrGetCoredumpStateForShell) {
    TRACY_FUNC(sceAppMgrGetCoredumpStateForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetCurrentBgmState) {
    TRACY_FUNC(sceAppMgrGetCurrentBgmState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetCurrentBgmState2) {
    TRACY_FUNC(sceAppMgrGetCurrentBgmState2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetDevInfo, const char *dev, uint64_t *max_size, uint64_t *free_size) {
    TRACY_FUNC(sceAppMgrGetDevInfo, dev, max_size, free_size);
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
    TRACY_FUNC(sceAppMgrGetFgAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetIdByName) {
    TRACY_FUNC(sceAppMgrGetIdByName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMediaTypeFromDrive) {
    TRACY_FUNC(sceAppMgrGetMediaTypeFromDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMediaTypeFromDriveByPid) {
    TRACY_FUNC(sceAppMgrGetMediaTypeFromDriveByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetMountProcessNum) {
    TRACY_FUNC(sceAppMgrGetMountProcessNum);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetNameById) {
    TRACY_FUNC(sceAppMgrGetNameById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetPfsDrive) {
    TRACY_FUNC(sceAppMgrGetPfsDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetPidListForShell) {
    TRACY_FUNC(sceAppMgrGetPidListForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRawPath, char *path, char *resolved_path, int resolved_path_size) {
    TRACY_FUNC(sceAppMgrGetRawPath, resolved_path, path, resolved_path_size);
    STUBBED("Using strncpy");
    strncpy(resolved_path, path, resolved_path_size);
    return 0;
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    TRACY_FUNC(sceAppMgrGetRawPathOfApp0ByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRawPathOfApp0ByPidForShell) {
    TRACY_FUNC(sceAppMgrGetRawPathOfApp0ByPidForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRecommendedScreenOrientation) {
    TRACY_FUNC(sceAppMgrGetRecommendedScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetRunningAppIdListForShell) {
    TRACY_FUNC(sceAppMgrGetRunningAppIdListForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSaveDataInfo) {
    TRACY_FUNC(sceAppMgrGetSaveDataInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSaveDataInfoForSpecialExport) {
    TRACY_FUNC(sceAppMgrGetSaveDataInfoForSpecialExport);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusByAppId) {
    TRACY_FUNC(sceAppMgrGetStatusByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusById) {
    TRACY_FUNC(sceAppMgrGetStatusById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetStatusByName) {
    TRACY_FUNC(sceAppMgrGetStatusByName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSystemDataFilePlayReady) {
    TRACY_FUNC(sceAppMgrGetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetUserDirPath, int partition_id, char *userDirPath, SceSize path_maxlen) {
    TRACY_FUNC(sceAppMgrGetUserDirPath, partition_id, userDirPath, path_maxlen);
    STUBBED("Using strncpy");
    const auto partition = partition_id == 1 ? "ur0:" : "ux0:";
    fmt::format_to_n(userDirPath, path_maxlen, "{}user/{}{}", partition, emuenv.io.user_id, '\0');
    return 0;
}

EXPORT(int, sceAppMgrGetUserDirPathById) {
    TRACY_FUNC(sceAppMgrGetUserDirPathById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetVs0UserDataDrive) {
    TRACY_FUNC(sceAppMgrGetVs0UserDataDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetVs0UserModuleDrive) {
    TRACY_FUNC(sceAppMgrGetVs0UserModuleDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrInitSafeMemoryById) {
    TRACY_FUNC(sceAppMgrInitSafeMemoryById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrInstallDirMount) {
    TRACY_FUNC(sceAppMgrInstallDirMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsCameraActive) {
    TRACY_FUNC(sceAppMgrIsCameraActive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName) {
    TRACY_FUNC(sceAppMgrLaunchAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2) {
    TRACY_FUNC(sceAppMgrLaunchAppByName2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2ForShell) {
    TRACY_FUNC(sceAppMgrLaunchAppByName2ForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByName2ndStage) {
    TRACY_FUNC(sceAppMgrLaunchAppByName2ndStage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByNameForShell) {
    TRACY_FUNC(sceAppMgrLaunchAppByNameForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByPath4) {
    TRACY_FUNC(sceAppMgrLaunchAppByPath4);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByUri) {
    TRACY_FUNC(sceAppMgrLaunchAppByUri);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppByUri2) {
    TRACY_FUNC(sceAppMgrLaunchAppByUri2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchVideoStreamingApp) {
    TRACY_FUNC(sceAppMgrLaunchVideoStreamingApp);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam) {
    TRACY_FUNC(sceAppMgrLoadExec, appPath, argv, optParam);
    return CALL_EXPORT(_sceAppMgrLoadExec, appPath, argv, optParam);
}

EXPORT(int, sceAppMgrLoadSaveDataSystemFile) {
    TRACY_FUNC(sceAppMgrLoadSaveDataSystemFile);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLoopBackFormat) {
    TRACY_FUNC(sceAppMgrLoopBackFormat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLoopBackMount) {
    TRACY_FUNC(sceAppMgrLoopBackMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrMmsMount) {
    TRACY_FUNC(sceAppMgrMmsMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrOverwriteLaunchParamForShell) {
    TRACY_FUNC(sceAppMgrOverwriteLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPeekLaunchParamForShell) {
    TRACY_FUNC(sceAppMgrPeekLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPhotoMount) {
    TRACY_FUNC(sceAppMgrPhotoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPhotoUmount) {
    TRACY_FUNC(sceAppMgrPhotoUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataGetParams) {
    TRACY_FUNC(sceAppMgrPspSaveDataGetParams);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataRead) {
    TRACY_FUNC(sceAppMgrPspSaveDataRead);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrPspSaveDataRootMount) {
    TRACY_FUNC(sceAppMgrPspSaveDataRootMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveEvent) {
    TRACY_FUNC(sceAppMgrReceiveEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveEventNum, SceUInt32 *eventNum) {
    TRACY_FUNC(sceAppMgrReceiveEventNum, eventNum);

    // Vita3K does not yet manage events
    *eventNum = 0;

    return STUBBED("Set eventNum to 0");
}

EXPORT(int, sceAppMgrReceiveNotificationRequestForShell) {
    TRACY_FUNC(sceAppMgrReceiveNotificationRequestForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveShellEvent) {
    TRACY_FUNC(sceAppMgrReceiveShellEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveSystemEvent, SceAppMgrSystemEvent *systemEvent) {
    TRACY_FUNC(sceAppMgrReceiveSystemEvent, systemEvent);
    systemEvent->systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataAddMount) {
    TRACY_FUNC(sceAppMgrSaveDataAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataRemove, Ptr<SceAppMgrSaveDataDataDelete> data) {
    TRACY_FUNC(sceAppMgrSaveDataDataRemove, data);
    for (int i = 0; i < data.get(emuenv.mem)->fileNum; i++) {
        const auto file = fs::path(construct_savedata0_path(data.get(emuenv.mem)->files.get(emuenv.mem)[i].dataPath.get(emuenv.mem)));
        if (fs::is_regular_file(file)) {
            remove_file(emuenv.io, file.string().c_str(), emuenv.pref_path.wstring(), export_name);
        } else
            remove_dir(emuenv.io, file.string().c_str(), emuenv.pref_path.wstring(), export_name);
    }

    return 0;
}

EXPORT(int, sceAppMgrSaveDataDataRemove2) {
    TRACY_FUNC(sceAppMgrSaveDataDataRemove2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataSave, Ptr<SceAppMgrSaveDataData> data) {
    TRACY_FUNC(sceAppMgrSaveDataDataSave, data);
    SceUID fd;

    for (int i = 0; i < data.get(emuenv.mem)->fileNum; i++) {
        auto files = data.get(emuenv.mem)->files.get(emuenv.mem);

        const auto file_path = construct_savedata0_path(files[i].dataPath.get(emuenv.mem));
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY:
            create_dir(emuenv.io, file_path.c_str(), 0777, emuenv.pref_path.wstring(), export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE:
            if (files[i].buf) {
                fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path.wstring(), export_name);
                seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, emuenv.io, export_name);
                write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
                close_file(emuenv.io, fd, export_name);
            }
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_APPEND | SCE_O_TRUNC, emuenv.pref_path.wstring(), export_name);
            truncate_file(fd, files[i].bufSize + files[i].offset, emuenv.io, export_name);
            close_file(emuenv.io, fd, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE:
        default:
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path.wstring(), export_name);
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
    TRACY_FUNC(sceAppMgrSaveDataDataSave2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataGetQuota) {
    TRACY_FUNC(sceAppMgrSaveDataGetQuota);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataMount) {
    TRACY_FUNC(sceAppMgrSaveDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotCreate) {
    TRACY_FUNC(sceAppMgrSaveDataSlotCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotDelete) {
    TRACY_FUNC(sceAppMgrSaveDataSlotDelete);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileClose) {
    TRACY_FUNC(sceAppMgrSaveDataSlotFileClose);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileGetParam) {
    TRACY_FUNC(sceAppMgrSaveDataSlotFileGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotFileOpen) {
    TRACY_FUNC(sceAppMgrSaveDataSlotFileOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotGetParam) {
    TRACY_FUNC(sceAppMgrSaveDataSlotGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotGetStatus) {
    TRACY_FUNC(sceAppMgrSaveDataSlotGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotInit) {
    TRACY_FUNC(sceAppMgrSaveDataSlotInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotSetParam) {
    TRACY_FUNC(sceAppMgrSaveDataSlotSetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataSlotSetStatus) {
    TRACY_FUNC(sceAppMgrSaveDataSlotSetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataUmount) {
    TRACY_FUNC(sceAppMgrSaveDataUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendNotificationRequest) {
    TRACY_FUNC(sceAppMgrSendNotificationRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendParam) {
    TRACY_FUNC(sceAppMgrSendParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendSystemEvent) {
    TRACY_FUNC(sceAppMgrSendSystemEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendSystemEvent2) {
    TRACY_FUNC(sceAppMgrSendSystemEvent2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBackRenderPortOwner) {
    TRACY_FUNC(sceAppMgrSetBackRenderPortOwner);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmProxyApp) {
    TRACY_FUNC(sceAppMgrSetBgmProxyApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    TRACY_FUNC(sceAppMgrSetNetworkDisconnectionWarningDialogState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetPowerSaveMode) {
    TRACY_FUNC(sceAppMgrSetPowerSaveMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationForShell) {
    TRACY_FUNC(sceAppMgrSetRecommendedScreenOrientationForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetShellScreenOrientation) {
    TRACY_FUNC(sceAppMgrSetShellScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemDataFile) {
    TRACY_FUNC(sceAppMgrSetSystemDataFile);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemDataFilePlayReady) {
    TRACY_FUNC(sceAppMgrSetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamDateTimeGetConf) {
    TRACY_FUNC(sceAppMgrSystemParamDateTimeGetConf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamGetInt) {
    TRACY_FUNC(sceAppMgrSystemParamGetInt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSystemParamGetString) {
    TRACY_FUNC(sceAppMgrSystemParamGetString);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrThemeDataMount) {
    TRACY_FUNC(sceAppMgrThemeDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrTrophyMount) {
    TRACY_FUNC(sceAppMgrTrophyMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrTrophyMountById) {
    TRACY_FUNC(sceAppMgrTrophyMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUmount) {
    TRACY_FUNC(sceAppMgrUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUmountByPid) {
    TRACY_FUNC(sceAppMgrUmountByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrUpdateSaveDataParam) {
    TRACY_FUNC(sceAppMgrUpdateSaveDataParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrWorkDirMount, int mountId, char *mountPoint) {
    TRACY_FUNC(sceAppMgrWorkDirMount, mountId, mountPoint);
    STUBBED("using strcpy");
    switch (mountId) {
    case 0xC8:
        strcpy(mountPoint, "ur0:temp/sqlite/");
        break;
    case 0xC9:
        strcpy(mountPoint, "ur0:temp/attach/");
        break;
    case 0xCA:
        strcpy(mountPoint, "ux0:pspemu/");
        break;
    case 0xCC:
        strcpy(mountPoint, "ur0:temp/checkout/");
        break;
    case 0xCE:
        strcpy(mountPoint, "ur0:temp/webbrowser/");
        break;
    default:
        LOG_WARN("Unknown mount id: {}", log_hex(mountId));
        break;
    }

    return 0;
}

EXPORT(int, sceAppMgrWorkDirMountById) {
    TRACY_FUNC(sceAppMgrWorkDirMountById);
    return UNIMPLEMENTED();
}
