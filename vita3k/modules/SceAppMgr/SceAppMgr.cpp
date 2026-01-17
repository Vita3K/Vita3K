// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "SceAppMgr.h"

#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>
#include <kernel/state.h>
#include <packages/sfo.h>
#include <renderer/state.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(SceAppMgr);

EXPORT(SceInt32, __sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion) {
    TRACY_FUNC(__sceAppMgrGetAppState, appState, sizeofSceAppMgrAppState, buildVersion);
    memset(appState, 0, sizeofSceAppMgrAppState);

    return STUBBED("Set to 0.");
}

EXPORT(int, _sceAppMgrAcidDirSet) {
    TRACY_FUNC(_sceAppMgrAcidDirSet);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAcquireSoundOutExclusive3) {
    TRACY_FUNC(_sceAppMgrAcquireSoundOutExclusive3);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAddContAddMount) {
    TRACY_FUNC(_sceAppMgrAddContAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAddContMount) {
    TRACY_FUNC(_sceAppMgrAddContMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppDataMount) {
    TRACY_FUNC(_sceAppMgrAppDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppDataMountById) {
    TRACY_FUNC(_sceAppMgrAppDataMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppMount) {
    TRACY_FUNC(_sceAppMgrAppMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppParamGetInt) {
    TRACY_FUNC(_sceAppMgrAppParamGetInt);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, char *string, sceAppMgrAppParamGetStringOptParam *optParam) {
    TRACY_FUNC(_sceAppMgrAppParamGetString, pid, param, string, optParam);
    if (!string)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);

    if (param == 100) {
        param = 6;
        STUBBED("Use global CONTENT_ID"); // Application can set this parameter via _sceAppMgrAppParamSetString
    } else if (param == 0x65) {
        param = 9;
        STUBBED("Use global TITLE"); // Application can set this parameter via _sceAppMgrAppParamSetString
    }

    if ((param < 6) || (param > 0xe))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);

    std::string res;
    if (!sfo::get_data_by_id(res, emuenv.sfo_handle, param))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    else {
        uint32_t size = optParam->size;
        if (size > 400)
            size = 400;

        if (res.size() >= size)
            return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);

        res.copy(string, size);
        return 0;
    }
}

EXPORT(int, _sceAppMgrAppParamSetString) {
    TRACY_FUNC(_sceAppMgrAppParamSetString);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppUmount) {
    TRACY_FUNC(_sceAppMgrAppUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrBgdlGetQueueStatus) {
    TRACY_FUNC(_sceAppMgrBgdlGetQueueStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCaptureFrameBufDMACByAppId) {
    TRACY_FUNC(_sceAppMgrCaptureFrameBufDMACByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCaptureFrameBufIFTUByAppId) {
    TRACY_FUNC(_sceAppMgrCaptureFrameBufIFTUByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCheckRifGD) {
    TRACY_FUNC(_sceAppMgrCheckRifGD);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrContentInstallPeriodStart) {
    TRACY_FUNC(_sceAppMgrContentInstallPeriodStart);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrContentInstallPeriodStop) {
    TRACY_FUNC(_sceAppMgrContentInstallPeriodStop);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrConvertVs0UserDrivePath) {
    TRACY_FUNC(_sceAppMgrConvertVs0UserDrivePath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDeclareShellProcess2) {
    TRACY_FUNC(_sceAppMgrDeclareShellProcess2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDestroyAppByName) {
    TRACY_FUNC(_sceAppMgrDestroyAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDrmClose) {
    TRACY_FUNC(_sceAppMgrDrmClose);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDrmOpen) {
    TRACY_FUNC(_sceAppMgrDrmOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrForceUmount) {
    TRACY_FUNC(_sceAppMgrForceUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGameDataMount) {
    TRACY_FUNC(_sceAppMgrGameDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppInfo) {
    TRACY_FUNC(_sceAppMgrGetAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppMgrState) {
    TRACY_FUNC(_sceAppMgrGetAppMgrState);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppParam) {
    TRACY_FUNC(_sceAppMgrGetAppParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppParam2) {
    TRACY_FUNC(_sceAppMgrGetAppParam2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetBootParam) {
    TRACY_FUNC(_sceAppMgrGetBootParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetBudgetInfo) {
    TRACY_FUNC(_sceAppMgrGetBudgetInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCoredumpStateForShell) {
    TRACY_FUNC(_sceAppMgrGetCoredumpStateForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCurrentBgmState) {
    TRACY_FUNC(_sceAppMgrGetCurrentBgmState);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCurrentBgmState2) {
    TRACY_FUNC(_sceAppMgrGetCurrentBgmState2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetDevInfo) {
    TRACY_FUNC(_sceAppMgrGetDevInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetFgAppInfo) {
    TRACY_FUNC(_sceAppMgrGetFgAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetIdByName) {
    TRACY_FUNC(_sceAppMgrGetIdByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMediaTypeFromDrive) {
    TRACY_FUNC(_sceAppMgrGetMediaTypeFromDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMediaTypeFromDriveByPid) {
    TRACY_FUNC(_sceAppMgrGetMediaTypeFromDriveByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMountProcessNum) {
    TRACY_FUNC(_sceAppMgrGetMountProcessNum);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetNameById) {
    TRACY_FUNC(_sceAppMgrGetNameById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetPfsDrive) {
    TRACY_FUNC(_sceAppMgrGetPfsDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetPidListForShell) {
    TRACY_FUNC(_sceAppMgrGetPidListForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPath) {
    TRACY_FUNC(_sceAppMgrGetRawPath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    TRACY_FUNC(_sceAppMgrGetRawPathOfApp0ByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPathOfApp0ByPidForShell) {
    TRACY_FUNC(_sceAppMgrGetRawPathOfApp0ByPidForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRecommendedScreenOrientation) {
    TRACY_FUNC(_sceAppMgrGetRecommendedScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRunningAppIdListForShell) {
    TRACY_FUNC(_sceAppMgrGetRunningAppIdListForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetSaveDataInfo) {
    TRACY_FUNC(_sceAppMgrGetSaveDataInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetSaveDataInfoForSpecialExport) {
    TRACY_FUNC(_sceAppMgrGetSaveDataInfoForSpecialExport);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusByAppId) {
    TRACY_FUNC(_sceAppMgrGetStatusByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusById) {
    TRACY_FUNC(_sceAppMgrGetStatusById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusByName) {
    TRACY_FUNC(_sceAppMgrGetStatusByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetSystemDataFilePlayReady) {
    TRACY_FUNC(_sceAppMgrGetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetUserDirPath) {
    TRACY_FUNC(_sceAppMgrGetUserDirPath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetUserDirPathById) {
    TRACY_FUNC(_sceAppMgrGetUserDirPathById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetVs0UserDataDrive) {
    TRACY_FUNC(_sceAppMgrGetVs0UserDataDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetVs0UserModuleDrive) {
    TRACY_FUNC(_sceAppMgrGetVs0UserModuleDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrInitSafeMemoryById) {
    TRACY_FUNC(_sceAppMgrInitSafeMemoryById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrInstallDirMount) {
    TRACY_FUNC(_sceAppMgrInstallDirMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrIsCameraActive) {
    TRACY_FUNC(_sceAppMgrIsCameraActive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2ForShell) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2ForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2ndStage) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2ndStage);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByNameForShell) {
    TRACY_FUNC(_sceAppMgrLaunchAppByNameForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByPath4) {
    TRACY_FUNC(_sceAppMgrLaunchAppByPath4);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByUri) {
    TRACY_FUNC(_sceAppMgrLaunchAppByUri);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByUri2) {
    TRACY_FUNC(_sceAppMgrLaunchAppByUri2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchVideoStreamingApp) {
    TRACY_FUNC(_sceAppMgrLaunchVideoStreamingApp);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam) {
    TRACY_FUNC(_sceAppMgrLoadExec, appPath, argv, optParam);
    if (optParam)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);

    fs::path full_path;
    const auto app_device = device::get_device(appPath);
    if (app_device == VitaIoDevice::app0) {
        emuenv.load_exec_path = device::remove_device_from_path(appPath, app_device);
        emuenv.load_app_path = emuenv.io.app_path;
    } else {
        const std::string app_path(appPath);
        const auto first_sep = app_path.find('/');
        const auto second_sep = app_path.find('/', first_sep + 1);
        if ((first_sep == std::string::npos) || (second_sep == std::string::npos))
            return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);

        emuenv.load_app_path = app_path.substr(0, second_sep);
        emuenv.load_exec_path = app_path.substr(second_sep + 1);
    }

    const auto complete_path = emuenv.pref_path / convert_path(emuenv.load_app_path) / emuenv.load_exec_path;
    LOG_INFO("sceAppMgrLoadExec run self: {} in path: {}", appPath, complete_path.string());

    // Load exec executable
    if (fs::exists(complete_path)) {
        if (argv && argv->get(emuenv.mem)) {
            size_t args = 0;
            emuenv.load_exec_argv = "\"";
            for (auto i = 0; argv[i]; i++) {
                LOG_INFO("sceAppMgrLoadExec run with argument at {}: {}", i, argv[i].get(emuenv.mem));
                if (i)
                    emuenv.load_exec_argv += ", ";
                args += strlen(argv[i].get(emuenv.mem));
                emuenv.load_exec_argv += argv[i].get(emuenv.mem);
            }
            emuenv.load_exec_argv += "\"";

            if (args > 1024)
                return RET_ERROR(SCE_APPMGR_ERROR_TOO_LONG_ARGV);
        }

        emuenv.kernel.exit_delete_all_threads();

        emuenv.load_exec = true;
        // make sure we are not stuck waiting for a gpu command
        emuenv.renderer->should_display = true;

        return SCE_KERNEL_OK;
    }

    return RET_ERROR(SCE_APPMGR_ERROR_INVALID_SELF_PATH);
}

EXPORT(int, _sceAppMgrLoadSaveDataSystemFile) {
    TRACY_FUNC(_sceAppMgrLoadSaveDataSystemFile);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLoopBackFormat) {
    TRACY_FUNC(_sceAppMgrLoopBackFormat);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLoopBackMount) {
    TRACY_FUNC(_sceAppMgrLoopBackMount);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceAppMgrMmsMount, SceInt32 id, char *mount_point) {
    TRACY_FUNC(_sceAppMgrMmsMount, id, mount_point);
    switch (id) {
    case 0x190:
        strcpy(mount_point, "ux0:mms/photo");
        break;
    case 0x191:
        strcpy(mount_point, "ux0:mms/music");
        break;
    case 0x192:
        strcpy(mount_point, "ux0:mms/video");
        break;
    default:
        LOG_ERROR("Unknown id: {}", log_hex(id));
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    }

    return STUBBED("using strcpy");
}

EXPORT(int, _sceAppMgrOverwriteLaunchParamForShell) {
    TRACY_FUNC(_sceAppMgrOverwriteLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPeekLaunchParamForShell) {
    TRACY_FUNC(_sceAppMgrPeekLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPhotoMount) {
    TRACY_FUNC(_sceAppMgrPhotoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPhotoUmount) {
    TRACY_FUNC(_sceAppMgrPhotoUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataGetParams) {
    TRACY_FUNC(_sceAppMgrPspSaveDataGetParams);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataRead) {
    TRACY_FUNC(_sceAppMgrPspSaveDataRead);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataRootMount) {
    TRACY_FUNC(_sceAppMgrPspSaveDataRootMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveEvent) {
    TRACY_FUNC(_sceAppMgrReceiveEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveEventNum) {
    TRACY_FUNC(_sceAppMgrReceiveEventNum);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveNotificationRequestForShell) {
    TRACY_FUNC(_sceAppMgrReceiveNotificationRequestForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveShellEvent) {
    TRACY_FUNC(_sceAppMgrReceiveShellEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveSystemEvent, SceAppMgrSystemEvent *systemEvent) {
    TRACY_FUNC(_sceAppMgrReceiveSystemEvent, systemEvent);
    systemEvent->systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataAddMount) {
    TRACY_FUNC(_sceAppMgrSaveDataAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataRemove) {
    TRACY_FUNC(_sceAppMgrSaveDataDataRemove);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataRemove2) {
    TRACY_FUNC(_sceAppMgrSaveDataDataRemove2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataSave) {
    TRACY_FUNC(_sceAppMgrSaveDataDataSave);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataSave2) {
    TRACY_FUNC(_sceAppMgrSaveDataDataSave2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataGetQuota) {
    TRACY_FUNC(_sceAppMgrSaveDataGetQuota);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataMount) {
    TRACY_FUNC(_sceAppMgrSaveDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotCreate) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotDelete) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotDelete);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileClose) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileClose);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileGetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileOpen) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotGetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotGetStatus) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotInit) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotInit);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotSetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotSetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotSetStatus) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotSetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataUmount) {
    TRACY_FUNC(_sceAppMgrSaveDataUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendNotificationRequest) {
    TRACY_FUNC(_sceAppMgrSendNotificationRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendParam) {
    TRACY_FUNC(_sceAppMgrSendParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendSystemEvent) {
    TRACY_FUNC(_sceAppMgrSendSystemEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendSystemEvent2) {
    TRACY_FUNC(_sceAppMgrSendSystemEvent2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetBackRenderPortOwner) {
    TRACY_FUNC(_sceAppMgrSetBackRenderPortOwner);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetBgmProxyApp) {
    TRACY_FUNC(_sceAppMgrSetBgmProxyApp);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    TRACY_FUNC(_sceAppMgrSetNetworkDisconnectionWarningDialogState);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetPowerSaveMode) {
    TRACY_FUNC(_sceAppMgrSetPowerSaveMode);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetRecommendedScreenOrientationForShell) {
    TRACY_FUNC(_sceAppMgrSetRecommendedScreenOrientationForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetShellScreenOrientation) {
    TRACY_FUNC(_sceAppMgrSetShellScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetSystemDataFile) {
    TRACY_FUNC(_sceAppMgrSetSystemDataFile);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetSystemDataFilePlayReady) {
    TRACY_FUNC(_sceAppMgrSetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamDateTimeGetConf) {
    TRACY_FUNC(_sceAppMgrSystemParamDateTimeGetConf);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamGetInt) {
    TRACY_FUNC(_sceAppMgrSystemParamGetInt);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamGetString) {
    TRACY_FUNC(_sceAppMgrSystemParamGetString);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrThemeDataMount) {
    TRACY_FUNC(_sceAppMgrThemeDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrTrophyMount) {
    TRACY_FUNC(_sceAppMgrTrophyMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrTrophyMountById) {
    TRACY_FUNC(_sceAppMgrTrophyMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUmount) {
    TRACY_FUNC(_sceAppMgrUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUmountByPid) {
    TRACY_FUNC(_sceAppMgrUmountByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUpdateSaveDataParam) {
    TRACY_FUNC(_sceAppMgrUpdateSaveDataParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrWorkDirMount) {
    TRACY_FUNC(_sceAppMgrWorkDirMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrWorkDirMountById) {
    TRACY_FUNC(_sceAppMgrWorkDirMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireBgmPort) {
    TRACY_FUNC(sceAppMgrAcquireBgmPort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireBgmPortForMusicPlayer) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortForMusicPlayer);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireBgmPortWithPriority) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortWithPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireBtrm) {
    TRACY_FUNC(sceAppMgrAcquireBtrm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive) {
    TRACY_FUNC(sceAppMgrAcquireSoundOutExclusive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive2) {
    TRACY_FUNC(sceAppMgrAcquireSoundOutExclusive2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrActivateApp) {
    TRACY_FUNC(sceAppMgrActivateApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDeactivateApp) {
    TRACY_FUNC(sceAppMgrDeactivateApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDeclareSystemChatApp) {
    TRACY_FUNC(sceAppMgrDeclareSystemChatApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyAppByAppId) {
    TRACY_FUNC(sceAppMgrDestroyAppByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherApp) {
    TRACY_FUNC(sceAppMgrDestroyOtherApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherAppByAppIdForShell) {
    TRACY_FUNC(sceAppMgrDestroyOtherAppByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherAppByPidForShell) {
    TRACY_FUNC(sceAppMgrDestroyOtherAppByPidForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDump) {
    TRACY_FUNC(sceAppMgrDump);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnableCoredumpForTest) {
    TRACY_FUNC(sceAppMgrEnableCoredumpForTest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnableDuckingOnSystemChat) {
    TRACY_FUNC(sceAppMgrEnableDuckingOnSystemChat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnablePrioritizingSystemChat) {
    TRACY_FUNC(sceAppMgrEnablePrioritizingSystemChat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrExitToLiveboardForGameApp) {
    TRACY_FUNC(sceAppMgrExitToLiveboardForGameApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrFinishCoredumpForShell) {
    TRACY_FUNC(sceAppMgrFinishCoredumpForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppIdByAppId) {
    TRACY_FUNC(sceAppMgrGetAppIdByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetExtraAppParam) {
    TRACY_FUNC(sceAppMgrGetExtraAppParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetProcessIdByAppIdForShell) {
    TRACY_FUNC(sceAppMgrGetProcessIdByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetSystemDataFile) {
    TRACY_FUNC(sceAppMgrGetSystemDataFile);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGrowMemory) {
    TRACY_FUNC(sceAppMgrGrowMemory);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGrowMemory3) {
    TRACY_FUNC(sceAppMgrGrowMemory3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsDevelopmentMode) {
    TRACY_FUNC(sceAppMgrIsDevelopmentMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsGameBudgetAppPresent) {
    TRACY_FUNC(sceAppMgrIsGameBudgetAppPresent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsGameProgram) {
    TRACY_FUNC(sceAppMgrIsGameProgram);
    return 0;
}

EXPORT(int, sceAppMgrIsNonGameProgram) {
    TRACY_FUNC(sceAppMgrIsNonGameProgram);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsOtherAppPresent) {
    TRACY_FUNC(sceAppMgrIsOtherAppPresent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsPidShellAndCrashed) {
    TRACY_FUNC(sceAppMgrIsPidShellAndCrashed);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsPsNowClient) {
    TRACY_FUNC(sceAppMgrIsPsNowClient);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppCancel) {
    TRACY_FUNC(sceAppMgrLaunchAppCancel);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLoadSafeMemory) {
    TRACY_FUNC(sceAppMgrLoadSafeMemory);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrNotifyLiveBoardModeForShell) {
    TRACY_FUNC(sceAppMgrNotifyLiveBoardModeForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrQuitApp) {
    TRACY_FUNC(sceAppMgrQuitApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrQuitForNonSuspendableApp) {
    TRACY_FUNC(sceAppMgrQuitForNonSuspendableApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveShellEventNum) {
    TRACY_FUNC(sceAppMgrReceiveShellEventNum);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseBgmPort) {
    TRACY_FUNC(sceAppMgrReleaseBgmPort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseBtrm) {
    TRACY_FUNC(sceAppMgrReleaseBtrm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive2) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive3) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrRestoreBgmSettingForShell) {
    TRACY_FUNC(sceAppMgrRestoreBgmSettingForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrRestoreDisplaySettingForShell) {
    TRACY_FUNC(sceAppMgrRestoreDisplaySettingForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrResumeBgAppByShell) {
    TRACY_FUNC(sceAppMgrResumeBgAppByShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReturnLiveAreaOperationResultForShell) {
    TRACY_FUNC(sceAppMgrReturnLiveAreaOperationResultForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataGetCachedRequiredSizeKiB) {
    TRACY_FUNC(sceAppMgrSaveDataGetCachedRequiredSizeKiB);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveSafeMemory) {
    TRACY_FUNC(sceAppMgrSaveSafeMemory);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSendLiveBoardMode) {
    TRACY_FUNC(sceAppMgrSendLiveBoardMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetAppProtectionModeOnMemoryShortage) {
    TRACY_FUNC(sceAppMgrSetAppProtectionModeOnMemoryShortage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmSubPriority) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmSubPriorityForSystemChat) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriorityForSystemChat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetDisplayMergeConf) {
    TRACY_FUNC(sceAppMgrSetDisplayMergeConf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetFakeSettingBug51800) {
    TRACY_FUNC(sceAppMgrSetFakeSettingBug51800);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarState) {
    TRACY_FUNC(sceAppMgrSetInfobarState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarStateForCommonDialog) {
    TRACY_FUNC(sceAppMgrSetInfobarStateForCommonDialog);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarStateForShellByAppId) {
    TRACY_FUNC(sceAppMgrSetInfobarStateForShellByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationActivated) {
    TRACY_FUNC(sceAppMgrSetRecommendedScreenOrientationActivated);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemImposeState) {
    TRACY_FUNC(sceAppMgrSetSystemImposeState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemImposeState2) {
    TRACY_FUNC(sceAppMgrSetSystemImposeState2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSuspendBgAppByShell) {
    TRACY_FUNC(sceAppMgrSuspendBgAppByShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSuspendUntilActivated) {
    TRACY_FUNC(sceAppMgrSuspendUntilActivated);
    return UNIMPLEMENTED();
}
