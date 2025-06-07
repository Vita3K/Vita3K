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

#include <dialog/state.h>
#include <dialog/types.h>
#include <emuenv/app_util.h>
#include <gui/state.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
#include <packages/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL_timer.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceCommonDialog);

template <>
std::string to_debug_str<SceMsgDialogProgressBarTarget>(const MemState &mem, SceMsgDialogProgressBarTarget type) {
    switch (type) {
    case SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT:
        return "SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT";
    }
    return std::to_string(type);
}

template <>
std::string to_debug_str<SceSaveDataDialogProgressBarTarget>(const MemState &mem, SceSaveDataDialogProgressBarTarget type) {
    switch (type) {
    case SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT:
        return "SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT";
    }
    return std::to_string(type);
}

EXPORT(int, sceCameraImportDialogAbort) {
    TRACY_FUNC(sceCameraImportDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogGetResult) {
    TRACY_FUNC(sceCameraImportDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogGetStatus) {
    TRACY_FUNC(sceCameraImportDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogInit) {
    TRACY_FUNC(sceCameraImportDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCameraImportDialogTerm) {
    TRACY_FUNC(sceCameraImportDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialog_B7D4C911) {
    STUBBED("Return 2");
    return 2;
}

EXPORT(int, sceCommonDialogGetWorkerThreadId) {
    TRACY_FUNC(sceCommonDialogGetWorkerThreadId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogIsRunning) {
    TRACY_FUNC(sceCommonDialogIsRunning);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogSetConfigParam) {
    TRACY_FUNC(sceCommonDialogSetConfigParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCommonDialogUpdate) {
    TRACY_FUNC(sceCommonDialogUpdate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogAbort) {
    TRACY_FUNC(sceCompanionUtilDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogGetResult) {
    TRACY_FUNC(sceCompanionUtilDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogGetStatus) {
    TRACY_FUNC(sceCompanionUtilDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogInit) {
    TRACY_FUNC(sceCompanionUtilDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCompanionUtilDialogTerm) {
    TRACY_FUNC(sceCompanionUtilDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogAbort) {
    TRACY_FUNC(sceCrossControllerDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogGetResult) {
    TRACY_FUNC(sceCrossControllerDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogGetStatus) {
    TRACY_FUNC(sceCrossControllerDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogInit) {
    TRACY_FUNC(sceCrossControllerDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceCrossControllerDialogTerm) {
    TRACY_FUNC(sceCrossControllerDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceImeDialogAbort) {
    TRACY_FUNC(sceImeDialogAbort);
    if (emuenv.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceImeDialogGetResult, SceImeDialogResult *result) {
    TRACY_FUNC(sceImeDialogGetResult, result);
    if (emuenv.common_dialog.type != IME_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result->result = emuenv.common_dialog.result;
    result->button = emuenv.common_dialog.ime.status;
    return 0;
}

EXPORT(int, sceImeDialogGetStatus) {
    TRACY_FUNC(sceImeDialogGetStatus);
    if (emuenv.common_dialog.type != IME_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    return emuenv.common_dialog.status;
}

EXPORT(int, sceImeDialogInit, const Ptr<SceImeDialogParam> param) {
    TRACY_FUNC(sceImeDialogInit, param);
    if (emuenv.common_dialog.type != NO_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);

    SceImeDialogParam *p = param.get(emuenv.mem);

    std::u16string title = p->title.cast<char16_t>().get(emuenv.mem);
    std::u16string text = p->initialText.cast<char16_t>().get(emuenv.mem);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.type = IME_DIALOG;

    emuenv.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_NONE;
    snprintf(emuenv.common_dialog.ime.title, sizeof(emuenv.common_dialog.ime.title), "%s", string_utils::utf16_to_utf8(title).c_str());
    snprintf(emuenv.common_dialog.ime.text, sizeof(emuenv.common_dialog.ime.text), "%s", string_utils::utf16_to_utf8(text).c_str());
    emuenv.common_dialog.ime.max_length = p->maxTextLength;
    emuenv.common_dialog.ime.multiline = (p->option & SCE_IME_OPTION_MULTILINE);
    emuenv.common_dialog.ime.cancelable = (p->dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL);
    emuenv.common_dialog.ime.result = p->inputTextBuffer.get(emuenv.mem);

    return 0;
}

EXPORT(int, sceImeDialogTerm) {
    TRACY_FUNC(sceImeDialogTerm);
    if (emuenv.common_dialog.type != IME_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceMsgDialogAbort) {
    TRACY_FUNC(sceMsgDialogAbort);
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceMsgDialogClose) {
    TRACY_FUNC(sceMsgDialogClose);
    if (emuenv.common_dialog.type != MESSAGE_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    return 0;
}

EXPORT(int, sceMsgDialogGetResult, SceMsgDialogResult *result) {
    TRACY_FUNC(sceMsgDialogGetResult, result);
    if (emuenv.common_dialog.type != MESSAGE_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result->result = emuenv.common_dialog.result;
    result->mode = emuenv.common_dialog.msg.mode;
    result->buttonId = emuenv.common_dialog.msg.status;
    return 0;
}

EXPORT(int, sceMsgDialogGetStatus) {
    TRACY_FUNC(sceMsgDialogGetStatus);
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    return emuenv.common_dialog.status;
}

EXPORT(int, sceMsgDialogInit, const Ptr<SceMsgDialogParam> param) {
    TRACY_FUNC(sceMsgDialogInit, param);
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

    auto &common = emuenv.common_dialog.lang.common;
    auto &save = emuenv.common_dialog.lang.save_data.save;
    const auto &CANCEL = common["cancel"];

    switch (p->mode) {
    case SCE_MSG_DIALOG_MODE_USER_MSG:
        up = p->userMsgParam.get(emuenv.mem);
        emuenv.common_dialog.msg.message = reinterpret_cast<char *>(up->msg.get(emuenv.mem));
        switch (up->buttonType) {
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = common["ok"];
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
            emuenv.common_dialog.msg.btn[0] = common["ok"];
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
            emuenv.common_dialog.msg.message = common["please_wait"];
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE:
            emuenv.common_dialog.msg.message = save["not_free_space"];
            emuenv.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION:
            emuenv.common_dialog.msg.message = "Move away from the source of interference, or adjust the compass by moving your PS Vita system as shown below.";
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = CANCEL;
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED:
            emuenv.common_dialog.msg.message = common["microphone_disabled"];
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = common["ok"];
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
            emuenv.common_dialog.msg.message = common["please_wait"];
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
            emuenv.common_dialog.msg.message = save["not_free_space"];
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = common["ok"];
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED:
            emuenv.common_dialog.msg.message = common["microphone_disabled"];
            emuenv.common_dialog.msg.btn_num = 1;
            emuenv.common_dialog.msg.btn[0] = common["ok"];
            emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
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
        emuenv.common_dialog.msg.message = fmt::format(fmt::runtime(common["an_error_occurred"]), log_hex(p->errorCodeParam.get(emuenv.mem)->errorCode));
        emuenv.common_dialog.msg.btn_num = 1;
        emuenv.common_dialog.msg.btn[0] = common["ok"];
        emuenv.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
        pp = p->progBarParam.get(emuenv.mem);
        emuenv.common_dialog.msg.bar_percent = 0;
        emuenv.common_dialog.msg.btn_num = 0;
        emuenv.common_dialog.msg.has_progress_bar = true;
        if (pp->msg.get(emuenv.mem) != nullptr) {
            emuenv.common_dialog.msg.message = reinterpret_cast<char *>(pp->msg.get(emuenv.mem));
        } else {
            switch (pp->sysMsgParam.sysMsgType) {
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
                emuenv.common_dialog.msg.message = common["please_wait"];
                break;
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
                emuenv.common_dialog.msg.message = common["please_wait"];
                emuenv.common_dialog.msg.btn_num = 1;
                emuenv.common_dialog.msg.btn[0] = CANCEL;
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
    TRACY_FUNC(sceMsgDialogProgressBarInc, target, delta);
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
    TRACY_FUNC(sceMsgDialogProgressBarSetMsg, target, msg);
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
    TRACY_FUNC(sceMsgDialogProgressBarSetValue, target, rate);
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
    TRACY_FUNC(sceMsgDialogTerm);
    if (emuenv.common_dialog.type != MESSAGE_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceNetCheckDialogAbort) {
    TRACY_FUNC(sceNetCheckDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogGetPS3ConnectInfo) {
    TRACY_FUNC(sceNetCheckDialogGetPS3ConnectInfo);
    return UNIMPLEMENTED();
}

typedef struct SceNetCheckDialogResult {
    SceInt32 result;
    SceBool psnModeSucceeded;
    SceUInt8 reserved[124];
} SceNetCheckDialogResult;

EXPORT(int, sceNetCheckDialogGetResult, SceNetCheckDialogResult *result) {
    TRACY_FUNC(sceNetCheckDialogGetResult, result);
    result->result = 0;
    return STUBBED("result->result = 0");
}

EXPORT(SceCommonDialogStatus, sceNetCheckDialogGetStatus) {
    TRACY_FUNC(sceNetCheckDialogGetStatus);
    if (emuenv.common_dialog.type != NETCHECK_DIALOG)
        return SCE_COMMON_DIALOG_STATUS_NONE;

    STUBBED("SCE_COMMON_DIALOG_STATUS_FINISHED");
    return emuenv.common_dialog.status;
}

EXPORT(int, sceNetCheckDialogInit) {
    TRACY_FUNC(sceNetCheckDialogInit);
    emuenv.common_dialog.type = NETCHECK_DIALOG;
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    return UNIMPLEMENTED();
}

EXPORT(int, sceNetCheckDialogTerm) {
    TRACY_FUNC(sceNetCheckDialogTerm);
    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogAbort) {
    TRACY_FUNC(sceNpFriendList2DialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogGetResult) {
    TRACY_FUNC(sceNpFriendList2DialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogGetStatus) {
    TRACY_FUNC(sceNpFriendList2DialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogInit) {
    TRACY_FUNC(sceNpFriendList2DialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendList2DialogTerm) {
    TRACY_FUNC(sceNpFriendList2DialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogAbort) {
    TRACY_FUNC(sceNpFriendListDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogGetResult) {
    TRACY_FUNC(sceNpFriendListDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogGetStatus) {
    TRACY_FUNC(sceNpFriendListDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogInit) {
    TRACY_FUNC(sceNpFriendListDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpFriendListDialogTerm) {
    TRACY_FUNC(sceNpFriendListDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogAbort) {
    TRACY_FUNC(sceNpMessageDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogGetResult) {
    TRACY_FUNC(sceNpMessageDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogGetStatus) {
    TRACY_FUNC(sceNpMessageDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogInit) {
    TRACY_FUNC(sceNpMessageDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpMessageDialogTerm) {
    TRACY_FUNC(sceNpMessageDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogAbort) {
    TRACY_FUNC(sceNpProfileDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogGetResult) {
    TRACY_FUNC(sceNpProfileDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogGetStatus) {
    TRACY_FUNC(sceNpProfileDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogInit) {
    TRACY_FUNC(sceNpProfileDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpProfileDialogTerm) {
    TRACY_FUNC(sceNpProfileDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogAbort) {
    TRACY_FUNC(sceNpSnsFacebookDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetResult) {
    TRACY_FUNC(sceNpSnsFacebookDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetResultLongToken) {
    TRACY_FUNC(sceNpSnsFacebookDialogGetResultLongToken);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogGetStatus) {
    TRACY_FUNC(sceNpSnsFacebookDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogInit) {
    TRACY_FUNC(sceNpSnsFacebookDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpSnsFacebookDialogTerm) {
    TRACY_FUNC(sceNpSnsFacebookDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpTrophySetupDialogAbort) {
    TRACY_FUNC(sceNpTrophySetupDialogAbort);
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetResult, Ptr<SceNpTrophySetupDialogResult> result) {
    TRACY_FUNC(sceNpTrophySetupDialogGetResult, result);
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    result.get(emuenv.mem)->result = emuenv.common_dialog.result;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    TRACY_FUNC(sceNpTrophySetupDialogGetStatus);
    return emuenv.common_dialog.status;
}

EXPORT(int, sceNpTrophySetupDialogInit, const Ptr<SceNpTrophySetupDialogParam> param) {
    TRACY_FUNC(sceNpTrophySetupDialogInit, param);
    if (emuenv.common_dialog.type != NO_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.type = TROPHY_SETUP_DIALOG;
    emuenv.common_dialog.trophy.tick = SDL_GetTicks() + ((param.get(emuenv.mem)->options & 0x01) ? 3000 : 0);
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    TRACY_FUNC(sceNpTrophySetupDialogTerm);
    if (emuenv.common_dialog.type != TROPHY_SETUP_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);

    if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    emuenv.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, scePhotoImportDialogAbort) {
    TRACY_FUNC(scePhotoImportDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogGetResult) {
    TRACY_FUNC(scePhotoImportDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogGetStatus) {
    TRACY_FUNC(scePhotoImportDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogInit) {
    TRACY_FUNC(scePhotoImportDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoImportDialogTerm) {
    TRACY_FUNC(scePhotoImportDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogAbort) {
    TRACY_FUNC(scePhotoReviewDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogGetResult) {
    TRACY_FUNC(scePhotoReviewDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogGetStatus) {
    TRACY_FUNC(scePhotoReviewDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogInit) {
    TRACY_FUNC(scePhotoReviewDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, scePhotoReviewDialogTerm) {
    TRACY_FUNC(scePhotoReviewDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogContinue) {
    TRACY_FUNC(scePspSaveDataDialogContinue);
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogGetResult) {
    TRACY_FUNC(scePspSaveDataDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogInit) {
    TRACY_FUNC(scePspSaveDataDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, scePspSaveDataDialogTerm) {
    TRACY_FUNC(scePspSaveDataDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogAbort) {
    TRACY_FUNC(sceRemoteOSKDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogGetResult) {
    TRACY_FUNC(sceRemoteOSKDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogGetStatus) {
    TRACY_FUNC(sceRemoteOSKDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogInit) {
    TRACY_FUNC(sceRemoteOSKDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceRemoteOSKDialogTerm) {
    TRACY_FUNC(sceRemoteOSKDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSaveDataDialogAbort) {
    TRACY_FUNC(sceSaveDataDialogAbort);
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

static void check_save_file(const uint32_t index, EmuEnvState &emuenv, const char *export_name) {
    emuenv.common_dialog.savedata.title[index].clear();
    emuenv.common_dialog.savedata.subtitle[index].clear();
    emuenv.common_dialog.savedata.icon_texture[index] = {};
    emuenv.common_dialog.savedata.has_date[index] = false;
    emuenv.common_dialog.savedata.slot_info[index].isExist = 0;
    auto &icon_buf_tmp = emuenv.common_dialog.savedata.icon_buffer[index];
    icon_buf_tmp.clear();

    SceUID fd = open_file(emuenv.io, construct_slotparam_path(emuenv.common_dialog.savedata.slot_id[index]).c_str(), SCE_O_RDONLY, emuenv.pref_path, export_name);
    if (fd < 0) {
        auto empty_param = emuenv.common_dialog.savedata.list_empty_param[index];
        if (empty_param) {
            emuenv.common_dialog.savedata.title[index] = empty_param->title ? empty_param->title.get(emuenv.mem) : emuenv.common_dialog.lang.save_data.save["new_saved_data"];
            const auto iconPath = empty_param->iconPath.get(emuenv.mem);
            SceUChar8 *iconBuf = empty_param->iconBuf.cast<SceUChar8>().get(emuenv.mem);
            const auto iconBufSize = empty_param->iconBufSize;
            if (iconPath) {
                auto device = device::get_device(iconPath);
                const auto thumbnail_path = translate_path(empty_param->iconPath.get(emuenv.mem), device, emuenv.io.device_paths);
                vfs::read_file(VitaIoDevice::ux0, icon_buf_tmp, emuenv.pref_path, thumbnail_path);
            } else if (iconBuf && (iconBufSize > 0)) {
                icon_buf_tmp.insert(icon_buf_tmp.end(), iconBuf, iconBuf + iconBufSize);
            }
        }
    } else {
        vfs::FileBuffer thumbnail_buffer;
        SceAppUtilSaveDataSlotParam slot_param{};
        read_file(&slot_param, emuenv.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
        close_file(emuenv.io, fd, export_name);
        emuenv.common_dialog.savedata.slot_info[index].isExist = 1;
        emuenv.common_dialog.savedata.title[index] = slot_param.title;
        emuenv.common_dialog.savedata.subtitle[index] = slot_param.subTitle;
        emuenv.common_dialog.savedata.details[index] = slot_param.detail;
        emuenv.common_dialog.savedata.date[index] = slot_param.modifiedTime;
        emuenv.common_dialog.savedata.has_date[index] = true;
        auto device = device::get_device(slot_param.iconPath);
        auto thumbnail_path = translate_path(slot_param.iconPath, device, emuenv.io.device_paths);
        vfs::read_file(device, thumbnail_buffer, emuenv.pref_path, thumbnail_path);
        icon_buf_tmp = thumbnail_buffer;
    }
}

static void handle_user_message(SceSaveDataDialogUserMessageParam *user_message, EmuEnvState &emuenv) {
    auto &common = emuenv.common_dialog.lang.common;
    switch (user_message->buttonType) {
    case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK:
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
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
    auto &lang = emuenv.common_dialog.lang;
    auto &common = lang.common;
    auto &save_data = lang.save_data;
    auto &deleting = save_data.deleting;
    auto &load = save_data.load;
    auto &save = save_data.save;

    switch (sys_message->sysMsgType) {
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NODATA:
        emuenv.common_dialog.savedata.msg = load["no_saved_data"];
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
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
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
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
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE:
        emuenv.common_dialog.savedata.msg = save["could_not_save"];
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK;
        break;
    case SCE_SAVEDATE_DIALOG_SYSMSG_TYPE_NODATA_IMPORT:
        emuenv.common_dialog.savedata.msg = "There is no saved data that can be used with this application.\n\
			If the saved data you want to use is on the memory card, select the \"import saved data\" icon on the LiveArea screen.";
        emuenv.common_dialog.savedata.btn_num = 1;
        emuenv.common_dialog.savedata.btn[0] = common["ok"];
        emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
        break;
    default: break;
    }
}

EXPORT(int, sceSaveDataDialogContinue, const SceSaveDataDialogParam *p) {
    TRACY_FUNC(sceSaveDataDialogContinue, p);
    if (p == nullptr) {
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
    emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    emuenv.common_dialog.savedata.has_progress_bar = false;

    SceSaveDataDialogListParam *list_param;
    SceSaveDataDialogUserMessageParam *user_message;
    SceSaveDataDialogSystemMessageParam *sys_message;
    SceSaveDataDialogErrorCodeParam *error_code;
    SceSaveDataDialogProgressBarParam *progress_bar;
    std::vector<SceAppUtilSaveDataSlot> slot_list;
    vfs::FileBuffer thumbnail_buffer;

    emuenv.common_dialog.savedata.mode = p->mode;
    emuenv.common_dialog.savedata.display_type = p->dispType == 0 ? emuenv.common_dialog.savedata.display_type : p->dispType;
    emuenv.common_dialog.savedata.userdata = p->userdata;

    auto &common = emuenv.common_dialog.lang.common;

    switch (emuenv.common_dialog.savedata.mode) {
    default:
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        switch (p->mode) {
        case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
            user_message = p->userMsgParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(emuenv.mem));
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = user_message->targetSlot.id;
            emuenv.common_dialog.savedata.list_empty_param[emuenv.common_dialog.savedata.selected_save] = user_message->targetSlot.emptyParam.get(emuenv.mem);

            handle_user_message(user_message, emuenv);
            break;
        case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
            sys_message = p->sysMsgParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = sys_message->targetSlot.id;
            emuenv.common_dialog.savedata.list_empty_param[emuenv.common_dialog.savedata.selected_save] = sys_message->targetSlot.emptyParam.get(emuenv.mem);

            handle_sys_message(sys_message, emuenv);
            break;
        case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
            error_code = p->errorCodeParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = error_code->targetSlot.id;
            emuenv.common_dialog.savedata.list_empty_param[emuenv.common_dialog.savedata.selected_save] = error_code->targetSlot.emptyParam.get(emuenv.mem);

            emuenv.common_dialog.savedata.btn_num = 1;
            emuenv.common_dialog.savedata.btn[0] = common["ok"];
            emuenv.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
            switch (emuenv.common_dialog.savedata.display_type) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while saving.\n({})", log_hex(error_code->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while loading.\n({})", log_hex(error_code->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                emuenv.common_dialog.savedata.msg = fmt::format("An error has occurred while deleting.\n({})", log_hex(error_code->errorCode));
                break;
            }
            break;
        case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
            emuenv.common_dialog.savedata.btn_num = 0;
            progress_bar = p->progressBarParam.get(emuenv.mem);
            emuenv.common_dialog.savedata.slot_id[emuenv.common_dialog.savedata.selected_save] = progress_bar->targetSlot.id;
            emuenv.common_dialog.savedata.has_progress_bar = true;
            emuenv.common_dialog.savedata.list_empty_param[emuenv.common_dialog.savedata.selected_save] = progress_bar->targetSlot.emptyParam.get(emuenv.mem);
            if (progress_bar->msg) {
                emuenv.common_dialog.savedata.msg = progress_bar->msg.cast<char>().get(emuenv.mem);
            } else {
                auto &lang = emuenv.common_dialog.lang;
                auto &save_data = lang.save_data;
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
        check_save_file(emuenv.common_dialog.savedata.selected_save, emuenv, export_name);
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_LIST;
        list_param = p->listParam.get(emuenv.mem);
        if (list_param->slotListSize > 0)
            emuenv.common_dialog.savedata.slot_list_size = list_param->slotListSize;
        slot_list.resize(list_param->slotListSize);
        for (std::uint32_t i = 0; i < list_param->slotListSize; i++) {
            slot_list[i] = list_param->slotList.get(emuenv.mem)[i];
            emuenv.common_dialog.savedata.slot_id[i] = slot_list[i].id;
            emuenv.common_dialog.savedata.list_empty_param[i] = slot_list[i].emptyParam.get(emuenv.mem);
            check_save_file(i, emuenv, export_name);
        }
        break;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogFinish, const SceSaveDataDialogFinishParam *finishParam) {
    TRACY_FUNC(sceSaveDataDialogFinish, finishParam);
    if (!finishParam) {
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

EXPORT(SceInt32, sceSaveDataDialogGetResult, SceSaveDataDialogResult *result) {
    TRACY_FUNC(sceSaveDataDialogGetResult, result);
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG)
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);

    if (!result) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (result->reserved[0] != 0) {
        return RET_ERROR(SCE_SAVEDATA_DIALOG_ERROR_PARAM);
    }

    if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_FINISHED || emuenv.common_dialog.substatus == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        auto &save_dialog = emuenv.common_dialog.savedata;
        auto result_slotinfo = result->slotInfo.get(emuenv.mem);

        result->mode = save_dialog.mode;
        result->result = emuenv.common_dialog.result;
        result->buttonId = save_dialog.button_id;

        if (!save_dialog.slot_id.empty()) {
            result->slotId = save_dialog.slot_id[save_dialog.selected_save];
        }
        if (result->slotInfo && !save_dialog.slot_info.empty()) {
            result_slotinfo->isExist = save_dialog.slot_info[save_dialog.selected_save].isExist;
            result_slotinfo->slotParam = save_dialog.slot_info[save_dialog.selected_save].slotParam;
        }
        result->userdata = save_dialog.userdata;
        return 0;
    } else {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_FINISHED);
    }
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    TRACY_FUNC(sceSaveDataDialogGetStatus);
    if ((emuenv.common_dialog.type != SAVEDATA_DIALOG) || ((emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) && (emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_RUNNING)))
        return SCE_COMMON_DIALOG_STATUS_NONE;

    return emuenv.common_dialog.status;
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    TRACY_FUNC(sceSaveDataDialogGetSubStatus);
    if (emuenv.common_dialog.type != SAVEDATA_DIALOG || emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return SCE_COMMON_DIALOG_STATUS_NONE;
    }
    return emuenv.common_dialog.substatus;
}

static void initialize_savedata_vectors(EmuEnvState &emuenv, unsigned int size) {
    emuenv.common_dialog.savedata.icon_texture.resize(size);
    emuenv.common_dialog.savedata.slot_info.resize(size);
    emuenv.common_dialog.savedata.title.resize(size);
    emuenv.common_dialog.savedata.subtitle.resize(size);
    emuenv.common_dialog.savedata.details.resize(size);
    emuenv.common_dialog.savedata.has_date.resize(size);
    emuenv.common_dialog.savedata.date.resize(size);
    emuenv.common_dialog.savedata.icon_buffer.resize(size);
    emuenv.common_dialog.savedata.slot_id.resize(size);
    emuenv.common_dialog.savedata.list_empty_param.resize(size);
}

EXPORT(int, sceSaveDataDialogInit, const SceSaveDataDialogParam *p) {
    TRACY_FUNC(sceSaveDataDialogInit, p);
    if (p == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (emuenv.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_BUSY);
    }

    emuenv.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;

    SceSaveDataDialogFixedParam *fixed_param;
    SceSaveDataDialogListParam *list_param;
    SceSaveDataDialogUserMessageParam *user_message;
    SceSaveDataDialogSystemMessageParam *sys_message;
    SceSaveDataDialogProgressBarParam *progress_bar;
    std::vector<SceAppUtilSaveDataSlot> slot_list;

    emuenv.common_dialog.savedata = {};

    emuenv.common_dialog.savedata.mode = p->mode;
    emuenv.common_dialog.savedata.mode_to_display = p->mode;
    emuenv.common_dialog.savedata.display_type = p->dispType;
    emuenv.common_dialog.savedata.userdata = p->userdata;

    switch (p->mode) {
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        fixed_param = p->fixedParam.get(emuenv.mem);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.list_empty_param[0] = fixed_param->targetSlot.emptyParam.get(emuenv.mem);
        emuenv.common_dialog.savedata.slot_id[0] = fixed_param->targetSlot.id;
        check_save_file(0, emuenv, export_name);
        emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        list_param = p->listParam.get(emuenv.mem);
        emuenv.common_dialog.savedata.slot_list_size = list_param->slotListSize;
        emuenv.common_dialog.savedata.list_style = list_param->itemStyle;

        slot_list.resize(list_param->slotListSize);
        initialize_savedata_vectors(emuenv, list_param->slotListSize);

        if (list_param->listTitle) {
            emuenv.common_dialog.savedata.list_title = list_param->listTitle.cast<char>().get(emuenv.mem);
        } else {
            auto &lang = emuenv.common_dialog.lang;
            auto &save_data = lang.save_data;
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
            emuenv.common_dialog.savedata.list_empty_param[i] = slot_list[i].emptyParam.get(emuenv.mem);
            check_save_file(i, emuenv, export_name);
        }
        break;
    case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
        user_message = p->userMsgParam.get(emuenv.mem);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.slot_id[0] = user_message->targetSlot.id;
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(emuenv.mem));
        emuenv.common_dialog.savedata.list_empty_param[0] = user_message->targetSlot.emptyParam.get(emuenv.mem);
        check_save_file(0, emuenv, export_name);

        handle_user_message(user_message, emuenv);
        break;
    case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
        sys_message = p->sysMsgParam.get(emuenv.mem);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.slot_id[0] = sys_message->targetSlot.id;
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        emuenv.common_dialog.savedata.list_empty_param[0] = sys_message->targetSlot.emptyParam.get(emuenv.mem);
        check_save_file(0, emuenv, export_name);

        handle_sys_message(sys_message, emuenv);
        break;
    case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
        progress_bar = p->progressBarParam.get(emuenv.mem);
        initialize_savedata_vectors(emuenv, 1);
        emuenv.common_dialog.savedata.btn_num = 0;
        emuenv.common_dialog.savedata.slot_id[0] = progress_bar->targetSlot.id;
        emuenv.common_dialog.savedata.has_progress_bar = true;
        emuenv.common_dialog.savedata.list_empty_param[0] = progress_bar->targetSlot.emptyParam.get(emuenv.mem);
        check_save_file(0, emuenv, export_name);

        if (progress_bar->msg.get(emuenv.mem) != nullptr) {
            emuenv.common_dialog.savedata.msg = reinterpret_cast<const char *>(progress_bar->msg.get(emuenv.mem));
        } else {
            auto &lang = emuenv.common_dialog.lang;
            auto &save_data = lang.save_data;
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
        LOG_ERROR("Attempt to initialize savedata dialog with unknown mode: {}", log_hex(p->mode));
        break;
    }
    emuenv.common_dialog.type = SAVEDATA_DIALOG;
    return 0;
}

EXPORT(int, sceSaveDataDialogProgressBarInc, SceSaveDataDialogProgressBarTarget target, SceUInt32 delta) {
    TRACY_FUNC(sceSaveDataDialogProgressBarInc, target, delta);
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
    TRACY_FUNC(sceSaveDataDialogProgressBarSetValue, target, rate);
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
    TRACY_FUNC(sceSaveDataDialogSubClose);
    if (emuenv.common_dialog.substatus != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    return 0;
}

EXPORT(int, sceSaveDataDialogTerm) {
    TRACY_FUNC(sceSaveDataDialogTerm);
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
    TRACY_FUNC(sceStoreCheckoutDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogGetResult) {
    TRACY_FUNC(sceStoreCheckoutDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogGetStatus) {
    TRACY_FUNC(sceStoreCheckoutDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogInit) {
    TRACY_FUNC(sceStoreCheckoutDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceStoreCheckoutDialogTerm) {
    TRACY_FUNC(sceStoreCheckoutDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogAbort) {
    TRACY_FUNC(sceTwDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogGetResult) {
    TRACY_FUNC(sceTwDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogGetStatus) {
    TRACY_FUNC(sceTwDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogInit) {
    TRACY_FUNC(sceTwDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwDialogTerm) {
    TRACY_FUNC(sceTwDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogAbort) {
    TRACY_FUNC(sceTwLoginDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogGetResult) {
    TRACY_FUNC(sceTwLoginDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogGetStatus) {
    TRACY_FUNC(sceTwLoginDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceTwLoginDialogTerm) {
    TRACY_FUNC(sceTwLoginDialogTerm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogAbort) {
    TRACY_FUNC(sceVideoImportDialogAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogGetResult) {
    TRACY_FUNC(sceVideoImportDialogGetResult);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogGetStatus) {
    TRACY_FUNC(sceVideoImportDialogGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogInit) {
    TRACY_FUNC(sceVideoImportDialogInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceVideoImportDialogTerm) {
    TRACY_FUNC(sceVideoImportDialogTerm);
    return UNIMPLEMENTED();
}
