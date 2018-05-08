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

#include <psp2/apputil.h>
#include <psp2/system_param.h>

#include <host/app_util.h>
#include <io/functions.h>

EXPORT(int, sceAppUtilAddCookieWebBrowser) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAddcontMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAddcontUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseGameCustomData) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseIncomingDialog) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseLiveArea) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseNearGift) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseNpActivity) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseNpAppDataMessage) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseNpBasicJoinablePresence) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseNpInviteMessage) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseScreenShotNotification) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseSessionInvitation) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseTeleport) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseTriggerUtil) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppEventParseWebBrowser) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilAppParamGetInt) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilBgdlGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilDrmClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilDrmOpen) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilLaunchWebBrowser) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilLoadSafeMemory) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilMusicMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilMusicUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilPhotoMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilPhotoUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilPspSaveDataGetDirNameList) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilPspSaveDataLoad) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilReceiveAppEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilResetCookieWebBrowser) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataDataRemove, emu::SceAppUtilSaveDataFileSlot *slot, emu::SceAppUtilSaveDataRemoveItem *files, unsigned int fileNum, SceAppUtilSaveDataMountPoint *mountPoint) {
    for (unsigned int i = 0; i < fileNum; i++) {
        std::string file_path = "savedata0:/";
        file_path += files[i].dataPath.get(host.mem);
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT:
            remove_file(file_path.c_str(), host.pref_path.c_str());
            break;
        default: // Directory remotion
            remove_dir(file_path.c_str(), host.pref_path.c_str());
            break;
        }
    }

    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slot->id);
    slot_path += ".bin";
    remove_file(slot_path.c_str(), host.pref_path.c_str());
    return 0;
}

EXPORT(int, sceAppUtilSaveDataDataSave, emu::SceAppUtilSaveDataFileSlot *slot, emu::SceAppUtilSaveDataFile *files, unsigned int fileNum, SceAppUtilSaveDataMountPoint *mountPoint, SceSize *requiredSizeKB) {
    SceUID fd;

    for (unsigned int i = 0; i < fileNum; i++) {
        std::string file_path = "savedata0:/";
        file_path += files[i].filePath.get(host.mem);
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE:
            fd = open_file(host.io, file_path, SCE_O_WRONLY | SCE_O_CREAT, host.pref_path.c_str());
            seek_file(fd, files[i].offset, SCE_SEEK_SET, host.io);
            write_file(fd, files[i].buf.get(host.mem), files[i].bufSize, host.io);
            close_file(host.io, fd);
            break;
        default: // Directory creation
            create_dir(file_path.c_str(), 0777, host.pref_path.c_str());
            break;
        }
    }

    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slot->id);
    slot_path += ".bin";
    fd = open_file(host.io, slot_path, SCE_O_WRONLY | SCE_O_CREAT, host.pref_path.c_str());
    write_file(fd, &slot->slotParam, sizeof(SceAppUtilSaveDataSlotParam), host.io);
    close_file(host.io, fd);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataGetQuota) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotCreate, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slotId);
    slot_path += ".bin";
    SceUID fd = open_file(host.io, slot_path, SCE_O_WRONLY | SCE_O_CREAT, host.pref_path.c_str());
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), host.io);
    close_file(host.io, fd);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotDelete, unsigned int slotId, SceAppUtilSaveDataMountPoint *mountPoint) {
    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slotId);
    slot_path += ".bin";
    remove_file(slot_path.c_str(), host.pref_path.c_str());
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotGetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slotId);
    slot_path += ".bin";
    SceUID fd = open_file(host.io, slot_path, SCE_O_RDONLY, host.pref_path.c_str());
    if (fd < 0)
        return RET_ERROR(export_name, SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    read_file(param, host.io, fd, sizeof(SceAppUtilSaveDataSlotParam));
    close_file(host.io, fd);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataSlotSearch) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotSetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilSaveDataMountPoint *mountPoint) {
    std::string slot_path = "savedata0:/SlotParam_";
    slot_path += std::to_string(slotId);
    slot_path += ".bin";
    SceUID fd = open_file(host.io, slot_path, SCE_O_WRONLY, host.pref_path.c_str());
    if (fd < 0)
        return RET_ERROR(export_name, SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    write_file(fd, param, sizeof(SceAppUtilSaveDataSlotParam), host.io);
    close_file(host.io, fd);
    return 0;
}

EXPORT(int, sceAppUtilSaveDataUmount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveSafeMemory) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilShutdown) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilStoreBrowse) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSystemParamGetInt, unsigned int paramId, int *value) {
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        *value = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
        return 0;
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        *value = SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS;
        return 0;
    default:
        return RET_ERROR(export_name, SCE_APPUTIL_ERROR_PARAMETER);
    }
}

EXPORT(int, sceAppUtilSystemParamGetString, unsigned int paramId, SceChar8 *buf, SceSize bufSize) {
    char devname[80];
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_USERNAME:
        gethostname(devname, 80);
#ifdef WIN32
        strcpy_s((char *)buf, SCE_SYSTEM_PARAM_USERNAME_MAXSIZE, devname);
#else
        strcpy((char *)buf, devname);
#endif
        break;
    default:
        return RET_ERROR(export_name, SCE_APPUTIL_ERROR_PARAMETER);
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
