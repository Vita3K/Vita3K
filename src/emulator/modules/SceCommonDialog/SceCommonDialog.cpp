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

#include <dialog/types.h>
#include <host/functions.h>
#include <util/string_convert.h>

#include "SceCommonDialog.h"

EXPORT(int, sceCameraImportDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceCameraImportDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceCameraImportDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceCameraImportDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceCameraImportDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceCommonDialogGetWorkerThreadId) {
    return unimplemented(export_name);
}

EXPORT(int, sceCommonDialogIsRunning) {
    return unimplemented(export_name);
}

EXPORT(int, sceCommonDialogSetConfigParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceCommonDialogUpdate) {
    return unimplemented(export_name);
}

EXPORT(int, sceCompanionUtilDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceCompanionUtilDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceCompanionUtilDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceCompanionUtilDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceCompanionUtilDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceCrossControllerDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceCrossControllerDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceCrossControllerDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceCrossControllerDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceCrossControllerDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceImeDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceImeDialogGetResult, SceImeDialogResult *result) {
    result->button = host.gui.common_dialog.ime.status;
    return 0;
}

EXPORT(int, sceImeDialogGetStatus) {
    return host.gui.common_dialog.status;
}

EXPORT(int, sceImeDialogInit, const Ptr<emu::SceImeDialogParam> param) {
    if (host.gui.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    emu::SceImeDialogParam *p = param.get(host.mem);

    std::u16string title = reinterpret_cast<char16_t *>(p->title.get(host.mem));
    std::u16string text = reinterpret_cast<char16_t *>(p->initialText.get(host.mem));

    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.gui.common_dialog.type = IME_DIALOG;

    host.gui.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_NONE;
    host.gui.common_dialog.ime.title = utf16_to_utf8(title);
    sprintf(host.gui.common_dialog.ime.text, utf16_to_utf8(text).c_str());
    host.gui.common_dialog.ime.max_length = p->maxTextLength;
    host.gui.common_dialog.ime.multiline = (p->option & SCE_IME_OPTION_MULTILINE);
    host.gui.common_dialog.ime.cancelable = (p->dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL);
    host.gui.common_dialog.ime.result = reinterpret_cast<uint16_t *>(p->inputTextBuffer.get(host.mem));

    return 0;
}

EXPORT(int, sceImeDialogTerm) {
    if (host.gui.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.gui.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceMsgDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogProgressBarInc) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogProgressBarSetMsg) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogProgressBarSetValue) {
    return unimplemented(export_name);
}

EXPORT(int, sceMsgDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogGetPS3ConnectInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNetCheckDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendList2DialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendList2DialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendList2DialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendList2DialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendList2DialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendListDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendListDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendListDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendListDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpFriendListDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpMessageDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpMessageDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpMessageDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpMessageDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpMessageDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpProfileDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpProfileDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpProfileDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpProfileDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpProfileDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogGetResultLongToken) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpSnsFacebookDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTrophySetupDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTrophySetupDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTrophySetupDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoImportDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoImportDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoImportDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoImportDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoImportDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoReviewDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoReviewDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoReviewDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoReviewDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, scePhotoReviewDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, scePspSaveDataDialogContinue) {
    return unimplemented(export_name);
}

EXPORT(int, scePspSaveDataDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, scePspSaveDataDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, scePspSaveDataDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceRemoteOSKDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceRemoteOSKDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceRemoteOSKDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceRemoteOSKDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceRemoteOSKDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogContinue) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogFinish) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogProgressBarInc) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogProgressBarSetValue) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogSubClose) {
    return unimplemented(export_name);
}

EXPORT(int, sceSaveDataDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceStoreCheckoutDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceStoreCheckoutDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceStoreCheckoutDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceStoreCheckoutDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceStoreCheckoutDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwLoginDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwLoginDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwLoginDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceTwLoginDialogTerm) {
    return unimplemented(export_name);
}

EXPORT(int, sceVideoImportDialogAbort) {
    return unimplemented(export_name);
}

EXPORT(int, sceVideoImportDialogGetResult) {
    return unimplemented(export_name);
}

EXPORT(int, sceVideoImportDialogGetStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceVideoImportDialogInit) {
    return unimplemented(export_name);
}

