// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <config/config.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/state.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <packages/sfo.h>
#include <renderer/state.h>
#include <util/tracy.h>
#include <vfs/mounts.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>

TRACY_MODULE_NAME(SceAppMgr);

struct AppMgrBgm {
    std::mutex mutex;
    int32_t active_process = -1;
    int32_t requested_priority = 0;
    int32_t sub_priority = 0;
    bool sub_priority_enabled = false;
};

LIBRARY_INIT(SceAppMgr) {
    emuenv.kernel.obj_store.create<AppMgrBgm>();
    emuenv.kernel.obj_store.create<AppMgrEvents>();
}

AppMgrBgmState get_appmgr_bgm_state(EmuEnvState &emuenv) {
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    const std::lock_guard<std::mutex> lock(bgm->mutex);
    if (bgm->active_process < 0)
        return { -1, 0xffff, 0xffff, -1, {} };
    const int32_t sub_priority = bgm->sub_priority_enabled ? bgm->sub_priority : 0;
    return { bgm->active_process, std::max(bgm->requested_priority, sub_priority), sub_priority, -1, {} };
}

static std::string appmgr_mount_log_string(const MemState &mem, Ptr<const char> input, size_t limit = 256) {
    if (!input)
        return "<null>";

    std::string result;
    for (size_t i = 0; i < limit; ++i) {
        if (i > std::numeric_limits<Address>::max() - input.address())
            return result + "<invalid>";
        const Ptr<const char> current(input.address() + static_cast<Address>(i));
        if (!current.valid(mem))
            return result + "<invalid>";
        const unsigned char ch = *current.get(mem);
        if (ch == 0)
            return result;
        result += ch >= 32 && ch < 127 ? static_cast<char>(ch) : '?';
    }
    return result + "<truncated>";
}

static bool read_mount_string(const MemState &mem, Ptr<const char> input, size_t max_length, std::string &value) {
    value.clear();
    if (!input)
        return false;
    for (size_t i = 0; i <= max_length; ++i) {
        if (i > std::numeric_limits<Address>::max() - input.address())
            return false;
        const Ptr<const char> current(input.address() + static_cast<Address>(i));
        if (!current.valid(mem))
            return false;
        const char ch = *current.get(mem);
        if (ch == '\0')
            return true;
        if (i == max_length)
            return false;
        value.push_back(ch);
    }
    return false;
}

static bool valid_mount_output(const MemState &mem, Ptr<char> output) {
    if (!output || output.address() > std::numeric_limits<Address>::max() - 15)
        return false;
    for (Address i = 0; i < 16; ++i) {
        if (!Ptr<char>(output.address() + i).valid(mem))
            return false;
    }
    return true;
}

static bool valid_mount_sfo(EmuEnvState &emuenv, const std::string &mount_point, const char *relative_sfo, const std::string &title_id, const char *export_name) {
    const std::string sfo_path = mount_point + relative_sfo;
    SceIoStat stat{};
    if (stat_file(emuenv.io, sfo_path.c_str(), &stat, emuenv.vita_fs_path, export_name) < 0
        || !(stat.st_mode & SCE_S_IFREG) || stat.st_size <= 0 || stat.st_size > 32768)
        return false;

    const SceUID fd = open_file(emuenv.io, sfo_path.c_str(), SCE_O_RDONLY, emuenv.vita_fs_path, export_name);
    if (fd < 0)
        return false;
    vfs::FileBuffer buffer(static_cast<size_t>(stat.st_size));
    const int bytes_read = read_file(buffer.data(), emuenv.io, fd, static_cast<SceSize>(buffer.size()), export_name);
    close_file(emuenv.io, fd, export_name);
    sfo::SfoAppInfo info;
    return bytes_read == static_cast<int>(buffer.size())
        && sfo::get_param_info(info, buffer, emuenv.cfg.sys_lang) && info.app_title_id == title_id;
}

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

