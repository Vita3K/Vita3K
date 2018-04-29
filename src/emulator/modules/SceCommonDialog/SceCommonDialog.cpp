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
    return unimplemented("sceCameraImportDialogAbort");
}

EXPORT(int, sceCameraImportDialogGetResult) {
    return unimplemented("sceCameraImportDialogGetResult");
}

EXPORT(int, sceCameraImportDialogGetStatus) {
    return unimplemented("sceCameraImportDialogGetStatus");
}

EXPORT(int, sceCameraImportDialogInit) {
    return unimplemented("sceCameraImportDialogInit");
}

EXPORT(int, sceCameraImportDialogTerm) {
    return unimplemented("sceCameraImportDialogTerm");
}

EXPORT(int, sceCommonDialogGetWorkerThreadId) {
    return unimplemented("sceCommonDialogGetWorkerThreadId");
}

EXPORT(int, sceCommonDialogIsRunning) {
    return unimplemented("sceCommonDialogIsRunning");
}

EXPORT(int, sceCommonDialogSetConfigParam) {
    return unimplemented("sceCommonDialogSetConfigParam");
}

EXPORT(int, sceCommonDialogUpdate) {
    return unimplemented("sceCommonDialogUpdate");
}

EXPORT(int, sceCompanionUtilDialogAbort) {
    return unimplemented("sceCompanionUtilDialogAbort");
}

EXPORT(int, sceCompanionUtilDialogGetResult) {
    return unimplemented("sceCompanionUtilDialogGetResult");
}

EXPORT(int, sceCompanionUtilDialogGetStatus) {
    return unimplemented("sceCompanionUtilDialogGetStatus");
}

EXPORT(int, sceCompanionUtilDialogInit) {
    return unimplemented("sceCompanionUtilDialogInit");
}

EXPORT(int, sceCompanionUtilDialogTerm) {
    return unimplemented("sceCompanionUtilDialogTerm");
}

EXPORT(int, sceCrossControllerDialogAbort) {
    return unimplemented("sceCrossControllerDialogAbort");
}

EXPORT(int, sceCrossControllerDialogGetResult) {
    return unimplemented("sceCrossControllerDialogGetResult");
}

EXPORT(int, sceCrossControllerDialogGetStatus) {
    return unimplemented("sceCrossControllerDialogGetStatus");
}

EXPORT(int, sceCrossControllerDialogInit) {
    return unimplemented("sceCrossControllerDialogInit");
}

EXPORT(int, sceCrossControllerDialogTerm) {
    return unimplemented("sceCrossControllerDialogTerm");
}