EXPORT(int, sceVideoImportDialogTerm) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceCameraImportDialogAbort)
BRIDGE_IMPL(sceCameraImportDialogGetResult)
BRIDGE_IMPL(sceCameraImportDialogGetStatus)
BRIDGE_IMPL(sceCameraImportDialogInit)
BRIDGE_IMPL(sceCameraImportDialogTerm)
BRIDGE_IMPL(sceCommonDialogGetWorkerThreadId)
BRIDGE_IMPL(sceCommonDialogIsRunning)
BRIDGE_IMPL(sceCommonDialogSetConfigParam)
BRIDGE_IMPL(sceCommonDialogUpdate)
BRIDGE_IMPL(sceCompanionUtilDialogAbort)
BRIDGE_IMPL(sceCompanionUtilDialogGetResult)
BRIDGE_IMPL(sceCompanionUtilDialogGetStatus)
BRIDGE_IMPL(sceCompanionUtilDialogInit)
BRIDGE_IMPL(sceCompanionUtilDialogTerm)
BRIDGE_IMPL(sceCrossControllerDialogAbort)
BRIDGE_IMPL(sceCrossControllerDialogGetResult)
BRIDGE_IMPL(sceCrossControllerDialogGetStatus)
BRIDGE_IMPL(sceCrossControllerDialogInit)
BRIDGE_IMPL(sceCrossControllerDialogTerm)
BRIDGE_IMPL(sceImeDialogAbort)
BRIDGE_IMPL(sceImeDialogGetResult)
BRIDGE_IMPL(sceImeDialogGetStatus)
BRIDGE_IMPL(sceImeDialogInit)
BRIDGE_IMPL(sceImeDialogTerm)
BRIDGE_IMPL(sceMsgDialogAbort)
BRIDGE_IMPL(sceMsgDialogClose)
BRIDGE_IMPL(sceMsgDialogGetResult)
BRIDGE_IMPL(sceMsgDialogGetStatus)
BRIDGE_IMPL(sceMsgDialogInit)
BRIDGE_IMPL(sceMsgDialogProgressBarInc)
BRIDGE_IMPL(sceMsgDialogProgressBarSetMsg)
BRIDGE_IMPL(sceMsgDialogProgressBarSetValue)
BRIDGE_IMPL(sceMsgDialogTerm)
BRIDGE_IMPL(sceNetCheckDialogAbort)
BRIDGE_IMPL(sceNetCheckDialogGetPS3ConnectInfo)
BRIDGE_IMPL(sceNetCheckDialogGetResult)
BRIDGE_IMPL(sceNetCheckDialogGetStatus)
BRIDGE_IMPL(sceNetCheckDialogInit)
BRIDGE_IMPL(sceNetCheckDialogTerm)
BRIDGE_IMPL(sceNpFriendList2DialogAbort)
BRIDGE_IMPL(sceNpFriendList2DialogGetResult)
BRIDGE_IMPL(sceNpFriendList2DialogGetStatus)
BRIDGE_IMPL(sceNpFriendList2DialogInit)
BRIDGE_IMPL(sceNpFriendList2DialogTerm)
BRIDGE_IMPL(sceNpFriendListDialogAbort)
BRIDGE_IMPL(sceNpFriendListDialogGetResult)
BRIDGE_IMPL(sceNpFriendListDialogGetStatus)
BRIDGE_IMPL(sceNpFriendListDialogInit)
BRIDGE_IMPL(sceNpFriendListDialogTerm)
BRIDGE_IMPL(sceNpMessageDialogAbort)
BRIDGE_IMPL(sceNpMessageDialogGetResult)
BRIDGE_IMPL(sceNpMessageDialogGetStatus)
BRIDGE_IMPL(sceNpMessageDialogInit)
BRIDGE_IMPL(sceNpMessageDialogTerm)
BRIDGE_IMPL(sceNpProfileDialogAbort)
BRIDGE_IMPL(sceNpProfileDialogGetResult)
BRIDGE_IMPL(sceNpProfileDialogGetStatus)
BRIDGE_IMPL(sceNpProfileDialogInit)
BRIDGE_IMPL(sceNpProfileDialogTerm)
BRIDGE_IMPL(sceNpSnsFacebookDialogAbort)
BRIDGE_IMPL(sceNpSnsFacebookDialogGetResult)
BRIDGE_IMPL(sceNpSnsFacebookDialogGetResultLongToken)
BRIDGE_IMPL(sceNpSnsFacebookDialogGetStatus)
BRIDGE_IMPL(sceNpSnsFacebookDialogInit)
BRIDGE_IMPL(sceNpSnsFacebookDialogTerm)
BRIDGE_IMPL(sceNpTrophySetupDialogAbort)
BRIDGE_IMPL(sceNpTrophySetupDialogGetResult)
BRIDGE_IMPL(sceNpTrophySetupDialogGetStatus)
BRIDGE_IMPL(sceNpTrophySetupDialogInit)
BRIDGE_IMPL(sceNpTrophySetupDialogTerm)
BRIDGE_IMPL(scePhotoImportDialogAbort)
BRIDGE_IMPL(scePhotoImportDialogGetResult)
BRIDGE_IMPL(scePhotoImportDialogGetStatus)
BRIDGE_IMPL(scePhotoImportDialogInit)
BRIDGE_IMPL(scePhotoImportDialogTerm)
BRIDGE_IMPL(scePhotoReviewDialogAbort)
BRIDGE_IMPL(scePhotoReviewDialogGetResult)
BRIDGE_IMPL(scePhotoReviewDialogGetStatus)
BRIDGE_IMPL(scePhotoReviewDialogInit)
BRIDGE_IMPL(scePhotoReviewDialogTerm)
BRIDGE_IMPL(scePspSaveDataDialogContinue)
BRIDGE_IMPL(scePspSaveDataDialogGetResult)
BRIDGE_IMPL(scePspSaveDataDialogInit)
BRIDGE_IMPL(scePspSaveDataDialogTerm)
BRIDGE_IMPL(sceRemoteOSKDialogAbort)
BRIDGE_IMPL(sceRemoteOSKDialogGetResult)
BRIDGE_IMPL(sceRemoteOSKDialogGetStatus)
BRIDGE_IMPL(sceRemoteOSKDialogInit)
BRIDGE_IMPL(sceRemoteOSKDialogTerm)
BRIDGE_IMPL(sceSaveDataDialogAbort)
BRIDGE_IMPL(sceSaveDataDialogContinue)
BRIDGE_IMPL(sceSaveDataDialogFinish)
BRIDGE_IMPL(sceSaveDataDialogGetResult)
BRIDGE_IMPL(sceSaveDataDialogGetStatus)
BRIDGE_IMPL(sceSaveDataDialogGetSubStatus)
BRIDGE_IMPL(sceSaveDataDialogInit)
BRIDGE_IMPL(sceSaveDataDialogProgressBarInc)
BRIDGE_IMPL(sceSaveDataDialogProgressBarSetValue)
BRIDGE_IMPL(sceSaveDataDialogSubClose)
BRIDGE_IMPL(sceSaveDataDialogTerm)
BRIDGE_IMPL(sceStoreCheckoutDialogAbort)
BRIDGE_IMPL(sceStoreCheckoutDialogGetResult)
BRIDGE_IMPL(sceStoreCheckoutDialogGetStatus)
BRIDGE_IMPL(sceStoreCheckoutDialogInit)
BRIDGE_IMPL(sceStoreCheckoutDialogTerm)
BRIDGE_IMPL(sceTwDialogAbort)
BRIDGE_IMPL(sceTwDialogGetResult)
BRIDGE_IMPL(sceTwDialogGetStatus)
BRIDGE_IMPL(sceTwDialogInit)
BRIDGE_IMPL(sceTwDialogTerm)
BRIDGE_IMPL(sceTwLoginDialogAbort)
BRIDGE_IMPL(sceTwLoginDialogGetResult)
BRIDGE_IMPL(sceTwLoginDialogGetStatus)
BRIDGE_IMPL(sceTwLoginDialogTerm)
BRIDGE_IMPL(sceVideoImportDialogAbort)
BRIDGE_IMPL(sceVideoImportDialogGetResult)
BRIDGE_IMPL(sceVideoImportDialogGetStatus)
BRIDGE_IMPL(sceVideoImportDialogInit)
BRIDGE_IMPL(sceVideoImportDialogTerm)