EXPORT(int, _sceAppMgrAppDataMountById, int mountId, Ptr<const char> id, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrAppDataMountById, mountId, id.address(), mountPoint.address());
    if (mountId != 0x6e) {
        LOG_WARN("Unsupported AppDataMountById type {}", log_hex(mountId));
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string title_id;
    if (!read_mount_string(emuenv.mem, id, 32, title_id) || title_id.empty() || !valid_mount_output(emuenv.mem, mountPoint))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (title_id == "new") {
        LOG_WARN("Writable appmeta promotion is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }

    std::string mount_point;
    if (!pfsmgr::mount_readonly_appmeta(emuenv.io, emuenv.vita_fs_path, title_id, mount_point)) {
        LOG_WARN("Cannot mount appmeta for '{}'", title_id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    if (mount_point.size() >= 16 || !valid_mount_sfo(emuenv, mount_point, "/param.sfo", title_id, export_name)) {
        LOG_WARN("Invalid or encrypted appmeta SFO for '{}'", title_id);
        if (!pfsmgr::unmount_appmeta(emuenv.io, mount_point))
            LOG_ERROR("Failed to release rejected appmeta mount '{}'", mount_point);
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted appmeta with readable SFO for '{}' at '{}'", title_id, mount_point);
    return 0;
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
        uint32_t size = optParam->length;
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

EXPORT(int, _sceAppMgrDrmClose, const SceAppMgrDrmOpenParam *param) {
    TRACY_FUNC(_sceAppMgrDrmClose, param);
    if (!param || param->size != sizeof(*param)
        || !std::memchr(param->dir_name, '\0', sizeof(param->dir_name))
        || !std::memchr(param->mount_point, '\0', sizeof(param->mount_point)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    return pfsmgr::unmount_addcont(emuenv.io, param->mount_point, param->dir_name);
}

EXPORT(int, _sceAppMgrDrmOpen, const SceAppMgrDrmOpenParam *param) {
    TRACY_FUNC(_sceAppMgrDrmOpen, param);
    if (!param || param->size != sizeof(*param)
        || !std::memchr(param->dir_name, '\0', sizeof(param->dir_name))
        || !std::memchr(param->mount_point, '\0', sizeof(param->mount_point)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    return pfsmgr::mount_addcont(emuenv.io, emuenv.vita_fs_path, param->mount_point, param->dir_name);
}

EXPORT(int, _sceAppMgrForceUmount) {
    TRACY_FUNC(_sceAppMgrForceUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGameDataMount, Ptr<const char> source, Ptr<const char> patch, Ptr<const void> license, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrGameDataMount, source.address(), patch.address(), license.address(), mountPoint.address());
    std::string source_path;
    std::string patch_path;
    if (!read_mount_string(emuenv.mem, source, 0x123, source_path) || !valid_mount_output(emuenv.mem, mountPoint)
        || (patch && !read_mount_string(emuenv.mem, patch, 0x123, patch_path)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (license) {
        LOG_WARN("GameDataMount with an explicit license is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    if (source_path.starts_with("ux0:/"))
        source_path.erase(4, 1);
    if (patch_path.starts_with("ux0:/"))
        patch_path.erase(4, 1);
    if (!source_path.starts_with("ux0:app/") || source_path.size() <= 8) {
        LOG_WARN("Unsupported GameDataMount source '{}'", source_path);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    const auto title_id = source_path.substr(8);
    if (patch && patch_path != "ux0:patch/" + title_id) {
        LOG_WARN("Unsupported GameDataMount patch '{}'", patch_path);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string mount_point;
    if (!pfsmgr::mount_readonly_game_data(emuenv.io, emuenv.vita_fs_path, title_id, bool(patch), mount_point)) {
        LOG_WARN("Cannot mount game data for '{}'", title_id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    if (!valid_mount_sfo(emuenv, mount_point, "/sce_sys/param.sfo", title_id, export_name)) {
        LOG_WARN("Invalid or encrypted game data SFO for '{}'", title_id);
        if (!pfsmgr::unmount_game_data(emuenv.io, mount_point))
            LOG_ERROR("Failed to release rejected game data mount '{}'", mount_point);
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted game data for '{}' at '{}' in read-only mode", title_id, mount_point);
    return 0;
}

EXPORT(int, _sceAppMgrGetAppInfo) {
    TRACY_FUNC(_sceAppMgrGetAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppMgrState) {
    TRACY_FUNC(_sceAppMgrGetAppMgrState);
    return UNIMPLEMENTED();
}

// The size sceAppMgrGetAppParam writes at most, without a terminator if the URI fills it
constexpr size_t APP_PARAM_SIZE = 1024;

EXPORT(SceInt32, _sceAppMgrGetAppParam, char *param) {
    TRACY_FUNC(_sceAppMgrGetAppParam, param);
    if (!param)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);

    // Only the first call gets the launch URI, the ones after it get an empty string
    const std::optional<std::string> uri = emuenv.take_launch_uri();
    if (!uri) {
        param[0] = '\0';
        return SCE_APPMGR_ERROR_APP_PARAM_READ;
    }
    memcpy(param, uri->c_str(), std::min(uri->size() + 1, APP_PARAM_SIZE));
    return 0;
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

EXPORT(int, _sceAppMgrGetCurrentBgmState2, AppMgrBgmState *state) {
    TRACY_FUNC(_sceAppMgrGetCurrentBgmState2);
    return CALL_EXPORT(sceAppMgrGetCurrentBgmState2, state);
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

int appmgr_launch_app_by_path(EmuEnvState &emuenv, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const char> param, SceUInt32 process_attr, Ptr<const void> option) {
    LOG_DEBUG("LaunchAppByPath4 path: {}, title: {}, budget: {}, param: {}, process attributes: {}, option: {}",
        appmgr_mount_log_string(emuenv.mem, path, 292), appmgr_mount_log_string(emuenv.mem, title_id, 32),
        log_hex(budget_size), log_hex(param.address()), log_hex(process_attr), log_hex(option.address()));
    LOG_ERROR("LaunchAppByPath4 requires multi-process support; application was not launched");
    return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
}

EXPORT(int, _sceAppMgrLaunchAppByPath4, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const AppMgrLaunchAppByPathArgs> arguments) {
    TRACY_FUNC(_sceAppMgrLaunchAppByPath4, path, title_id, budget_size, arguments);
    if (!arguments || !is_valid_addr_len(emuenv.mem, arguments.address(), sizeof(AppMgrLaunchAppByPathArgs)))
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    AppMgrLaunchAppByPathArgs args{};
    auto *bytes = reinterpret_cast<uint8_t *>(&args);
    for (size_t i = 0; i < sizeof(args); ++i)
        bytes[i] = *Ptr<const uint8_t>(arguments.address() + static_cast<Address>(i)).get(emuenv.mem);
    return appmgr_launch_app_by_path(emuenv, path, title_id, budget_size, args.param, args.process_attr, args.option);
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

    // Create exec path
    auto exec_path = static_cast<std::string>(appPath);
    if (exec_path.contains("app0:/"))
        exec_path.erase(0, 6);
    else
        exec_path.erase(0, 5);

    LOG_INFO("sceAppMgrLoadExec run self: {}", appPath);

    // Load exec executable
    vfs::FileBuffer exec_buffer;
    const auto mounted_exec_path = "app0:/" + exec_path;
    const bool read_exec = emuenv.io.app_game_base_mount.empty()
        ? vfs::read_app_file(exec_buffer, emuenv.vita_fs_path, emuenv.io.app_path, exec_path, emuenv.io.title_id)
        : read_file_to_buffer(emuenv.io, mounted_exec_path.c_str(), emuenv.vita_fs_path, exec_buffer, export_name);
    if (read_exec) {
        std::vector<std::string> exec_argv;
        if (argv && argv->get(emuenv.mem)) {
            size_t args = 0;
            for (auto i = 0; argv[i]; i++) {
                const char *arg = argv[i].get(emuenv.mem);
                LOG_INFO("sceAppMgrLoadExec run with argument at {}: {}", i, arg);
                args += strlen(arg);
                exec_argv.emplace_back(arg);
            }

            if (args > 1024)
                return RET_ERROR(SCE_APPMGR_ERROR_TOO_LONG_ARGV);
        }

        emuenv.kernel.request_process_exit(0, AppLaunchRequest{ .app_path = emuenv.io.app_path, .self_path = std::move(exec_path), .argv = std::move(exec_argv), .reason = AppLaunchReason::LoadExec });

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

EXPORT(int, _sceAppMgrReceiveEvent, SceAppMgrEvent *event) {
    TRACY_FUNC(_sceAppMgrReceiveEvent, event);
    if (!event)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);
    STUBBED("Only launch parameter events are supported");
    auto *events = emuenv.kernel.obj_store.get<AppMgrEvents>();
    if (!events->launch_param_pending.exchange(false)) {
        event->event = 0;
        return RET_ERROR(SCE_APPMGR_ERROR_NO_EVENT);
    }
    *event = {};
    event->event = 0x20000004;
    event->app_id = -1;
    return 0;
}

EXPORT(int, _sceAppMgrReceiveEventNum, SceUInt32 *eventNum) {
    TRACY_FUNC(_sceAppMgrReceiveEventNum, eventNum);
    if (!eventNum)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);
    auto *events = emuenv.kernel.obj_store.get<AppMgrEvents>();
    *eventNum = events->launch_param_pending.load() ? 1 : 0;
    STUBBED("Only launch parameter events are supported");
    return 0;
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

EXPORT(int, _sceAppMgrThemeDataMount, Ptr<const char> contentId, Ptr<const void> option1, Ptr<const void> option2, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrThemeDataMount, contentId.address(), option1.address(), option2.address(), mountPoint.address());
    std::string id;
    if (!read_mount_string(emuenv.mem, contentId, 0x7f, id) || !valid_mount_output(emuenv.mem, mountPoint)
        || id.empty() || id == "." || id == ".." || id.find_first_of("\\/:") != std::string::npos)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (option1 || option2) {
        LOG_WARN("ThemeDataMount with options is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string mount_point;
    bool is_protected = false;
    const auto directory = emuenv.vita_fs_path / "ux0" / "theme" / fs_utils::utf8_to_path(id);
    if (!pfsmgr::mount_theme_data(emuenv.io, directory, mount_point, is_protected)) {
        LOG_WARN("Cannot mount theme data for '{}'", id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted theme data for '{}' at '{}'", id, mount_point);
    return 0;
}

EXPORT(int, _sceAppMgrTrophyMount) {
    TRACY_FUNC(_sceAppMgrTrophyMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrTrophyMountById) {
    TRACY_FUNC(_sceAppMgrTrophyMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUmount, Ptr<const char> mountPoint) {
    TRACY_FUNC(_sceAppMgrUmount, mountPoint.address());
    std::string mount_point;
    if (!read_mount_string(emuenv.mem, mountPoint, 15, mount_point))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (pfsmgr::unmount_game_data(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted game data '{}'", mount_point);
        return 0;
    }
    if (pfsmgr::unmount_appmeta(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted appmeta '{}'", mount_point);
        return 0;
    }
    if (pfsmgr::unmount_theme_data(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted theme data '{}'", mount_point);
        return 0;
    }
    if (mount_point == "ux0:picture" || mount_point == "friend0:" || mount_point == "psnmsg0:"
        || mount_point == "near0:" || mount_point == "ux0:music" || mount_point == "ux0:calendar"
        || mount_point == "ux0:video" || mount_point == "sqlite0:/" || mount_point == "attach0:/"
        || mount_point == "ux0:pspemu/" || mount_point == "checkout0:/" || mount_point == "webbrowser0:/"
        || mount_point == "ux0:mms/photo" || mount_point == "ux0:mms/music" || mount_point == "ux0:mms/video")
        return STUBBED("Unmount of legacy static path redirection");
    LOG_WARN("Unknown or unsupported AppMgr mount '{}'", mount_point);
    return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
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
    return CALL_EXPORT(sceAppMgrAcquireBgmPortWithPriority, 256);
}

EXPORT(int, sceAppMgrAcquireBgmPortForMusicPlayer) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortForMusicPlayer);
    return CALL_EXPORT(sceAppMgrAcquireBgmPortWithPriority, 130);
}

EXPORT(int, sceAppMgrAcquireBgmPortWithPriority, int priority) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortWithPriority, priority);
    if (priority < 0)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    const std::lock_guard<std::mutex> lock(bgm->mutex);
    bgm->requested_priority = priority;
    bgm->active_process = PROCESS_ID;
    return STUBBED("Single-process BGM; permissions and audio adoption are not emulated");
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

EXPORT(SceInt32, sceAppMgrLoadSafeMemory, void *buffer, SceSize size, SceOff offset) {
    TRACY_FUNC(sceAppMgrLoadSafeMemory, buffer, size, offset);
    if (!buffer)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_POINTER);
    if (offset < 0 || static_cast<uint64_t>(offset) + size > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    const auto result = execute_savedata_safe_memory(emuenv, buffer, size, offset, export_name, false);
    if (result < 0)
        return RET_ERROR(result);
    return result > 0 ? size : 0;
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
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    const std::lock_guard<std::mutex> lock(bgm->mutex);
    bgm->requested_priority = 0;
    bgm->active_process = PROCESS_ID;
    return STUBBED("Single-process BGM; audio adoption is not emulated");
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

EXPORT(SceInt32, sceAppMgrSaveSafeMemory, const void *buffer, SceSize size, SceOff offset) {
    TRACY_FUNC(sceAppMgrSaveSafeMemory, buffer, size, offset);
    if (!buffer)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_POINTER);
    if (offset < 0 || static_cast<uint64_t>(offset) + size > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    const auto result = execute_savedata_safe_memory(emuenv, const_cast<void *>(buffer), size, offset, export_name, true);
    return result < 0 ? RET_ERROR(result) : static_cast<SceInt32>(size);
}

EXPORT(int, sceAppMgrSendLiveBoardMode) {
    TRACY_FUNC(sceAppMgrSendLiveBoardMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetAppProtectionModeOnMemoryShortage) {
    TRACY_FUNC(sceAppMgrSetAppProtectionModeOnMemoryShortage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmSubPriority, int enabled, int priority) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriority, enabled, priority);
    if (priority < 0)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    const std::lock_guard<std::mutex> lock(bgm->mutex);
    bgm->sub_priority_enabled = enabled != 0;
    bgm->sub_priority = priority;
    bgm->active_process = PROCESS_ID;
    return STUBBED("Single-process BGM; permissions and audio adoption are not emulated");
}

EXPORT(int, sceAppMgrSetBgmSubPriorityForSystemChat, int enabled) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriorityForSystemChat, enabled);
    return CALL_EXPORT(sceAppMgrSetBgmSubPriority, enabled, 194);
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
