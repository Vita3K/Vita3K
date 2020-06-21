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

#include "SceAppUtil.h"

#include <host/app_util.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/vfs.h>

#include <cstring>

#define SCE_SYSTEM_PARAM_USERNAME_MAXSIZE 17

enum SceSystemParamId {
    SCE_SYSTEM_PARAM_ID_LANG = 1,
    SCE_SYSTEM_PARAM_ID_ENTER_BUTTON,
    SCE_SYSTEM_PARAM_ID_USERNAME,
    SCE_SYSTEM_PARAM_ID_DATE_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_ZONE,
    SCE_SYSTEM_PARAM_ID_DAYLIGHT_SAVINGS,
    SCE_SYSTEM_PARAM_ID_MAX_VALUE = 0xFFFFFFFF
};

enum SceAppUtilSaveDataRemoveMode {
    SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT = 0,
    SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_NO_SLOT = 1
};

enum SceAppUtilSaveDataSaveMode {
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE = 0,
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE = 1,
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY = 2
};

enum SceAppUtilErrorCode {
    SCE_APPUTIL_ERROR_PARAMETER = 0x80100600,
    SCE_APPUTIL_ERROR_NOT_INITIALIZED = 0x80100601,
    SCE_APPUTIL_ERROR_NO_MEMORY = 0x80100602,
    SCE_APPUTIL_ERROR_BUSY = 0x80100603,
    SCE_APPUTIL_ERROR_NOT_MOUNTED = 0x80100604,
    SCE_APPUTIL_ERROR_NO_PERMISSION = 0x80100605,
    SCE_APPUTIL_ERROR_PASSCODE_MISMATCH = 0x80100606,
    SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA = 0x80100620,
    SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS = 0x80100640,
    SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND = 0x80100641,
    SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_QUOTA = 0x80100642,
    SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS = 0x80100643,
    SCE_APPUTIL_ERROR_DRM_NO_ENTITLEMENT = 0x80100660,
    SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND = 0x80100680,
    SCE_APPUTIL_ERROR_MUSIC_DEVICE_NOT_FOUND = 0x80100685,
    SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER = 0x80100686,
    SCE_APPUTIL_ERROR_STACKSIZE_TOO_SHORT = 0x801006A0
};

struct SceAppUtilSaveDataMountPoint {
    uint8_t data[16];
};

EXPORT(int, sceAppUtilAddCookieWebBrowser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseGameCustomData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseIncomingDialog) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseLiveArea) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNearGift) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpActivity) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpAppDataMessage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpBasicJoinablePresence) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpInviteMessage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseScreenShotNotification) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseSessionInvitation) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTeleport) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTriggerUtil) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseWebBrowser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppParamGetInt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilBgdlGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilDrmClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilDrmOpen, SceAppUtilDrmAddcontId *dirName, SceAppUtilMountPoint *mountPoint) {
    const auto drm_content_id_path{ fs::path(host.pref_path) / (+VitaIoDevice::ux0)._to_string() / host.io.device_paths.addcont0 / reinterpret_cast<char *>(dirName->data) };

    if (dirName == nullptr)
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_INITIALIZED);

    if (!fs::exists(drm_content_id_path) || (fs::is_empty(drm_content_id_path)))
        return SCE_ERROR_ERRNO_ENOENT;

    return 0;
}

EXPORT(int, sceAppUtilInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilLaunchWebBrowser) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilLoadSafeMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataGetDirNameList) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataLoad) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilReceiveAppEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilResetCookieWebBrowser) {
    return UNIMPLEMENTED();
}

static std::string construct_savedata0_path(const std::string &data, const char *ext = "") {
    return device::construct_normalized_path(VitaIoDevice::savedata0, data, ext);
}

std::string construct_slotparam_path(const unsigned int data) {
    return construct_savedata0_path("SlotParam_" + std::to_string(data), "bin");
}

