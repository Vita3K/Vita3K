// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <emuenv/app_util.h>
#include <gui/state.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <packages/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL_timer.h>

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
    if (emuenv.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceImeDialogGetResult, SceImeDialogResult *result) {
    if (emuenv.common_dialog.type != IME_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result->result = emuenv.common_dialog.result;
    result->button = emuenv.common_dialog.ime.status;
    return 0;
}

EXPORT(int, sceImeDialogGetStatus) {
    if (emuenv.common_dialog.type != IME_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    return emuenv.common_dialog.status;
}

EXPORT(int, sceImeDialogInit, const Ptr<SceImeDialogParam> param) {
    if (emuenv.common_dialog.type != NO_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);

    SceImeDialogParam *p = param.get(emuenv.mem);

    std::u16string title = reinterpret_cast<char16_t *>(p->title.get(emuenv.mem));
    std::u16string text = reinterpret_cast<char16_t *>(p->initialText.get(emuenv.mem));

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.type = IME_DIALOG;

    emuenv.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_NONE;
    sprintf(emuenv.common_dialog.ime.title, "%s", string_utils::utf16_to_utf8(title).c_str());
    sprintf(emuenv.common_dialog.ime.text, "%s", string_utils::utf16_to_utf8(text).c_str());
    emuenv.common_dialog.ime.max_length = p->maxTextLength;
    emuenv.common_dialog.ime.multiline = (p->option & SCE_IME_OPTION_MULTILINE);
    emuenv.common_dialog.ime.cancelable = (p->dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL);
    emuenv.common_dialog.ime.result = reinterpret_cast<uint16_t *>(p->inputTextBuffer.get(emuenv.mem));

    return 0;
}

EXPORT(int, sceImeDialogTerm) {
    if (emuenv.common_dialog.type != IME_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceMsgDialogAbort) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceMsgDialogClose) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    return 0;
}

EXPORT(int, sceMsgDialogGetResult, SceMsgDialogResult *result) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result->result = emuenv.common_dialog.result;
    result->mode = emuenv.common_dialog.msg.mode;
    result->buttonId = emuenv.common_dialog.msg.status;
    return 0;
}

EXPORT(int, sceMsgDialogGetStatus) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    return emuenv.common_dialog.status;
}

