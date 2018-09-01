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

#include "SceCommonDialog.h"

#include <dialog/types.h>
#include <host/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL_timer.h>

#include <psp2/common_dialog.h>
#include <psp2/netcheck_dialog.h>

EXPORT(int, sceCameraImportDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogGetWorkerThreadId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogIsRunning) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogSetConfigParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogUpdate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceImeDialogAbort) {
    if (host.gui.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceImeDialogGetResult, SceImeDialogResult *result) {
    result->result = host.gui.common_dialog.result;
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
    host.gui.common_dialog.ime.title = string_utils::utf16_to_utf8(title);
    sprintf(host.gui.common_dialog.ime.text, "%s", string_utils::utf16_to_utf8(text).c_str());
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
    if (host.gui.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceMsgDialogClose) {
    if (host.gui.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    return 0;
}

EXPORT(int, sceMsgDialogGetResult, SceMsgDialogResult *result) {
    result->result = host.gui.common_dialog.result;
    result->mode = host.gui.common_dialog.msg.mode;
    result->buttonId = host.gui.common_dialog.msg.status;
    return 0;
}

EXPORT(int, sceMsgDialogGetStatus) {
    return host.gui.common_dialog.status;
}

EXPORT(int, sceMsgDialogInit, const Ptr<emu::SceMsgDialogParam> param) {
    if (host.gui.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    emu::SceMsgDialogParam *p = param.get(host.mem);
    emu::SceMsgDialogUserMessageParam *up;
    emu::SceMsgDialogButtonsParam *bp;
    emu::SceMsgDialogProgressBarParam *pp;

    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.gui.common_dialog.type = MESSAGE_DIALOG;

    host.gui.common_dialog.msg.mode = p->mode;
    host.gui.common_dialog.msg.status = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
    switch (p->mode) {
    case SCE_MSG_DIALOG_MODE_USER_MSG:
        up = p->userMsgParam.get(host.mem);
        host.gui.common_dialog.msg.message = reinterpret_cast<char *>(up->msg.get(host.mem));
        switch (up->buttonType) {
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
            host.gui.common_dialog.msg.btn_num = 1;
            host.gui.common_dialog.msg.btn[0] = "OK";
            host.gui.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
            host.gui.common_dialog.msg.btn_num = 2;
            host.gui.common_dialog.msg.btn[0] = "Yes";
            host.gui.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            host.gui.common_dialog.msg.btn[1] = "No";
            host.gui.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_NONE:
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
            host.gui.common_dialog.msg.btn_num = 2;
            host.gui.common_dialog.msg.btn[0] = "OK";
            host.gui.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            host.gui.common_dialog.msg.btn[1] = "Cancel";
            host.gui.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS:
            bp = up->buttonParam.get(host.mem);
            host.gui.common_dialog.msg.btn_num = 3;
            host.gui.common_dialog.msg.btn[0] = bp->msg1.get(host.mem);
            host.gui.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            host.gui.common_dialog.msg.btn[1] = bp->msg2.get(host.mem);
            host.gui.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            host.gui.common_dialog.msg.btn[2] = bp->msg3.get(host.mem);
            host.gui.common_dialog.msg.btn_val[2] = SCE_MSG_DIALOG_BUTTON_ID_RETRY;
            break;
        }
        break;
    case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
        switch (p->sysMsgParam.get(host.mem)->sysMsgType) {
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
            host.gui.common_dialog.msg.message = "Please wait.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE:
            host.gui.common_dialog.msg.message = "There is not enough free space on the memory card.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION:
            host.gui.common_dialog.msg.message = "Move away from the source of interference, or adjust the compass by moving your PS Vita system as shown below.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
            host.gui.common_dialog.msg.message = "Please wait.";
            host.gui.common_dialog.msg.btn[0] = "Cancel";
            host.gui.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            host.gui.common_dialog.msg.btn_num = 1;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE:
            host.gui.common_dialog.msg.message = "Cannot continue the application. No memory card is inserted.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION:
            host.gui.common_dialog.msg.message = "Cannot perform this operation. No memory card is inserted.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED:
            host.gui.common_dialog.msg.message = "You must enable the microphone.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION:
            host.gui.common_dialog.msg.message = "You must use Wi-Fi to do this.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION:
            host.gui.common_dialog.msg.message = "You must use Wi-Fi to use this application.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE:
            host.gui.common_dialog.msg.message = "No content is available yet.";
            host.gui.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID:
        default:
            LOG_ERROR("Attempt to init message dialog with unknown system message mode: {}", log_hex(p->sysMsgParam.get(host.mem)->sysMsgType));
        }
        break;
    case SCE_MSG_DIALOG_MODE_ERROR_CODE:
        host.gui.common_dialog.msg.message = fmt::format("An error occurred. Errorcode: {}", log_hex(p->errorCodeParam.get(host.mem)->errorCode));
        host.gui.common_dialog.msg.btn_num = 0;
        break;
    case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
        // TODO: Properly implement this
        LOG_WARN("Progressbar message dialog used.");
        pp = p->progBarParam.get(host.mem);
        host.gui.common_dialog.msg.message = reinterpret_cast<char *>(pp->msg.get(host.mem));
        host.gui.common_dialog.msg.btn_num = 0;
        break;
    case SCE_MSG_DIALOG_MODE_INVALID:
    default:
        LOG_ERROR("Attempt to init message dialog with unknown mode: {}", log_hex(p->mode));
        break;
    }

    return 0;
}

EXPORT(int, sceMsgDialogProgressBarInc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMsgDialogProgressBarSetMsg) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMsgDialogProgressBarSetValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMsgDialogTerm) {
    if (host.gui.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.gui.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceNetCheckDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogGetPS3ConnectInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(SceCommonDialogStatus, sceNetCheckDialogGetStatus) {
    STUBBED("SCE_COMMON_DIALOG_STATUS_FINISHED");
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

EXPORT(int, sceNetCheckDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetResultLongToken) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophySetupDialogAbort) {
    if (host.gui.common_dialog.type != TROPHY_SETUP_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetResult, emu::SceNpTrophySetupDialogResult *result) {
    result->result = host.gui.common_dialog.result;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    return host.gui.common_dialog.status;
}

EXPORT(int, sceNpTrophySetupDialogInit, const Ptr<emu::SceNpTrophySetupDialogParam> param) {
    if (host.gui.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.gui.common_dialog.type = TROPHY_SETUP_DIALOG;
    host.gui.common_dialog.trophy.tick = SDL_GetTicks() + (param.get(host.mem)->options & 0x01) ? 3000 : 0;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    if (host.gui.common_dialog.type != TROPHY_SETUP_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.gui.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, scePhotoImportDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogContinue) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogContinue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogFinish) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogProgressBarInc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogProgressBarSetValue) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogSubClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogGetResult) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogGetStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogInit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogTerm) {
    return UNIMPLEMENTED();
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
