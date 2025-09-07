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

#include <module/module.h>

#include <emuenv/app_util.h>

#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/vfs.h>

#include <packages/license.h>
#include <packages/sfo.h>

#include <util/safe_time.h>
#include <util/tracy.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <unistd.h>
#endif

#include <cstring>

TRACY_MODULE_NAME(SceAppUtil);

template <>
std::string to_debug_str<SceSystemParamId>(const MemState &mem, SceSystemParamId type) {
    switch (type) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        return "SCE_SYSTEM_PARAM_ID_LANG";
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        return "SCE_SYSTEM_PARAM_ID_ENTER_BUTTON";
    case SCE_SYSTEM_PARAM_ID_USER_NAME:
        return "SCE_SYSTEM_PARAM_ID_USER_NAME";
    case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
        return "SCE_SYSTEM_PARAM_ID_DATE_FORMAT";
    case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
        return "SCE_SYSTEM_PARAM_ID_TIME_FORMAT";
    case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
        return "SCE_SYSTEM_PARAM_ID_TIME_ZONE";
    case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
        return "SCE_SYSTEM_PARAM_ID_SUMMERTIME";
    case SCE_SYSTEM_PARAM_ID_MAX_VALUE:
        return "SCE_SYSTEM_PARAM_ID_MAX_VALUE";
    }
    return std::to_string(type);
}