EXPORT(int, sceMsgDialogInit, const Ptr<SceMsgDialogParam> param) {
    if (emuenv.common_dialog.type != NO_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);

    SceMsgDialogParam *p = param.get(emuenv.mem);
    SceMsgDialogUserMessageParam *up;
    SceMsgDialogButtonsParam *bp;
    SceMsgDialogProgressBarParam *pp;

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.type = MESSAGE_DIALOG;
    emuenv.common_dialog.msg.has_progress_bar = false;

    emuenv.common_dialog.msg.mode = p->mode;
    emuenv.common_dialog.msg.status = SCE_MSG_DIALOG_BUTTON_ID_INVALID;

    auto common = emuenv.common_dialog.lang.common;
    const auto CANCEL = common["cancel"];

    switch (p->mode) {
    case SCE_MSG_DIALOG_MODE_USER_MSG:
        up = p->userMsgParam.get(emuenv.mem);
        emuenv.common_dialog.msg.message = reinterpret_cast<char *>(up->msg.get(emuenv.mem));
        switch (up->buttonType) {
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = "OK";
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
            emuenv.common_dialog.msg.btn_num = 2;
            emuenv.common_dialog.msg.btn[0] = common["yes"];
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            emuenv.common_dialog.msg.btn[1] = common["no"];
            emuenv.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_NONE:
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
            emuenv.common_dialog.msg.btn_num = 2;
            emuenv.common_dialog.msg.btn[0] = "OK";
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            emuenv.common_dialog.msg.btn[1] = CANCEL;
            emuenv.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL:
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = CANCEL;
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS:
            bp = up->buttonParam.get(emuenv.mem);
            emuenv.common_dialog.msg.btn_num = 3;
            emuenv.common_dialog.msg.btn[0] = bp->msg1.get(emuenv.mem);
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            emuenv.common_dialog.msg.btn[1] = bp->msg2.get(emuenv.mem);
            emuenv.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            emuenv.common_dialog.msg.btn[2] = bp->msg3.get(emuenv.mem);
            emuenv.common_dialog.msg.btn_val[2] = SCE_MSG_DIALOG_BUTTON_ID_RETRY;
            break;
        }
        break;
    case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
        // TODO: implement the remaining system message types
        switch (p->sysMsgParam.get(emuenv.mem)->sysMsgType) {
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
            emuenv.common_dialog.msg.message = "Please wait.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE:
            emuenv.common_dialog.msg.message = "There is not enough free space on the memory card.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION:
            emuenv.common_dialog.msg.message = "Move away from the source of interference, or adjust the compass by moving your PS Vita system as shown below.";
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = CANCEL;
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED:
            emuenv.common_dialog.msg.message = "The microphone is disabled";
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = CANCEL;
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
            emuenv.common_dialog.msg.message = "Please wait.";
            emuenv.common_dialog.msg.btn[0] = CANCEL;
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            emuenv.common_dialog.msg.btn_num = 1;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE:
            emuenv.common_dialog.msg.message = "Cannot continue the application. No memory card is inserted.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION:
            emuenv.common_dialog.msg.message = "Cannot perform this operation. No memory card is inserted.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE:
            emuenv.common_dialog.msg.message = "There is not enough free space on the memory card.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED:
            emuenv.common_dialog.msg.message = "You must enable the microphone.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION:
            emuenv.common_dialog.msg.message = "You must use Wi-Fi to do this.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION:
            emuenv.common_dialog.msg.message = "You must use Wi-Fi to use this application.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE:
            emuenv.common_dialog.msg.message = "No content is available yet.";
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID:
        default:
            LOG_ERROR("Attempt to init message dialog with unknown system message mode: {}", log_hex(p->sysMsgParam.get(emuenv.mem)->sysMsgType));
        }
        break;
    case SCE_MSG_DIALOG_MODE_ERROR_CODE:
        emuenv.common_dialog.msg.message = fmt::format("An error occurred. Errorcode: {}", log_hex(p->errorCodeParam.get(emuenv.mem)->errorCode));
        emuenv.common_dialog.msg.btn_num = 1;
        emuenv.common_dialog.msg.btn[0] = "OK";
        emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
        pp = p->progBarParam.get(emuenv.mem);
        emuenv.common_dialog.msg.btn_num = 0;
        emuenv.common_dialog.msg.has_progress_bar = true;
        if (pp->msg.get(emuenv.mem) != nullptr) {
            emuenv.common_dialog.msg.message = reinterpret_cast<char *>(pp->msg.get(emuenv.mem));
        } else {
            switch (pp->sysMsgParam.sysMsgType) {
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
                emuenv.common_dialog.msg.message = "Please Wait.";
                break;
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
                emuenv.common_dialog.msg.message = "Please Wait";
                emuenv.common_dialog.msg.btn_num = 1;
                emuenv.common_dialog.msg.btn[0] = "CANCEL";
                emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_NO;
                break;
            }
        }
        break;
    case SCE_MSG_DIALOG_MODE_INVALID:
    default:
        LOG_ERROR("Attempt to init message dialog with unknown mode: {}", log_hex(p->mode));
        break;
    }

    return 0;
}

EXPORT(int, sceMsgDialogProgressBarInc, SceMsgDialogProgressBarTarget target, SceUInt32 delta) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (emuenv.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    emuenv.common_dialog.msg.bar_percent += delta;
    return 0;
}

EXPORT(int, sceMsgDialogProgressBarSetMsg, SceMsgDialogProgressBarTarget target, const SceChar8 *msg) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (emuenv.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    emuenv.common_dialog.msg.message = reinterpret_cast<const char *>(msg);
    return 0;
}