EXPORT(int, sceImeDialogAbort) {
    return unimplemented("sceImeDialogAbort");
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
        return RET_ERROR(__func__, SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
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
        return RET_ERROR(__func__, SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.gui.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceMsgDialogAbort) {
    return unimplemented("sceMsgDialogAbort");
}

EXPORT(int, sceMsgDialogClose) {
    return unimplemented("sceMsgDialogClose");
}

EXPORT(int, sceMsgDialogGetResult) {
    return unimplemented("sceMsgDialogGetResult");
}

EXPORT(int, sceMsgDialogGetStatus) {
    return unimplemented("sceMsgDialogGetStatus");
}

EXPORT(int, sceMsgDialogInit) {
    return unimplemented("sceMsgDialogInit");
}

EXPORT(int, sceMsgDialogProgressBarInc) {
    return unimplemented("sceMsgDialogProgressBarInc");
}

EXPORT(int, sceMsgDialogProgressBarSetMsg) {
    return unimplemented("sceMsgDialogProgressBarSetMsg");
}

EXPORT(int, sceMsgDialogProgressBarSetValue) {
    return unimplemented("sceMsgDialogProgressBarSetValue");
}

EXPORT(int, sceMsgDialogTerm) {
    return unimplemented("sceMsgDialogTerm");
}

EXPORT(int, sceNetCheckDialogAbort) {
    return unimplemented("sceNetCheckDialogAbort");
}

EXPORT(int, sceNetCheckDialogGetPS3ConnectInfo) {
    return unimplemented("sceNetCheckDialogGetPS3ConnectInfo");
}

EXPORT(int, sceNetCheckDialogGetResult) {
    return unimplemented("sceNetCheckDialogGetResult");
}

EXPORT(int, sceNetCheckDialogGetStatus) {
    return unimplemented("sceNetCheckDialogGetStatus");
}

EXPORT(int, sceNetCheckDialogInit) {
    return unimplemented("sceNetCheckDialogInit");
}

EXPORT(int, sceNetCheckDialogTerm) {
    return unimplemented("sceNetCheckDialogTerm");
}

EXPORT(int, sceNpFriendList2DialogAbort) {
    return unimplemented("sceNpFriendList2DialogAbort");
}

EXPORT(int, sceNpFriendList2DialogGetResult) {
    return unimplemented("sceNpFriendList2DialogGetResult");
}

EXPORT(int, sceNpFriendList2DialogGetStatus) {
    return unimplemented("sceNpFriendList2DialogGetStatus");
}

EXPORT(int, sceNpFriendList2DialogInit) {
    return unimplemented("sceNpFriendList2DialogInit");
}

EXPORT(int, sceNpFriendList2DialogTerm) {
    return unimplemented("sceNpFriendList2DialogTerm");
}

EXPORT(int, sceNpFriendListDialogAbort) {
    return unimplemented("sceNpFriendListDialogAbort");
}

EXPORT(int, sceNpFriendListDialogGetResult) {
    return unimplemented("sceNpFriendListDialogGetResult");
}

EXPORT(int, sceNpFriendListDialogGetStatus) {
    return unimplemented("sceNpFriendListDialogGetStatus");
}

EXPORT(int, sceNpFriendListDialogInit) {
    return unimplemented("sceNpFriendListDialogInit");
}

EXPORT(int, sceNpFriendListDialogTerm) {
    return unimplemented("sceNpFriendListDialogTerm");
}

EXPORT(int, sceNpMessageDialogAbort) {
    return unimplemented("sceNpMessageDialogAbort");
}

EXPORT(int, sceNpMessageDialogGetResult) {
    return unimplemented("sceNpMessageDialogGetResult");
}

EXPORT(int, sceNpMessageDialogGetStatus) {
    return unimplemented("sceNpMessageDialogGetStatus");
}

EXPORT(int, sceNpMessageDialogInit) {
    return unimplemented("sceNpMessageDialogInit");
}

EXPORT(int, sceNpMessageDialogTerm) {
    return unimplemented("sceNpMessageDialogTerm");
}

EXPORT(int, sceNpProfileDialogAbort) {
    return unimplemented("sceNpProfileDialogAbort");
}

EXPORT(int, sceNpProfileDialogGetResult) {
    return unimplemented("sceNpProfileDialogGetResult");
}

EXPORT(int, sceNpProfileDialogGetStatus) {
    return unimplemented("sceNpProfileDialogGetStatus");
}

EXPORT(int, sceNpProfileDialogInit) {
    return unimplemented("sceNpProfileDialogInit");
}

EXPORT(int, sceNpProfileDialogTerm) {
    return unimplemented("sceNpProfileDialogTerm");
}

EXPORT(int, sceNpSnsFacebookDialogAbort) {
    return unimplemented("sceNpSnsFacebookDialogAbort");
}

EXPORT(int, sceNpSnsFacebookDialogGetResult) {
    return unimplemented("sceNpSnsFacebookDialogGetResult");
}

EXPORT(int, sceNpSnsFacebookDialogGetResultLongToken) {
    return unimplemented("sceNpSnsFacebookDialogGetResultLongToken");
}

EXPORT(int, sceNpSnsFacebookDialogGetStatus) {
    return unimplemented("sceNpSnsFacebookDialogGetStatus");
}

EXPORT(int, sceNpSnsFacebookDialogInit) {
    return unimplemented("sceNpSnsFacebookDialogInit");
}

EXPORT(int, sceNpSnsFacebookDialogTerm) {
    return unimplemented("sceNpSnsFacebookDialogTerm");
}

EXPORT(int, sceNpTrophySetupDialogAbort) {
    return unimplemented("sceNpTrophySetupDialogAbort");
}

EXPORT(int, sceNpTrophySetupDialogGetResult) {
    return unimplemented("sceNpTrophySetupDialogGetResult");
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    return unimplemented("sceNpTrophySetupDialogGetStatus");
}

EXPORT(int, sceNpTrophySetupDialogInit) {
    return unimplemented("sceNpTrophySetupDialogInit");
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    return unimplemented("sceNpTrophySetupDialogTerm");
}

EXPORT(int, scePhotoImportDialogAbort) {
    return unimplemented("scePhotoImportDialogAbort");
}

EXPORT(int, scePhotoImportDialogGetResult) {
    return unimplemented("scePhotoImportDialogGetResult");
}

EXPORT(int, scePhotoImportDialogGetStatus) {
    return unimplemented("scePhotoImportDialogGetStatus");
}

EXPORT(int, scePhotoImportDialogInit) {
    return unimplemented("scePhotoImportDialogInit");
}

EXPORT(int, scePhotoImportDialogTerm) {
    return unimplemented("scePhotoImportDialogTerm");
}

EXPORT(int, scePhotoReviewDialogAbort) {
    return unimplemented("scePhotoReviewDialogAbort");
}

EXPORT(int, scePhotoReviewDialogGetResult) {
    return unimplemented("scePhotoReviewDialogGetResult");
}

EXPORT(int, scePhotoReviewDialogGetStatus) {
    return unimplemented("scePhotoReviewDialogGetStatus");
}

EXPORT(int, scePhotoReviewDialogInit) {
    return unimplemented("scePhotoReviewDialogInit");
}

EXPORT(int, scePhotoReviewDialogTerm) {
    return unimplemented("scePhotoReviewDialogTerm");
}

EXPORT(int, scePspSaveDataDialogContinue) {
    return unimplemented("scePspSaveDataDialogContinue");
}

EXPORT(int, scePspSaveDataDialogGetResult) {
    return unimplemented("scePspSaveDataDialogGetResult");
}

EXPORT(int, scePspSaveDataDialogInit) {
    return unimplemented("scePspSaveDataDialogInit");
}

EXPORT(int, scePspSaveDataDialogTerm) {
    return unimplemented("scePspSaveDataDialogTerm");
}

EXPORT(int, sceRemoteOSKDialogAbort) {
    return unimplemented("sceRemoteOSKDialogAbort");
}

EXPORT(int, sceRemoteOSKDialogGetResult) {
    return unimplemented("sceRemoteOSKDialogGetResult");
}

EXPORT(int, sceRemoteOSKDialogGetStatus) {
    return unimplemented("sceRemoteOSKDialogGetStatus");
}

EXPORT(int, sceRemoteOSKDialogInit) {
    return unimplemented("sceRemoteOSKDialogInit");
}

EXPORT(int, sceRemoteOSKDialogTerm) {
    return unimplemented("sceRemoteOSKDialogTerm");
}

EXPORT(int, sceSaveDataDialogAbort) {
    return unimplemented("sceSaveDataDialogAbort");
}

EXPORT(int, sceSaveDataDialogContinue) {
    return unimplemented("sceSaveDataDialogContinue");
}

EXPORT(int, sceSaveDataDialogFinish) {
    return unimplemented("sceSaveDataDialogFinish");
}

EXPORT(int, sceSaveDataDialogGetResult) {
    return unimplemented("sceSaveDataDialogGetResult");
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    return unimplemented("sceSaveDataDialogGetStatus");
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    return unimplemented("sceSaveDataDialogGetSubStatus");
}

EXPORT(int, sceSaveDataDialogInit) {
    return unimplemented("sceSaveDataDialogInit");
}

EXPORT(int, sceSaveDataDialogProgressBarInc) {
    return unimplemented("sceSaveDataDialogProgressBarInc");
}

EXPORT(int, sceSaveDataDialogProgressBarSetValue) {
    return unimplemented("sceSaveDataDialogProgressBarSetValue");
}

EXPORT(int, sceSaveDataDialogSubClose) {
    return unimplemented("sceSaveDataDialogSubClose");
}

EXPORT(int, sceSaveDataDialogTerm) {
    return unimplemented("sceSaveDataDialogTerm");
}

EXPORT(int, sceStoreCheckoutDialogAbort) {
    return unimplemented("sceStoreCheckoutDialogAbort");
}

EXPORT(int, sceStoreCheckoutDialogGetResult) {
    return unimplemented("sceStoreCheckoutDialogGetResult");
}

EXPORT(int, sceStoreCheckoutDialogGetStatus) {
    return unimplemented("sceStoreCheckoutDialogGetStatus");
}

EXPORT(int, sceStoreCheckoutDialogInit) {
    return unimplemented("sceStoreCheckoutDialogInit");
}

EXPORT(int, sceStoreCheckoutDialogTerm) {
    return unimplemented("sceStoreCheckoutDialogTerm");
}

EXPORT(int, sceTwDialogAbort) {
    return unimplemented("sceTwDialogAbort");
}

EXPORT(int, sceTwDialogGetResult) {
    return unimplemented("sceTwDialogGetResult");
}

EXPORT(int, sceTwDialogGetStatus) {
    return unimplemented("sceTwDialogGetStatus");
}

EXPORT(int, sceTwDialogInit) {
    return unimplemented("sceTwDialogInit");
}

EXPORT(int, sceTwDialogTerm) {
    return unimplemented("sceTwDialogTerm");
}

EXPORT(int, sceTwLoginDialogAbort) {
    return unimplemented("sceTwLoginDialogAbort");
}

EXPORT(int, sceTwLoginDialogGetResult) {
    return unimplemented("sceTwLoginDialogGetResult");
}

EXPORT(int, sceTwLoginDialogGetStatus) {
    return unimplemented("sceTwLoginDialogGetStatus");
}

EXPORT(int, sceTwLoginDialogTerm) {
    return unimplemented("sceTwLoginDialogTerm");
}

EXPORT(int, sceVideoImportDialogAbort) {
    return unimplemented("sceVideoImportDialogAbort");
}

EXPORT(int, sceVideoImportDialogGetResult) {
    return unimplemented("sceVideoImportDialogGetResult");
}

EXPORT(int, sceVideoImportDialogGetStatus) {
    return unimplemented("sceVideoImportDialogGetStatus");
}

EXPORT(int, sceVideoImportDialogInit) {
    return unimplemented("sceVideoImportDialogInit");
}

EXPORT(int, sceVideoImportDialogTerm) {
    return unimplemented("sceVideoImportDialogTerm");
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