EXPORT(int, sceAppUtilAddCookieWebBrowser) {
    TRACY_FUNC(sceAppUtilAddCookieWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontMount) {
    TRACY_FUNC(sceAppUtilAddcontMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontUmount) {
    TRACY_FUNC(sceAppUtilAddcontUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseGameCustomData) {
    TRACY_FUNC(sceAppUtilAppEventParseGameCustomData);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseIncomingDialog) {
    TRACY_FUNC(sceAppUtilAppEventParseIncomingDialog);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseLiveArea) {
    TRACY_FUNC(sceAppUtilAppEventParseLiveArea);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNearGift) {
    TRACY_FUNC(sceAppUtilAppEventParseNearGift);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpActivity) {
    TRACY_FUNC(sceAppUtilAppEventParseNpActivity);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpAppDataMessage) {
    TRACY_FUNC(sceAppUtilAppEventParseNpAppDataMessage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpBasicJoinablePresence) {
    TRACY_FUNC(sceAppUtilAppEventParseNpBasicJoinablePresence);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpInviteMessage) {
    TRACY_FUNC(sceAppUtilAppEventParseNpInviteMessage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseScreenShotNotification) {
    TRACY_FUNC(sceAppUtilAppEventParseScreenShotNotification);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseSessionInvitation) {
    TRACY_FUNC(sceAppUtilAppEventParseSessionInvitation);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTeleport) {
    TRACY_FUNC(sceAppUtilAppEventParseTeleport);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTriggerUtil) {
    TRACY_FUNC(sceAppUtilAppEventParseTriggerUtil);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseWebBrowser) {
    TRACY_FUNC(sceAppUtilAppEventParseWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppUtilAppParamGetInt, SceAppUtilAppParamId paramId, SceInt32 *value) {
    TRACY_FUNC(sceAppUtilAppParamGetInt, paramId, value);
    if (paramId != SCE_APPUTIL_APPPARAM_ID_SKU_FLAG)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (!value)
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_INITIALIZED);

    *value = emuenv.license.rif[emuenv.io.title_id].sku_flag;

    return 0;
}

EXPORT(int, sceAppUtilBgdlGetStatus) {
    TRACY_FUNC(sceAppUtilBgdlGetStatus);
    return UNIMPLEMENTED();
}

static bool is_addcont_exist(EmuEnvState &emuenv, const SceChar8 *path) {
    const auto drm_content_id_path{ emuenv.pref_path / (+VitaIoDevice::ux0)._to_string() / emuenv.io.device_paths.addcont0 / reinterpret_cast<const char *>(path) };
    return (fs::exists(drm_content_id_path) && (!fs::is_empty(drm_content_id_path)));
}

EXPORT(SceInt32, sceAppUtilDrmClose, const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilDrmClose, dirName, mountPoint);
    if (!dirName)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (!is_addcont_exist(emuenv, dirName->data))
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_MOUNTED);

    return 0;
}

EXPORT(SceInt32, sceAppUtilDrmOpen, const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilDrmOpen, dirName, mountPoint);
    if (!dirName)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (!is_addcont_exist(emuenv, dirName->data))
        return SCE_ERROR_ERRNO_ENOENT;

    return 0;
}

EXPORT(int, sceAppUtilInit, void *initParam, void *bootParam) {
    TRACY_FUNC(sceAppUtilInit, initParam, bootParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilLaunchWebBrowser) {
    TRACY_FUNC(sceAppUtilLaunchWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicMount) {
    TRACY_FUNC(sceAppUtilMusicMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicUmount) {
    TRACY_FUNC(sceAppUtilMusicUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoMount) {
    TRACY_FUNC(sceAppUtilPhotoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoUmount) {
    TRACY_FUNC(sceAppUtilPhotoUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataGetDirNameList) {
    TRACY_FUNC(sceAppUtilPspSaveDataGetDirNameList);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataLoad) {
    TRACY_FUNC(sceAppUtilPspSaveDataLoad);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilReceiveAppEvent) {
    TRACY_FUNC(sceAppUtilReceiveAppEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilResetCookieWebBrowser) {
    TRACY_FUNC(sceAppUtilResetCookieWebBrowser);
    return UNIMPLEMENTED();
}

std::string construct_savedata0_path(const std::string &data, const char *ext) {
    return device::construct_normalized_path(VitaIoDevice::savedata0, data, ext);
}

std::string construct_slotparam_path(const unsigned int data) {
    return construct_savedata0_path("SlotParam_" + std::to_string(data), "bin");
}

EXPORT(int, sceAppUtilSaveDataDataRemove, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataRemoveItem *files, unsigned int fileNum, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataDataRemove, slot, files, fileNum, mountPoint);
    for (unsigned int i = 0; i < fileNum; i++) {
        const auto file = fs::path(construct_savedata0_path(files[i].dataPath.get(emuenv.mem)));
        if (fs::is_regular_file(file)) {
            remove_file(emuenv.io, file.string().c_str(), emuenv.pref_path, export_name);
        } else
            remove_dir(emuenv.io, file.string().c_str(), emuenv.pref_path, export_name);
    }

    if (slot && files[0].mode == SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT) {
        remove_file(emuenv.io, construct_slotparam_path(slot->id).c_str(), emuenv.pref_path, export_name);
    }

    return 0;
}

EXPORT(int, sceAppUtilSaveDataDataSave, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataDataSaveItem *files, unsigned int fileNum, SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB) {
    TRACY_FUNC(sceAppUtilSaveDataDataSave, slot, files, fileNum, mountPoint, requiredSizeKiB);
    SceUID fd;

    if (requiredSizeKiB)
        // requiredSizeKiB must be set to 0 if there is enough space available
        *requiredSizeKiB = 0;

    for (unsigned int i = 0; i < fileNum; i++) {
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
            write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
            close_file(emuenv.io, fd, export_name);
            break;
        }
    }

    if (slot && slot->slotParam) {
        SceDateTime modified_time;
        std::time_t time = std::time(0);
        tm local = {};

        SAFE_LOCALTIME(&time, &local);
        modified_time.year = local.tm_year + 1900;
        modified_time.month = local.tm_mon + 1;
        modified_time.day = local.tm_mday;
        modified_time.hour = local.tm_hour;
        modified_time.minute = local.tm_min;
        modified_time.second = local.tm_sec;
        slot->slotParam.get(emuenv.mem)->modifiedTime = modified_time;
        fd = open_file(emuenv.io, construct_slotparam_path(slot->id).c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path, export_name);
        write_file(fd, slot->slotParam.get(emuenv.mem), sizeof(SceAppUtilSaveDataSlotParam), emuenv.io, export_name);
        close_file(emuenv.io, fd, export_name);
    }

    return 0;
}

EXPORT(int, sceAppUtilSaveDataGetQuota, SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataGetQuota, quotaSizeKiB, usedSizeKiB, mountPoint);

    if (!quotaSizeKiB && !usedSizeKiB)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    // Quota from SFO
    if (quotaSizeKiB) {
        std::string savedata_max_size;

        if (!sfo::get_data_by_key(savedata_max_size, emuenv.sfo_handle, "SAVEDATA_MAX_SIZE"))
            savedata_max_size = "0";

        *quotaSizeKiB = static_cast<SceSize>(std::strtoul(savedata_max_size.c_str(), nullptr, 10));
    }

    // Used size from VFS
    if (usedSizeKiB) {
        *usedSizeKiB = vfs::get_directory_used_size(VitaIoDevice::ux0, emuenv.io.device_paths.savedata0, emuenv.pref_path) / KiB(1);

        // Clamp used size to quota
        if (quotaSizeKiB && (*quotaSizeKiB > 0) && (*usedSizeKiB > *quotaSizeKiB))
            *usedSizeKiB = *quotaSizeKiB;
    }

    return 0;
}

EXPORT(int, sceAppUtilSaveDataMount) {
    TRACY_FUNC(sceAppUtilSaveDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSaveDataSlotCreate, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotCreate, slotId, param, mountPoint);
    const auto fd = open_file(emuenv.io, construct_slotparam_path(slotId).c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path, export_name);
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), emuenv.io, export_name);
    close_file(emuenv.io, fd, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotDelete, unsigned int slotId, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotDelete, slotId, mountPoint);
    remove_file(emuenv.io, construct_slotparam_path(slotId).c_str(), emuenv.pref_path, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotGetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotGetParam, slotId, param, mountPoint);
    const auto fd = open_file(emuenv.io, construct_slotparam_path(slotId).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
    if (fd < 0)
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    read_file(param, emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
    close_file(emuenv.io, fd, export_name);
    param->status = 0;
    return 0;
}

EXPORT(SceInt32, sceAppUtilSaveDataSlotSearch, SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
    SceAppUtilSlotSearchResult *result, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotSearch, workBuf, cond, result, mountPoint);
    STUBBED("No sort slot list");

    if (!cond || !result)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (workBuf)
        result->slotList = Ptr<SceAppUtilSaveDataSlot>(workBuf->buf.address());

    result->hitNum = 0;
    auto slotList = result->slotList.get(emuenv.mem);
    for (auto i = cond->from; i < (cond->from + cond->range); i++) {
        if (slotList) {
            slotList[i].id = -1;
            slotList[i].status = 0;
            slotList[i].userParam = 0;
            slotList[i].emptyParam = Ptr<SceAppUtilSaveDataSlotEmptyParam>(0);
        }

        const auto fd = open_file(emuenv.io, construct_slotparam_path(i).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
        switch (cond->type) {
        case SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT:
            if (fd > 0) {
                if (slotList) {
                    SceAppUtilSaveDataSlotParam param{};
                    read_file(&param, emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
                    slotList[result->hitNum].userParam = param.userParam;
                    slotList[result->hitNum].status = param.status;
                    slotList[result->hitNum].id = i;
                }
                result->hitNum++;
            }
            break;
        case SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT:
            if (fd < 0) {
                if (slotList)
                    slotList[result->hitNum].id = i;
                result->hitNum++;
            }
            break;
        default: break;
        }

        if (fd > 0)
            close_file(emuenv.io, fd, export_name);
    }

    return 0;
}

EXPORT(SceInt32, sceAppUtilSaveDataSlotSetParam, SceAppUtilSaveDataSlotId slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotSetParam, slotId, param, mountPoint);
    const auto fd = open_file(emuenv.io, construct_slotparam_path(slotId).c_str(), SCE_O_WRONLY, emuenv.pref_path, export_name);
    if (fd < 0)
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), emuenv.io, export_name);
    close_file(emuenv.io, fd, export_name);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataUmount) {
    TRACY_FUNC(sceAppUtilSaveDataUmount);
    return UNIMPLEMENTED();
}

static SceInt32 SafeMemory(EmuEnvState &emuenv, void *buf, SceSize bufSize, SceOff offset, const char *export_name, bool save) {
    std::vector<char> safe_mem(SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE);
    const auto safe_mem_path = construct_savedata0_path("sce_sys/safemem", "dat");
    SceInt32 res = 0;

    // Open file when it exist
    const auto fd = open_file(emuenv.io, safe_mem_path.c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
    if (fd > 0) {
        // Read file for set data inside safe mem when it exist
        res = read_file(safe_mem.data(), emuenv.io, fd, SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE, export_name);
        close_file(emuenv.io, fd, export_name);
    }

    if ((fd < 0) || save) {
        // When safe mem no exist or in save mode, write it with set buffer inside data
        const auto fd = open_file(emuenv.io, safe_mem_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.pref_path, export_name);
        memcpy(&safe_mem[offset], buf, bufSize);
        write_file(fd, safe_mem.data(), SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE, emuenv.io, export_name);
        close_file(emuenv.io, fd, export_name);
    } else
        memcpy(buf, &safe_mem[offset], bufSize);

    return res;
}

EXPORT(SceInt32, sceAppUtilLoadSafeMemory, void *buf, SceSize bufSize, SceOff offset) {
    TRACY_FUNC(sceAppUtilLoadSafeMemory, buf, bufSize, offset);
    if (!buf || (offset + bufSize > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    const auto res = SafeMemory(emuenv, buf, bufSize, offset, export_name, false);

    // Load can return 0 when file no exist
    return res > 0 ? bufSize : 0;
}

EXPORT(SceInt32, sceAppUtilSaveSafeMemory, const void *buf, SceSize bufSize, SceOff offset) {
    TRACY_FUNC(sceAppUtilSaveSafeMemory, buf, bufSize, offset);
    if (!buf || (offset + bufSize > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    SafeMemory(emuenv, const_cast<void *>(buf), bufSize, offset, export_name, true);

    return bufSize;
}

EXPORT(int, sceAppUtilShutdown) {
    TRACY_FUNC(sceAppUtilShutdown);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilStoreBrowse) {
    TRACY_FUNC(sceAppUtilStoreBrowse);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppUtilSystemParamGetInt, SceSystemParamId paramId, SceInt32 *value) {
    TRACY_FUNC(sceAppUtilSystemParamGetInt, paramId, value);
    if (!value)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        *value = (SceSystemParamLang)emuenv.cfg.sys_lang;
        return 0;
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        *value = (SceSystemParamEnterButtonAssign)emuenv.cfg.sys_button;
        return 0;
    case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
        *value = (SceSystemParamDateFormat)emuenv.cfg.sys_date_format;
        return 0;
    case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
        *value = (SceSystemParamTimeFormat)emuenv.cfg.sys_time_format;
        return 0;
    case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
    case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
        STUBBED("No support Time Zone and Summer Time, give 0 value");
        *value = 0;
        return 0;
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
}

EXPORT(int, sceAppUtilSystemParamGetString, unsigned int paramId, SceChar8 *buf, SceSize bufSize) {
    TRACY_FUNC(sceAppUtilSystemParamGetString, paramId, buf, bufSize);
    constexpr auto devname_len = SCE_SYSTEM_PARAM_USERNAME_MAXSIZE;
    char devname[devname_len];
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_USER_NAME:
        if (gethostname(devname, devname_len)) {
            // fallback to User Name
            std::strncpy(devname, emuenv.io.user_name.c_str(), sizeof(devname));
        }
        std::strncpy(reinterpret_cast<char *>(buf), devname, sizeof(devname));
        break;
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
    return 0;
}
