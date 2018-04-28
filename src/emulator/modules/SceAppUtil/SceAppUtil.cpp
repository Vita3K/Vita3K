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

EXPORT(int, sceAppUtilSaveDataDataRemove) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataDataSave) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataGetQuota) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataMount) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotCreate) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotDelete) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotGetParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotSearch) {
    return unimplemented(export_name);
}

EXPORT(int, sceAppUtilSaveDataSlotSetParam) {
    return unimplemented(export_name);
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
        break;
    default:
        return error(export_name, SCE_APPUTIL_ERROR_PARAMETER);
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
        return error(export_name, SCE_APPUTIL_ERROR_PARAMETER);
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