EXPORT(int, sceAppUtilSaveDataDataRemove, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataRemoveItem *files, unsigned int fileNum, SceAppUtilSaveDataMountPoint *mountPoint) {
    for (unsigned int i = 0; i < fileNum; i++) {
        remove_file(host.io, construct_savedata0_path(files[i].dataPath.get(host.mem)).c_str(), host.pref_path, export_name);
    }

    if (slot && files[0].mode == SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT) {
        remove_file(host.io, construct_slotparam_path(slot->id).c_str(), host.pref_path, export_name);
    }
    return 0;
}

EXPORT(int, sceAppUtilSaveDataDataSave, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataDataSaveItem *files, unsigned int fileNum, SceAppUtilSaveDataMountPoint *mountPoint, SceSize *requiredSizeKB) {
    SceUID fd;

    for (unsigned int i = 0; i < fileNum; i++) {
        const auto file_path = construct_savedata0_path(files[i].dataPath.get(host.mem));
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY:
            create_dir(host.io, file_path.c_str(), 0777, host.pref_path, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE:
            fd = open_file(host.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_APPEND | SCE_O_TRUNC, host.pref_path, export_name);
            truncate_file(fd, files[i].bufSize + files[i].offset, host.io, export_name);
            close_file(host.io, fd, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE:
        default:
            fd = open_file(host.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, host.pref_path, export_name);
            seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, host.io, export_name);
            write_file(fd, files[i].buf.get(host.mem), files[i].bufSize, host.io, export_name);
            close_file(host.io, fd, export_name);
            break;
        }
    }

    if (slot && slot->slotParam) {
        SceDateTime modified_time;
        std::time_t time = std::time(0);
        tm *local = localtime(&time);
        modified_time.year = local->tm_year + 1900;
        modified_time.month = local->tm_mon + 1;
        modified_time.day = local->tm_mday;
        modified_time.hour = local->tm_hour;
        modified_time.minute = local->tm_min;
        modified_time.second = local->tm_sec;
        slot->slotParam.get(host.mem)->modifiedTime = modified_time;
        fd = open_file(host.io, construct_slotparam_path(slot->id).c_str(), SCE_O_WRONLY, host.pref_path, export_name);
        write_file(fd, slot->slotParam.get(host.mem), sizeof(SceAppUtilSaveDataSlotParam), host.io, export_name);
        close_file(host.io, fd, export_name);
    }

    return 0;
}

EXPORT(int, sceAppUtilSaveDataGetQuota, SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint) {
    *quotaSizeKiB = vfs::get_space_info(VitaIoDevice::ux0, host.io.device_paths.savedata0, host.pref_path).max_capacity / KB(1);
    *usedSizeKiB = vfs::get_space_info(VitaIoDevice::ux0, host.io.device_paths.savedata0, host.pref_path).used / KB(1);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataMount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSaveDataSlotCreate, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    const auto fd = open_file(host.io, construct_slotparam_path(slotId).c_str(), SCE_O_WRONLY | SCE_O_CREAT, host.pref_path, export_name);
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), host.io, export_name);
    close_file(host.io, fd, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotDelete, unsigned int slotId, SceAppUtilSaveDataMountPoint *mountPoint) {
    remove_file(host.io, construct_slotparam_path(slotId).c_str(), host.pref_path, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotGetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    const auto fd = open_file(host.io, construct_slotparam_path(slotId).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
    if (fd < 0)
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    read_file(param, host.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
    close_file(host.io, fd, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotSearch) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSaveDataSlotSetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    const auto fd = open_file(host.io, construct_slotparam_path(slotId).c_str(), SCE_O_WRONLY, host.pref_path, export_name);
    if (fd < 0)
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), host.io, export_name);
    close_file(host.io, fd, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataUmount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSaveSafeMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilShutdown) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilStoreBrowse) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSystemParamGetInt, unsigned int paramId, int *value) {
    const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
    const auto sys_button = static_cast<SceSystemParamEnterButtonAssign>(host.cfg.sys_button);

    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        *value = sys_lang;
        return 0;
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        *value = sys_button;
        return 0;
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
}