EXPORT(int, sceMsgDialogProgressBarSetValue, SceMsgDialogProgressBarTarget target, SceUInt32 rate) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (emuenv.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    emuenv.common_dialog.msg.bar_percent = rate;
    if (emuenv.common_dialog.msg.bar_percent > 100) {
        emuenv.common_dialog.msg.bar_percent = 100;
    }
    return 0;
}

EXPORT(int, sceMsgDialogTerm) {
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceNetCheckDialogAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogGetPS3ConnectInfo) {
    return UNIMPLEMENTED();
}

typedef struct SceNetCheckDialogResult {
    SceInt32 result;
    SceBool psnModeSucceeded;
    SceUInt8 reserved[124];
} SceNetCheckDialogResult;

EXPORT(int, sceNetCheckDialogGetResult, SceNetCheckDialogResult *result) {
    result->result = -1;
    return STUBBED("result->result = -1, return 0");
}

EXPORT(SceCommonDialogStatus, sceNetCheckDialogGetStatus) {
    if (emuenv.common_dialog.type != NETCHECK_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    STUBBED("SCE_COMMON_DIALOG_STATUS_FINISHED");
    return emuenv.common_dialog.status;
}

EXPORT(int, sceNetCheckDialogInit) {
    emuenv.common_dialog.type = NETCHECK_DIALOG;
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogTerm) {
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
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
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetResult, Ptr<SceNpTrophySetupDialogResult> result) {
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result.get(emuenv.mem)->result = emuenv.common_dialog.result;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    return emuenv.common_dialog.status;
}

EXPORT(int, sceNpTrophySetupDialogInit, const Ptr<SceNpTrophySetupDialogParam> param) {
    if (emuenv.common_dialog.type != NO_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.type = TROPHY_SETUP_DIALOG;
    emuenv.common_dialog.trophy.tick = SDL_GetTicks() + ((param.get(emuenv.mem)->options & 0x01) ? 3000 : 0);
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
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
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }

    emuenv.common_dialog.savedata.bar_percent = 0;
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

static void check_empty_param(EmuEnvState &emuenv, const SceAppUtilSaveDataSlotEmptyParam *empty_param, const uint32_t idx) {
    vfs::FileBuffer thumbnail_buffer;
    emuenv.common_dialog.savedata.title[idx] = "";
    emuenv.common_dialog.savedata.subtitle[idx] = "";
    emuenv.common_dialog.savedata.icon_buffer[idx].clear();
    emuenv.common_dialog.savedata.has_date[idx] = false;
    if (empty_param) {
        emuenv.common_dialog.savedata.title[idx] = empty_param->title.get(emuenv.mem) ? empty_param->title.get(emuenv.mem) : emuenv.common_dialog.lang.save_data.save["new_saved_data"];
        auto iconPath = empty_param->iconPath.get(emuenv.mem);
        SceUChar8 *iconBuf = reinterpret_cast<SceUChar8 *>(empty_param->iconBuf.get(emuenv.mem));
        auto iconBufSize = empty_param->iconBufSize;
        if (iconPath) {
            auto device = device::get_device(empty_param->iconPath.get(emuenv.mem));
            auto thumbnail_path = translate_path(empty_param->iconPath.get(emuenv.mem), device, emuenv.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, emuenv.pref_path, thumbnail_path);
            emuenv.common_dialog.savedata.icon_buffer[idx] = thumbnail_buffer;
            emuenv.common_dialog.savedata.icon_loaded[idx] = true;
        } else if (iconBuf && iconBufSize != 0) {
            thumbnail_buffer.insert(thumbnail_buffer.end(), iconBuf, iconBuf + iconBufSize);
            emuenv.common_dialog.savedata.icon_buffer[idx] = thumbnail_buffer;
            emuenv.common_dialog.savedata.icon_loaded[idx] = true;
        } else {
            emuenv.common_dialog.savedata.icon_loaded[idx] = false;
        }
    } else {
        emuenv.common_dialog.savedata.title[idx] = emuenv.common_dialog.lang.save_data.save["new_saved_data"];
        emuenv.common_dialog.savedata.icon_loaded[idx] = false;
    }
}

static void check_save_file(SceUID fd, std::vector<SceAppUtilSaveDataSlotParam> slot_param, int index, EmuEnvState &emuenv, const char *export_name) {
    vfs::FileBuffer thumbnail_buffer;
    if (fd < 0) {
        auto empty_param = emuenv.common_dialog.savedata.list_empty_param;
        check_empty_param(emuenv, empty_param, index);
    } else {
        read_file(&slot_param[index], emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
        close_file(emuenv.io, fd, export_name);
        emuenv.common_dialog.savedata.slot_info[index].isExist = 1;
        emuenv.common_dialog.savedata.title[index] = slot_param[index].title;
        emuenv.common_dialog.savedata.subtitle[index] = slot_param[index].subTitle;
        emuenv.common_dialog.savedata.details[index] = slot_param[index].detail;
        emuenv.common_dialog.savedata.date[index] = slot_param[index].modifiedTime;
        emuenv.common_dialog.savedata.has_date[index] = true;
        auto device = device::get_device(slot_param[index].iconPath);
        auto thumbnail_path = translate_path(slot_param[index].iconPath, device, emuenv.io.device_paths);
        vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, emuenv.pref_path, thumbnail_path);
        emuenv.common_dialog.savedata.icon_buffer[index] = thumbnail_buffer;
        emuenv.common_dialog.savedata.icon_loaded[index] = true;
    }
}

static void handle_user_message(SceSaveDataDialogUserMessageParam *user_message, EmuEnvState &emuenv) {
    auto common = emuenv.common_dialog.lang.common;
    switch (user_message->buttonType) {
    case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK:
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO:
        emuenv.common_dialog.savedata.btn_num = 2;
        emuenv.common_dialog.savedata.btn[0] = common["no"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
        emuenv.common_dialog.savedata.btn[1] = common["yes"];
        emuenv.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
        break;
    case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_NONE:
        emuenv.common_dialog.savedata.btn_num = 0;
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
        break;
    }
}

static void handle_sys_message(SceSaveDataDialogSystemMessageParam *sys_message, EmuEnvState &emuenv) {
    auto lang = emuenv.common_dialog.lang;
    auto common = lang.common;
    auto save_data = lang.save_data;
    auto deleting = save_data.deleting;
    auto load = save_data.load;
    auto save = save_data.save;

    switch (sys_message->sysMsgType) {
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NODATA:
        emuenv.common_dialog.savedata.msg = load["no_saved_data"];
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM:
        switch (emuenv.common_dialog.savedata.display_type) {
        case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
            emuenv.common_dialog.savedata.msg = save["save_the_data"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            emuenv.common_dialog.savedata.msg = load["load_saved_data"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
            emuenv.common_dialog.savedata.msg = deleting["delete_saved_data"];
            break;
        }
        emuenv.common_dialog.savedata.btn_num = 2;
        emuenv.common_dialog.savedata.btn[0] = common["no"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
        emuenv.common_dialog.savedata.btn[1] = common["yes"];
        emuenv.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_OVERWRITE:
        emuenv.common_dialog.savedata.msg = save["overwrite_saved_data"];
        emuenv.common_dialog.savedata.btn_num = 2;
        emuenv.common_dialog.savedata.btn[0] = common["no"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
        emuenv.common_dialog.savedata.btn[1] = common["yes"];
        emuenv.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE:
        emuenv.common_dialog.savedata.msg = save["not_free_space"];
        emuenv.common_dialog.savedata.btn_num = 0;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_PROGRESS:
        emuenv.common_dialog.savedata.btn_num = 0;
        switch (emuenv.common_dialog.savedata.display_type) {
        case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
            emuenv.common_dialog.savedata.msg = save["warning_saving"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            emuenv.common_dialog.savedata.msg = load["loading"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
            emuenv.common_dialog.savedata.msg = common["please_wait"];
            break;
        }
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FINISHED:
        switch (emuenv.common_dialog.savedata.display_type) {
        case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
            emuenv.common_dialog.savedata.msg = save["saving_complete"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            emuenv.common_dialog.savedata.msg = load["load_complete"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
            emuenv.common_dialog.savedata.msg = deleting["deletion_complete"];
            break;
        }
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM_CANCEL:
        switch (emuenv.common_dialog.savedata.display_type) {
        case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
            emuenv.common_dialog.savedata.msg = load["cancel_loading"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            emuenv.common_dialog.savedata.msg = save["cancel_saving"];
            break;
        case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
            emuenv.common_dialog.savedata.msg = deleting["cancel_deleting"];
            break;
        }
        emuenv.common_dialog.savedata.btn_num = 2;
        emuenv.common_dialog.savedata.btn[0] = common["no"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
        emuenv.common_dialog.savedata.btn[1] = common["yes"];
        emuenv.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FILE_CORRUPTED:
        emuenv.common_dialog.savedata.msg = common["file_corrupted"];
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE:
        emuenv.common_dialog.savedata.msg = save["could_not_save"];
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK;
        break;
    case SCE_SAVEDATE_DIALOG_SYSMSG_TYPE_NODATA_IMPORT:
        emuenv.common_dialog.savedata.msg = "There is no saved data that can be used with this application.\n\
			If the saved data you want to use is on the memory card, select the \"import saved data\" icon on the LiveArea screen.";
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = "OK";
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    default: break;
    }
}

EXPORT(int, sceSaveDataDialogContinue, const Ptr<SceSaveDataDialogParam> param) {
    if (param.get(emuenv.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (emuenv.common_dialog.type != SAVEDATA_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_FINISHED) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);
    }

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.savedata.has_progress_bar = false;

    const SceSaveDataDialogParam *p = param.get(emuenv.mem);
    SceSaveDataDialogUserMessageParam *user_message;
    SceSaveDataDialogSystemMessageParam *sys_message;
    SceSaveDataDialogProgressBarParam *progress_bar;
    SceAppUtilSaveDataSlotEmptyParam *empty_param;
    std::vector<SceAppUtilSaveDataSlotParam> slot_param;
    vfs::FileBuffer thumbnail_buffer;
    SceUID fd;

    emuenv.common_dialog.savedata.mode = p->mode;
    emuenv.common_dialog.savedata.display_type = p->dispType == 0 ? emuenv.common_dialog.savedata.display_type : p->dispType;
    emuenv.common_dialog.savedata.userdata = p->userdata;

    switch (emuenv.common_dialog.savedata.mode) {
    default:
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        slot_param.resize(1);
        emuenv.common_dialog.savedata.icon_loaded.resize(1);
        fd = open_file(emuenv.io, construct_slotparam_path(emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save]).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
        if (fd < 0) {
            emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist = 0;
        } else {
            emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist = 1;
            read_file(&slot_param[0], emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
            close_file(emuenv.io, fd, export_name);
            emuenv.common_dialog.savedata.title[emuenv.common_dialog.savedata.selected_save] = slot_param[0].title;
            emuenv.common_dialog.savedata.subtitle[emuenv.common_dialog.savedata.selected_save] = slot_param[0].subTitle;
            emuenv.common_dialog.savedata.details[emuenv.common_dialog.savedata.selected_save] = slot_param[0].detail;
            emuenv.common_dialog.savedata.date[emuenv.common_dialog.savedata.selected_save] = slot_param[0].modifiedTime;
            emuenv.common_dialog.savedata.has_date[emuenv.common_dialog.savedata.selected_save] = true;
            auto device = device::get_device(slot_param[0].iconPath);
            auto thumbnail_path = translate_path(slot_param[0].iconPath, device, emuenv.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, emuenv.pref_path, thumbnail_path);
            emuenv.common_dialog.savedata.icon_buffer[emuenv.common_dialog.savedata.selected_save] = thumbnail_buffer;
            emuenv.common_dialog.savedata.icon_loaded[0] = true;
        }
        switch (p->mode) {
        case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
            user_message = p->userMsgParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(emuenv.mem));
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = user_message->targetSlot.id;
            if (!emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist) {
                empty_param = user_message->targetSlot.emptyParam.get(emuenv.mem);
                check_empty_param(emuenv, empty_param, emuenv.common_dialog.savedata.selected_save);
            }

            handle_user_message(user_message, emuenv);
            break;
        case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
            sys_message = p->sysMsgParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = sys_message->targetSlot.id;
            if (!emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist) {
                empty_param = sys_message->targetSlot.emptyParam.get(emuenv.mem);
                check_empty_param(emuenv, empty_param, emuenv.common_dialog.savedata.selected_save);
            }
            handle_sys_message(sys_message, emuenv);
            break;
        case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = p->errorCodeParam.get(emuenv.mem)->targetSlot.id;
            if (!emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist) {
                empty_param = p->errorCodeParam.get(emuenv.mem)->targetSlot.emptyParam.get(emuenv.mem);
                check_empty_param(emuenv, empty_param, emuenv.common_dialog.savedata.selected_save);
            }
            emuenv.common_dialog.savedata.btn_num = 1;
            emuenv.common_dialog.savedata.btn[0] = "OK";
            emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
            switch (emuenv.common_dialog.savedata.display_type) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while saving.\n({})", log_hex(p->errorCodeParam.get(emuenv.mem)->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while loading.\n({})", log_hex(p->errorCodeParam.get(emuenv.mem)->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while deleting.\n({})", log_hex(p->errorCodeParam.get(emuenv.mem)->errorCode));
                break;
            }
            break;
        case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
            emuenv.common_dialog.savedata.btn_num = 0;
            progress_bar = p->progressBarParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = progress_bar->targetSlot.id;
            emuenv.common_dialog.savedata.has_progress_bar = true;
            if (!emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist) {
                empty_param = progress_bar->targetSlot.emptyParam.get(emuenv.mem);
                check_empty_param(emuenv, empty_param, emuenv.common_dialog.savedata.selected_save);
            }
            if (progress_bar->msg.get(emuenv.mem) != nullptr) {
                emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(progress_bar->msg.get(emuenv.mem));
            } else {
                auto lang = emuenv.common_dialog.lang;
                auto save_data = lang.save_data;
                switch (progress_bar->sysMsgParam.sysMsgType) {
                case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_PROGRESS:
                    switch (emuenv.common_dialog.savedata.display_type) {
                    case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                        emuenv.common_dialog.savedata.msg = save_data.save["saving"];
                        break;
                    case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                        emuenv.common_dialog.savedata.msg = save_data.load["loading"];
                        break;
                    case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                        emuenv.common_dialog.savedata.msg = lang.common["please_wait"];
                        break;
                    }
                    break;
                default:
                    LOG_ERROR("Attempt to continue savedata progress dialog with unknown system message type: {}", log_hex(progress_bar->sysMsgParam.sysMsgType));
                    break;
                }
            }
            break;
        default:
            LOG_ERROR("Attempt to continue savedata dialog with unknown mode: {}", log_hex(p->mode));
            break;
        }
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_LIST;
        slot_param.resize(emuenv.common_dialog.savedata.slot_list_size);
        emuenv.common_dialog.savedata.icon_loaded.resize(emuenv.common_dialog.savedata.slot_list_size);
        for (std::uint32_t i = 0; i < emuenv.common_dialog.savedata.slot_list_size; i++) {
            fd = open_file(emuenv.io, construct_slotparam_path(emuenv.common_dialog.savedata.slot_id[i]).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
            check_save_file(fd, slot_param, i, emuenv, export_name);
        }
        break;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogFinish, Ptr<const SceSaveDataDialogFinishParam> finishParam) {
    if (finishParam.get(emuenv.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (emuenv.common_dialog.type != SAVEDATA_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_FINISHED) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);
    }

    emuenv.common_dialog.savedata.bar_percent = 0;
    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

EXPORT(SceInt32, sceSaveDataDialogGetResult, Ptr<SceSaveDataDialogResult> result) {
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    if (!result.get(emuenv.mem)) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (strcmp(reinterpret_cast<char *>(result.get(emuenv.mem)->reserved), "\0") != 0) {
        return RET_ERROR(SCE_SAVEDATA_DIALOG_ERROR_PARAM);
    }

    if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_FINISHED || emuenv.common_dialog.substatus == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        auto save_dialog = emuenv.common_dialog.savedata;
        auto result_slotinfo = result.get(emuenv.mem)->slotInfo.get(emuenv.mem);

        result.get(emuenv.mem)->mode = save_dialog.mode;
        result.get(emuenv.mem)->result = emuenv.common_dialog.result;
        result.get(emuenv.mem)->buttonId = save_dialog.button_id;
        result.get(emuenv.mem)->slotId = save_dialog.slot_id[save_dialog.selected_save];
        if (result.get(emuenv.mem)->slotInfo) {
            result_slotinfo->isExist = save_dialog.slot_info[save_dialog.selected_save].isExist;
            result_slotinfo->slotParam = save_dialog.slot_info[save_dialog.selected_save].slotParam;
        }
        result.get(emuenv.mem)->userdata = save_dialog.userdata;
        return 0;
    } else {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);
    }
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    return emuenv.common_dialog.status;
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return SCE_COMMON_DIALOG_STATUS_NONE;
    }
    return emuenv.common_dialog.substatus;
}

static void initialize_savedata_vectors(EmuEnvState &emuenv, unsigned int size) {
    emuenv.common_dialog.savedata.icon_loaded.resize(size);
    emuenv.common_dialog.savedata.slot_info.resize(size);
    emuenv.common_dialog.savedata.title.resize(size);
    emuenv.common_dialog.savedata.subtitle.resize(size);
    emuenv.common_dialog.savedata.details.resize(size);
    emuenv.common_dialog.savedata.has_date.resize(size);
    emuenv.common_dialog.savedata.date.resize(size);
    emuenv.common_dialog.savedata.icon_buffer.resize(size);
    emuenv.common_dialog.savedata.slot_id.resize(size);
}

EXPORT(int, sceSaveDataDialogInit, const Ptr<SceSaveDataDialogParam> param) {
    if (param.get(emuenv.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (emuenv.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);
    }

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;

    const SceSaveDataDialogParam *p = param.get(emuenv.mem);
    SceSaveDataDialogFixedParam *fixed_param;
    SceSaveDataDialogListParam *list_param;
    SceSaveDataDialogUserMessageParam *user_message;
    SceSaveDataDialogSystemMessageParam *sys_message;
    std::vector<SceAppUtilSaveDataSlot> slot_list;
    std::vector<SceAppUtilSaveDataSlotParam> slot_param;
    std::string thumbnail_path;
    SceUID fd;

    emuenv.common_dialog.savedata.mode = p->mode;
    emuenv.common_dialog.savedata.mode_to_display = p->mode;
    emuenv.common_dialog.savedata.display_type = p->dispType;
    emuenv.common_dialog.savedata.userdata = p->userdata;

    switch (p->mode) {
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        fixed_param = p->fixedParam.get(emuenv.mem);
        slot_param.resize(1);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.slot_id[0] = fixed_param->targetSlot.id;
        fd = open_file(emuenv.io, construct_slotparam_path(fixed_param->targetSlot.id).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
        if (fd < 0) {
            emuenv.common_dialog.savedata.slot_info[0].isExist = 0;
            emuenv.common_dialog.savedata.title[0] = "";
            emuenv.common_dialog.savedata.subtitle[0] = "";
            emuenv.common_dialog.savedata.details[0] = "";
            emuenv.common_dialog.savedata.has_date[0] = false;
        } else {
            emuenv.common_dialog.savedata.slot_info[0].isExist = 1;
            read_file(&slot_param[0], emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
            close_file(emuenv.io, fd, export_name);
            emuenv.common_dialog.savedata.title[0] = slot_param[0].title;
            emuenv.common_dialog.savedata.subtitle[0] = slot_param[0].subTitle;
            emuenv.common_dialog.savedata.details[0] = slot_param[0].detail;
            emuenv.common_dialog.savedata.date[0] = slot_param[0].modifiedTime;
            vfs::FileBuffer thumbnail_buffer;
            auto device = device::get_device(slot_param[0].iconPath);
            auto thumbnail_path = translate_path(slot_param[0].iconPath, device, emuenv.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, emuenv.pref_path, thumbnail_path);
            emuenv.common_dialog.savedata.icon_buffer[0] = thumbnail_buffer;
            emuenv.common_dialog.savedata.icon_loaded[0] = true;
        }
        emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        list_param = p->listParam.get(emuenv.mem);
        emuenv.common_dialog.savedata.slot_list_size = list_param->slotListSize;
        emuenv.common_dialog.savedata.list_style = list_param->itemStyle;

        slot_param.resize(list_param->slotListSize);
        slot_list.resize(list_param->slotListSize);
        initialize_savedata_vectors(emuenv, list_param->slotListSize);

        if (list_param->listTitle.get(emuenv.mem)) {
            emuenv.common_dialog.savedata.list_title = reinterpret_cast<const char *>(list_param->listTitle.get(emuenv.mem));
        } else {
            auto lang = emuenv.common_dialog.lang;
            auto save_data = lang.save_data;
            switch (p->dispType) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                emuenv.common_dialog.savedata.list_title = save_data.save["title"];
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                emuenv.common_dialog.savedata.list_title = save_data.load["title"];
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                emuenv.common_dialog.savedata.list_title = lang.common["delete"];
                break;
            }
        }

        for (SceUInt i = 0; i < list_param->slotListSize; i++) {
            slot_list[i] = list_param->slotList.get(emuenv.mem)[i];
            emuenv.common_dialog.savedata.slot_id[i] = slot_list[i].id;
            emuenv.common_dialog.savedata.list_empty_param = slot_list[0].emptyParam.get(emuenv.mem);
            fd = open_file(emuenv.io, construct_slotparam_path(slot_list[i].id).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
            check_save_file(fd, slot_param, i, emuenv, export_name);
        }
        break;
    case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
        user_message = p->userMsgParam.get(emuenv.mem);
        slot_param.resize(1);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.slot_id[0] = user_message->targetSlot.id;
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(emuenv.mem));

        fd = open_file(emuenv.io, construct_slotparam_path(user_message->targetSlot.id).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
        check_save_file(fd, slot_param, 0, emuenv, export_name);

        handle_user_message(user_message, emuenv);
        break;
    case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
        sys_message = p->sysMsgParam.get(emuenv.mem);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.slot_id[0] = sys_message->targetSlot.id;
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        if (!emuenv.common_dialog.savedata.slot_info[0].isExist) {
            auto empty_param = sys_message->targetSlot.emptyParam.get(emuenv.mem);
            check_empty_param(emuenv, empty_param, 0);
        }
        handle_sys_message(sys_message, emuenv);
        break;
    default:
        LOG_ERROR("Attempt to initialize savedata dialog with unknown mode: {}", log_hex(p->mode));
        break;
    }
    emuenv.common_dialog.type = SAVEDATA_DIALOG;
    return 0;
}

EXPORT(int, sceSaveDataDialogProgressBarInc, SceSaveDataDialogProgressBarTarget target, SceUInt32 delta) {
    if (emuenv.common_dialog.savedata.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING && emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    emuenv.common_dialog.savedata.bar_percent += delta;
    if (emuenv.common_dialog.savedata.bar_percent >= 100) {
        emuenv.common_dialog.savedata.bar_percent = 100;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogProgressBarSetValue, SceSaveDataDialogProgressBarTarget target, SceUInt32 rate) {
    if (emuenv.common_dialog.savedata.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING && emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    emuenv.common_dialog.savedata.bar_percent = rate;
    if (emuenv.common_dialog.savedata.bar_percent >= 100) {
        emuenv.common_dialog.savedata.bar_percent = 100;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogSubClose) {
    if (emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    return 0;
}

EXPORT(int, sceSaveDataDialogTerm) {
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);
    }

    emuenv.common_dialog.savedata.bar_percent = 0;
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
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
