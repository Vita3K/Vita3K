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

#include <host/functions.h>
#include <host/load_self.h>
#include <io/vfs.h>
#include <kernel/functions.h>
#include <kernel/thread/thread_functions.h>
#include <modules/module_parent.h>
#include <util/find.h>

EXPORT(SceInt32, _sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion) {
    memset(appState, 0, sizeofSceAppMgrAppState);

    return STUBBED("Set to 0.");
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

EXPORT(int, sceAppMgrAppParamGetString, int pid, int param, char *string, int length) {
    std::string res;
    if (!sfo::get_data_by_id(res, host.sfo_handle, param)) {
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    } else {
        res.copy(string, length);
        return 0;
    }
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

EXPORT(int, sceAppMgrGetDevInfo) {
    return UNIMPLEMENTED();
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
    if (optParam)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);

    // Create exec path
    auto exec_path = static_cast<std::string>(appPath);
    if (exec_path.find("app0:/") != std::string::npos)
        exec_path.erase(0, 6);
    else
        exec_path.erase(0, 5);

    // Load exec executable
    vfs::FileBuffer exec_buffer;
    if (vfs::read_app_file(exec_buffer, host.pref_path, host.io.title_id, exec_path)) {
        Ptr<const void> exec_entry_point;
        const auto exec_id = load_self(exec_entry_point, host.kernel, host.mem, exec_buffer.data(), appPath, host.cfg);
        if (exec_id >= 0) {
            const auto exec_load = host.kernel.loaded_modules[exec_id];

            LOG_INFO("Exec executable {} ({}) loaded", exec_load->module_name, exec_path);

            auto inject = create_cpu_dep_inject(host);
            // Init exec thread
            const auto exec_thread_id = create_thread(exec_entry_point, host.kernel, host.mem, exec_load->module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, static_cast<int>(SCE_KERNEL_STACK_SIZE_USER_MAIN),
                inject, nullptr);

            if (exec_thread_id < 0) {
                LOG_ERROR("Failed to init exec thread.");
                return RET_ERROR(exec_thread_id);
            }

            // Init size of argv
            SceSize size_argv = 0;
            if (argv && argv->get(host.mem)) {
                while (argv[size_argv].get(host.mem))
                    ++size_argv;

                if (size_argv > KB(1))
                    return RET_ERROR(SCE_APPMGR_ERROR_TOO_LONG_ARGV);
            }

            // Start exec thread
            const auto exec = start_thread(host.kernel, exec_thread_id, size_argv, argv ? Ptr<void>(*argv) : Ptr<void>());
            if (exec < 0) {
                LOG_ERROR("Failed to run exec thread.");
                return RET_ERROR(exec);
            }

            LOG_INFO("Exec {} (at \"{}\") start_exec returned {}", exec_load->module_name, exec_load->path, log_hex(exec));

            // Erase current module/thread
            // TODO Unload and Erase it inside memory
            auto run_exec_thread = util::find(exec_thread_id, host.kernel.running_threads);
            host.kernel.running_threads[thread_id].swap(run_exec_thread);
            host.kernel.running_threads.erase(thread_id);

            auto exec_thread = util::find(exec_thread_id, host.kernel.threads);
            host.kernel.threads[thread_id].swap(exec_thread);
            host.kernel.threads.erase(thread_id);

            host.kernel.loaded_modules.erase(thread_id - 1);

            return 0;
        }

        return RET_ERROR(SCE_APPMGR_ERROR_STATE);
    }

    return RET_ERROR(SCE_APPMGR_ERROR_INVALID_SELF_PATH);
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

EXPORT(int, sceAppMgrSaveDataDataRemove) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataRemove2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataDataSave) {
    return UNIMPLEMENTED();
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