EXPORT(int, sceAppUtilSystemParamGetString, unsigned int paramId, SceChar8 *buf, SceSize bufSize) {
    constexpr auto devname_len = SCE_SYSTEM_PARAM_USERNAME_MAXSIZE;
    char devname[devname_len];
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_USERNAME:
        if (gethostname(devname, devname_len)) {
            // fallback to "Vita3k"
            std::strncpy(devname, "Vita3k", sizeof(devname));
        }
        std::strncpy(reinterpret_cast<char *>(buf), devname, sizeof(devname));
        break;
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
    return 0;
}

BRIDGE_IMPL(sceAppUtilAddCookieWebBrowser)
BRIDGE_IMPL(sceAppUtilAddcontMount)
BRIDGE_IMPL(sceAppUtilAddcontUmount)
BRIDGE_IMPL(sceAppUtilAppEventParseGameCustomData)
BRIDGE_IMPL(sceAppUtilAppEventParseIncomingDialog)
BRIDGE_IMPL(sceAppUtilAppEventParseLiveArea)
BRIDGE_IMPL(sceAppUtilAppEventParseNearGift)
BRIDGE_IMPL(sceAppUtilAppEventParseNpActivity)
BRIDGE_IMPL(sceAppUtilAppEventParseNpAppDataMessage)
BRIDGE_IMPL(sceAppUtilAppEventParseNpBasicJoinablePresence)
BRIDGE_IMPL(sceAppUtilAppEventParseNpInviteMessage)
BRIDGE_IMPL(sceAppUtilAppEventParseScreenShotNotification)
BRIDGE_IMPL(sceAppUtilAppEventParseSessionInvitation)
BRIDGE_IMPL(sceAppUtilAppEventParseTeleport)
BRIDGE_IMPL(sceAppUtilAppEventParseTriggerUtil)
BRIDGE_IMPL(sceAppUtilAppEventParseWebBrowser)
BRIDGE_IMPL(sceAppUtilAppParamGetInt)
BRIDGE_IMPL(sceAppUtilBgdlGetStatus)
BRIDGE_IMPL(sceAppUtilDrmClose)
BRIDGE_IMPL(sceAppUtilDrmOpen)
BRIDGE_IMPL(sceAppUtilInit)
BRIDGE_IMPL(sceAppUtilLaunchWebBrowser)
BRIDGE_IMPL(sceAppUtilLoadSafeMemory)
BRIDGE_IMPL(sceAppUtilMusicMount)
BRIDGE_IMPL(sceAppUtilMusicUmount)
BRIDGE_IMPL(sceAppUtilPhotoMount)
BRIDGE_IMPL(sceAppUtilPhotoUmount)
BRIDGE_IMPL(sceAppUtilPspSaveDataGetDirNameList)
BRIDGE_IMPL(sceAppUtilPspSaveDataLoad)
BRIDGE_IMPL(sceAppUtilReceiveAppEvent)
BRIDGE_IMPL(sceAppUtilResetCookieWebBrowser)
BRIDGE_IMPL(sceAppUtilSaveDataDataRemove)
BRIDGE_IMPL(sceAppUtilSaveDataDataSave)
BRIDGE_IMPL(sceAppUtilSaveDataGetQuota)
BRIDGE_IMPL(sceAppUtilSaveDataMount)
BRIDGE_IMPL(sceAppUtilSaveDataSlotCreate)
BRIDGE_IMPL(sceAppUtilSaveDataSlotDelete)
BRIDGE_IMPL(sceAppUtilSaveDataSlotGetParam)
BRIDGE_IMPL(sceAppUtilSaveDataSlotSearch)
BRIDGE_IMPL(sceAppUtilSaveDataSlotSetParam)
BRIDGE_IMPL(sceAppUtilSaveDataUmount)
BRIDGE_IMPL(sceAppUtilSaveSafeMemory)
BRIDGE_IMPL(sceAppUtilShutdown)
BRIDGE_IMPL(sceAppUtilStoreBrowse)
BRIDGE_IMPL(sceAppUtilSystemParamGetInt)
BRIDGE_IMPL(sceAppUtilSystemParamGetString)
