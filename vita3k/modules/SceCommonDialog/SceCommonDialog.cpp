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
#include <host/app_util.h>
#include <host/functions.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/vfs.h>
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
    if (host.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceImeDialogGetResult, SceImeDialogResult *result) {
    result->result = host.common_dialog.result;
    result->button = host.common_dialog.ime.status;
    return 0;
}

EXPORT(int, sceImeDialogGetStatus) {
    return host.common_dialog.status;
}

EXPORT(int, sceImeDialogInit, const Ptr<SceImeDialogParam> param) {
    if (host.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    SceImeDialogParam *p = param.get(host.mem);

    std::u16string title = reinterpret_cast<char16_t *>(p->title.get(host.mem));
    std::u16string text = reinterpret_cast<char16_t *>(p->initialText.get(host.mem));

    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.type = IME_DIALOG;

    host.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_NONE;
    host.common_dialog.ime.title = string_utils::utf16_to_utf8(title);
    sprintf(host.common_dialog.ime.text, "%s", string_utils::utf16_to_utf8(text).c_str());
    host.common_dialog.ime.max_length = p->maxTextLength;
    host.common_dialog.ime.multiline = (p->option & SCE_IME_OPTION_MULTILINE);
    host.common_dialog.ime.cancelable = (p->dialogMode == SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL);
    host.common_dialog.ime.result = reinterpret_cast<uint16_t *>(p->inputTextBuffer.get(host.mem));

    return 0;
}

EXPORT(int, sceImeDialogTerm) {
    if (host.common_dialog.type != IME_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.common_dialog.type = NO_DIALOG;
    return 0;
}

EXPORT(int, sceMsgDialogAbort) {
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceMsgDialogClose) {
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    return 0;
}

EXPORT(int, sceMsgDialogGetResult, SceMsgDialogResult *result) {
    result->result = host.common_dialog.result;
    result->mode = host.common_dialog.msg.mode;
    result->buttonId = host.common_dialog.msg.status;
    return 0;
}

EXPORT(int, sceMsgDialogGetStatus) {
    return host.common_dialog.status;
}

EXPORT(int, sceMsgDialogInit, const Ptr<SceMsgDialogParam> param) {
    if (host.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    SceMsgDialogParam *p = param.get(host.mem);
    SceMsgDialogUserMessageParam *up;
    SceMsgDialogButtonsParam *bp;
    SceMsgDialogProgressBarParam *pp;

    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.type = MESSAGE_DIALOG;
    host.common_dialog.msg.has_progress_bar = false;

    host.common_dialog.msg.mode = p->mode;
    host.common_dialog.msg.status = SCE_MSG_DIALOG_BUTTON_ID_INVALID;
    switch (p->mode) {
    case SCE_MSG_DIALOG_MODE_USER_MSG:
        up = p->userMsgParam.get(host.mem);
        host.common_dialog.msg.message = reinterpret_cast<char *>(up->msg.get(host.mem));
        switch (up->buttonType) {
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
            host.common_dialog.msg.btn_num = 1;
            host.common_dialog.msg.btn[0] = "OK";
            host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
            host.common_dialog.msg.btn_num = 2;
            host.common_dialog.msg.btn[0] = "Yes";
            host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            host.common_dialog.msg.btn[1] = "No";
            host.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_NONE:
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
            host.common_dialog.msg.btn_num = 2;
            host.common_dialog.msg.btn[0] = "OK";
            host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_OK;
            host.common_dialog.msg.btn[1] = "Cancel";
            host.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS:
            bp = up->buttonParam.get(host.mem);
            host.common_dialog.msg.btn_num = 3;
            host.common_dialog.msg.btn[0] = bp->msg1.get(host.mem);
            host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_YES;
            host.common_dialog.msg.btn[1] = bp->msg2.get(host.mem);
            host.common_dialog.msg.btn_val[1] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            host.common_dialog.msg.btn[2] = bp->msg3.get(host.mem);
            host.common_dialog.msg.btn_val[2] = SCE_MSG_DIALOG_BUTTON_ID_RETRY;
            break;
        }
        break;
    case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
        switch (p->sysMsgParam.get(host.mem)->sysMsgType) {
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
            host.common_dialog.msg.message = "Please wait.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE:
            host.common_dialog.msg.message = "There is not enough free space on the memory card.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION:
            host.common_dialog.msg.message = "Move away from the source of interference, or adjust the compass by moving your PS Vita system as shown below.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
            host.common_dialog.msg.message = "Please wait.";
            host.common_dialog.msg.btn[0] = "Cancel";
            host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_NO;
            host.common_dialog.msg.btn_num = 1;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE:
            host.common_dialog.msg.message = "Cannot continue the application. No memory card is inserted.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION:
            host.common_dialog.msg.message = "Cannot perform this operation. No memory card is inserted.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED:
            host.common_dialog.msg.message = "You must enable the microphone.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION:
            host.common_dialog.msg.message = "You must use Wi-Fi to do this.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION:
            host.common_dialog.msg.message = "You must use Wi-Fi to use this application.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE:
            host.common_dialog.msg.message = "No content is available yet.";
            host.common_dialog.msg.btn_num = 0;
            break;
        case SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID:
        default:
            LOG_ERROR("Attempt to init message dialog with unknown system message mode: {}", log_hex(p->sysMsgParam.get(host.mem)->sysMsgType));
        }
        break;
    case SCE_MSG_DIALOG_MODE_ERROR_CODE:
        host.common_dialog.msg.message = fmt::format("An error occurred. Errorcode: {}", log_hex(p->errorCodeParam.get(host.mem)->errorCode));
        host.common_dialog.msg.btn_num = 0;
        break;
    case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
        pp = p->progBarParam.get(host.mem);
        host.common_dialog.msg.btn_num = 0;
        host.common_dialog.msg.has_progress_bar = true;
        if (pp->msg.get(host.mem) != nullptr) {
            host.common_dialog.msg.message = reinterpret_cast<char *>(pp->msg.get(host.mem));
        } else {
            switch (pp->sysMsgParam.sysMsgType) {
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
                host.common_dialog.msg.message = "Please Wait.";
                break;
            case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
                host.common_dialog.msg.message = "Please Wait";
                host.common_dialog.msg.btn_num = 1;
                host.common_dialog.msg.btn[0] = "CANCEL";
                host.common_dialog.msg.btn_val[0] = SCE_MSG_DIALOG_BUTTON_ID_NO;
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
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (host.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.msg.bar_rate += delta;
    return 0;
}

EXPORT(int, sceMsgDialogProgressBarSetMsg, SceMsgDialogProgressBarTarget target, const SceChar8 *msg) {
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (host.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.msg.message = reinterpret_cast<const char *>(msg);
    return 0;
}

EXPORT(int, sceMsgDialogProgressBarSetValue, SceMsgDialogProgressBarTarget target, SceUInt32 rate) {
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (host.common_dialog.msg.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.msg.bar_rate = rate;
    if (host.common_dialog.msg.bar_rate > 100) {
        host.common_dialog.msg.bar_rate = 100;
    }
    return 0;
}

EXPORT(int, sceMsgDialogTerm) {
    if (host.common_dialog.type != MESSAGE_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.common_dialog.type = NO_DIALOG;
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
    if (host.common_dialog.type != TROPHY_SETUP_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetResult, SceNpTrophySetupDialogResult *result) {
    result->result = host.common_dialog.result;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogGetStatus) {
    return host.common_dialog.status;
}

EXPORT(int, sceNpTrophySetupDialogInit, const Ptr<SceNpTrophySetupDialogParam> param) {
    if (host.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.type = TROPHY_SETUP_DIALOG;
    host.common_dialog.trophy.tick = SDL_GetTicks() + (param.get(host.mem)->options & 0x01) ? 3000 : 0;
    return 0;
}

EXPORT(int, sceNpTrophySetupDialogTerm) {
    if (host.common_dialog.type != TROPHY_SETUP_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.common_dialog.type = NO_DIALOG;
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
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
    host.common_dialog.type = NO_DIALOG;
    return 0;
}

static void check_empty_param(HostState &host, const SceAppUtilSaveDataSlotEmptyParam *empty_param) {
    vfs::FileBuffer thumbnail_buffer;
    host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save] = "";
    host.common_dialog.savedata.subtitle[host.common_dialog.savedata.selected_save] = "";
    host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save].clear();
    host.common_dialog.savedata.has_date[host.common_dialog.savedata.selected_save] = false;
    if (empty_param != nullptr) {
        host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save] = empty_param->title.get(host.mem) == nullptr ? "New Saved Data" : empty_param->title.get(host.mem);
        auto device = device::get_device(empty_param->iconPath.get(host.mem));
        auto thumbnail_path = translate_path(empty_param->iconPath.get(host.mem), device, host.io.device_paths);
        vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, host.pref_path, thumbnail_path);
        host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save] = thumbnail_buffer;
        host.common_dialog.savedata.icon_loaded[0] = true;
    }
}

static void check_save_file(SceUID fd, std::vector<SceAppUtilSaveDataSlotParam> slot_param, int index, HostState &host, const char *export_name) {
    vfs::FileBuffer thumbnail_buffer;
    if (fd < 0) {
        host.common_dialog.savedata.slot_info[index].isExist = 0;
        host.common_dialog.savedata.title[index] = "";
        host.common_dialog.savedata.subtitle[index] = "";
        host.common_dialog.savedata.details[index] = "";
        host.common_dialog.savedata.icon_buffer[index].clear();
        host.common_dialog.savedata.has_date[index] = false;
        auto empty_param = host.common_dialog.savedata.list_empty_param;
        if (empty_param != nullptr) {
            host.common_dialog.savedata.title[index] = empty_param->title.get(host.mem) == nullptr ? "New Saved Data" : empty_param->title.get(host.mem);
            auto device = device::get_device(empty_param->iconPath.get(host.mem));
            auto thumbnail_path = translate_path(empty_param->iconPath.get(host.mem), device, host.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, host.pref_path, thumbnail_path);
            host.common_dialog.savedata.icon_buffer[index] = thumbnail_buffer;
            host.common_dialog.savedata.icon_loaded[index] = true;
        }
    } else {
        read_file(&slot_param[index], host.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
        close_file(host.io, fd, export_name);
        host.common_dialog.savedata.slot_info[index].isExist = 1;
        host.common_dialog.savedata.title[index] = slot_param[index].title;
        host.common_dialog.savedata.subtitle[index] = slot_param[index].subTitle;
        host.common_dialog.savedata.details[index] = slot_param[index].detail;
        host.common_dialog.savedata.date[index] = slot_param[index].modifiedTime;
        host.common_dialog.savedata.has_date[index] = true;
        auto device = device::get_device(slot_param[index].iconPath);
        auto thumbnail_path = translate_path(slot_param[index].iconPath, device, host.io.device_paths);
        vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, host.pref_path, thumbnail_path);
        host.common_dialog.savedata.icon_buffer[index] = thumbnail_buffer;
        host.common_dialog.savedata.icon_loaded[index] = true;
    }
}

EXPORT(int, sceSaveDataDialogContinue, const Ptr<SceSaveDataDialogParam> param) {
    if (param.get(host.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.savedata.has_progress_bar = false;

    const SceSaveDataDialogParam *p = param.get(host.mem);
    SceSaveDataDialogUserMessageParam *user_message;
    SceSaveDataDialogSystemMessageParam *sys_message;
    SceSaveDataDialogProgressBarParam *progress_bar;
    SceAppUtilSaveDataSlotEmptyParam *empty_param;
    std::vector<SceAppUtilSaveDataSlotParam> slot_param;
    vfs::FileBuffer thumbnail_buffer;
    SceUID fd;

    host.common_dialog.savedata.mode = p->mode;
    host.common_dialog.savedata.display_type = p->dispType == 0 ? host.common_dialog.savedata.display_type : p->dispType;
    host.common_dialog.savedata.userdata = p->userdata;

    switch (host.common_dialog.savedata.mode) {
    default:
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        host.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        slot_param.resize(1);
        host.common_dialog.savedata.icon_loaded.resize(1);
        fd = open_file(host.io, construct_slotparam_path(host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save]).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
        if (fd < 0) {
            host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist = 0;
        } else {
            host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist = 1;
            read_file(&slot_param[0], host.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
            close_file(host.io, fd, export_name);
            host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save] = slot_param[0].title;
            host.common_dialog.savedata.subtitle[host.common_dialog.savedata.selected_save] = slot_param[0].subTitle;
            host.common_dialog.savedata.details[host.common_dialog.savedata.selected_save] = slot_param[0].detail;
            host.common_dialog.savedata.date[host.common_dialog.savedata.selected_save] = slot_param[0].modifiedTime;
            host.common_dialog.savedata.has_date[host.common_dialog.savedata.selected_save] = true;
            auto device = device::get_device(slot_param[0].iconPath);
            auto thumbnail_path = translate_path(slot_param[0].iconPath, device, host.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, host.pref_path, thumbnail_path);
            host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save] = thumbnail_buffer;
            host.common_dialog.savedata.icon_loaded[0] = true;
        }
        switch (p->mode) {
        case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
            user_message = p->userMsgParam.get(host.mem);
            host.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(host.mem));
            host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save] = user_message->targetSlot.id;
            if (!host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist) {
                empty_param = user_message->targetSlot.emptyParam.get(host.mem);
                check_empty_param(host, empty_param);
            }

            switch (user_message->buttonType) {
            case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK:
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
                break;
            case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO:
                host.common_dialog.savedata.btn_num = 2;
                host.common_dialog.savedata.btn[0] = "NO";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
                host.common_dialog.savedata.btn[1] = "YES";
                host.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
                break;
            case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_NONE:
                host.common_dialog.savedata.btn_num = 0;
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
                break;
            }
            break;
        case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
            sys_message = p->sysMsgParam.get(host.mem);
            host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save] = sys_message->targetSlot.id;
            if (!host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist) {
                empty_param = sys_message->targetSlot.emptyParam.get(host.mem);
                check_empty_param(host, empty_param);
            }
            switch (sys_message->sysMsgType) {
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NODATA:
                host.common_dialog.savedata.msg = "There is no saved data.";
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM:
                switch (host.common_dialog.savedata.display_type) {
                case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                    host.common_dialog.savedata.msg = "Do you want to save the data?";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                    host.common_dialog.savedata.msg = "Do you want to load this saved data?";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                    host.common_dialog.savedata.msg = "Do you want to delete this saved data?";
                    break;
                }
                host.common_dialog.savedata.btn_num = 2;
                host.common_dialog.savedata.btn[0] = "NO";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
                host.common_dialog.savedata.btn[1] = "YES";
                host.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_OVERWRITE:
                host.common_dialog.savedata.msg = "Do you want to overwrite this saved data?";
                host.common_dialog.savedata.btn_num = 2;
                host.common_dialog.savedata.btn[0] = "NO";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
                host.common_dialog.savedata.btn[1] = "YES";
                host.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE:
                host.common_dialog.savedata.msg = "There is not enough free space on the memory card.";
                host.common_dialog.savedata.btn_num = 0;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_PROGRESS:
                host.common_dialog.savedata.btn_num = 0;
                switch (host.common_dialog.savedata.display_type) {
                case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                    host.common_dialog.savedata.msg = "Saving...\nDo not power off the system or close the application";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                    host.common_dialog.savedata.msg = "Loading...";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                    host.common_dialog.savedata.msg = "Please Wait...";
                    break;
                }
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FINISHED:
                switch (host.common_dialog.savedata.display_type) {
                case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                    host.common_dialog.savedata.msg = "Saving complete.";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                    host.common_dialog.savedata.msg = "Loading complete.";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                    host.common_dialog.savedata.msg = "Deletion complete.";
                    break;
                }
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM_CANCEL:
                switch (host.common_dialog.savedata.display_type) {
                case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                    host.common_dialog.savedata.msg = "Do you want to cancel saving?";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                    host.common_dialog.savedata.msg = "Do you want to cancel loading?";
                    break;
                case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                    host.common_dialog.savedata.msg = "Do you want to cancel deleting?";
                    break;
                }
                host.common_dialog.savedata.btn_num = 2;
                host.common_dialog.savedata.btn[0] = "NO";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
                host.common_dialog.savedata.btn[1] = "YES";
                host.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FILE_CORRUPTED:
                host.common_dialog.savedata.msg = "The file is corrupt";
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
                break;
            case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE:
                host.common_dialog.savedata.msg = "Could not save the file.\n\
				There is not enough free space on the memory card.";
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK;
                break;
            case SCE_SAVEDATE_DIALOG_SYSMSG_TYPE_NODATA_IMPORT:
                host.common_dialog.savedata.msg = "There is no saved data that can be used with this application.\n\
				If the saved data you want to use is on the memory card, select the \"import saved data\" icon on the LiveArea screen.";
                host.common_dialog.savedata.btn_num = 1;
                host.common_dialog.savedata.btn[0] = "OK";
                host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
                break;
            default:
                LOG_ERROR("Attempt to continue savedata dialog with unknown system message mode: {}", log_hex(sys_message->sysMsgType));
                break;
            }
            break;
        case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
            host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save] = p->errorCodeParam.get(host.mem)->targetSlot.id;
            if (!host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist) {
                empty_param = p->errorCodeParam.get(host.mem)->targetSlot.emptyParam.get(host.mem);
                check_empty_param(host, empty_param);
            }
            host.common_dialog.savedata.btn_num = 1;
            host.common_dialog.savedata.btn[0] = "OK";
            host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
            switch (host.common_dialog.savedata.display_type) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                host.common_dialog.savedata.msg = fmt::format("An error has occurred while saving.\n({})", log_hex(p->errorCodeParam.get(host.mem)->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                host.common_dialog.savedata.msg = fmt::format("An error has occurred while loading.\n({})", log_hex(p->errorCodeParam.get(host.mem)->errorCode));
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                host.common_dialog.savedata.msg = fmt::format("An error has occurred while deleting.\n({})", log_hex(p->errorCodeParam.get(host.mem)->errorCode));
                break;
            }
            break;
        case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
            progress_bar = p->progressBarParam.get(host.mem);
            host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save] = progress_bar->targetSlot.id;
            host.common_dialog.savedata.has_progress_bar = true;
            if (!host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist) {
                empty_param = progress_bar->targetSlot.emptyParam.get(host.mem);
                check_empty_param(host, empty_param);
            }
            if (progress_bar->msg.get(host.mem) != nullptr) {
                host.common_dialog.savedata.msg = reinterpret_cast<const char *>(progress_bar->msg.get(host.mem));
            } else {
                switch (progress_bar->sysMsgParam.sysMsgType) {
                case SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_PROGRESS:
                    switch (host.common_dialog.savedata.display_type) {
                    case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                        host.common_dialog.savedata.msg = "Saving...";
                        break;
                    case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                        host.common_dialog.savedata.msg = "Loading...";
                        break;
                    case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                        host.common_dialog.savedata.msg = "Please Wait.";
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
        host.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_LIST;
        slot_param.resize(host.common_dialog.savedata.slot_list_size);
        host.common_dialog.savedata.icon_loaded.resize(host.common_dialog.savedata.slot_list_size);
        for (int i = 0; i < host.common_dialog.savedata.slot_list_size; i++) {
            fd = open_file(host.io, construct_slotparam_path(host.common_dialog.savedata.slot_id[i]).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
            check_save_file(fd, slot_param, i, host, export_name);
        }
        break;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogFinish) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

EXPORT(int, sceSaveDataDialogGetResult, Ptr<SceSaveDataDialogResult> result) {
    if (result.get(host.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    result.get(host.mem)->mode = host.common_dialog.savedata.mode;
    result.get(host.mem)->result = host.common_dialog.result;
    result.get(host.mem)->buttonId = host.common_dialog.savedata.button_id;
    result.get(host.mem)->slotId = host.common_dialog.savedata.slot_id[host.common_dialog.savedata.selected_save];
    result.get(host.mem)->slotInfo.get(host.mem)->isExist = host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].isExist;
    result.get(host.mem)->slotInfo.get(host.mem)->slotParam = host.common_dialog.savedata.slot_info[host.common_dialog.savedata.selected_save].slotParam;
    result.get(host.mem)->userdata = host.common_dialog.savedata.userdata;
    return 0;
}

EXPORT(int, sceSaveDataDialogGetStatus) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return SCE_COMMON_DIALOG_STATUS_NONE;
    }
    return host.common_dialog.status;
}

EXPORT(int, sceSaveDataDialogGetSubStatus) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return SCE_COMMON_DIALOG_STATUS_NONE;
    }
    return host.common_dialog.substatus;
}

EXPORT(int, sceSaveDataDialogInit, const Ptr<SceSaveDataDialogParam> param) {
    if (param.get(host.mem) == nullptr) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NULL);
    }

    if (host.common_dialog.type != NO_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }

    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;
    host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_RUNNING;

    const SceSaveDataDialogParam *p = param.get(host.mem);
    SceSaveDataDialogFixedParam *fixed_param;
    SceSaveDataDialogListParam *list_param;
    SceSaveDataDialogUserMessageParam *user_message;
    std::vector<SceAppUtilSaveDataSlot> slot_list;
    std::vector<SceAppUtilSaveDataSlotParam> slot_param;
    std::string thumbnail_path;
    SceUID fd;

    host.common_dialog.savedata.mode = p->mode;
    host.common_dialog.savedata.mode_to_display = p->mode;
    host.common_dialog.savedata.display_type = p->dispType;
    host.common_dialog.savedata.userdata = p->userdata;

    switch (p->mode) {
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        fixed_param = p->fixedParam.get(host.mem);
        slot_param.resize(1);
        host.common_dialog.savedata.icon_loaded.resize(1);
        host.common_dialog.savedata.slot_info.resize(1);
        host.common_dialog.savedata.title.resize(1);
        host.common_dialog.savedata.subtitle.resize(1);
        host.common_dialog.savedata.details.resize(1);
        host.common_dialog.savedata.has_date.resize(1);
        host.common_dialog.savedata.date.resize(1);
        host.common_dialog.savedata.icon_buffer.resize(1);
        host.common_dialog.savedata.slot_id.resize(1);
        host.common_dialog.savedata.slot_id[0] = fixed_param->targetSlot.id;
        fd = open_file(host.io, construct_slotparam_path(fixed_param->targetSlot.id).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
        if (fd < 0) {
            host.common_dialog.savedata.slot_info[0].isExist = 0;
            host.common_dialog.savedata.title[0] = "";
            host.common_dialog.savedata.subtitle[0] = "";
            host.common_dialog.savedata.details[0] = "";
            host.common_dialog.savedata.has_date[0] = false;
        } else {
            host.common_dialog.savedata.slot_info[0].isExist = 1;
            read_file(&slot_param[0], host.io, fd, sizeof(SceAppUtilSaveDataSlotParam), export_name);
            close_file(host.io, fd, export_name);
            host.common_dialog.savedata.title[0] = slot_param[0].title;
            host.common_dialog.savedata.subtitle[0] = slot_param[0].subTitle;
            host.common_dialog.savedata.details[0] = slot_param[0].detail;
            host.common_dialog.savedata.date[0] = slot_param[0].modifiedTime;
            vfs::FileBuffer thumbnail_buffer;
            auto device = device::get_device(slot_param[0].iconPath);
            auto thumbnail_path = translate_path(slot_param[0].iconPath, device, host.io.device_paths);
            vfs::read_file(VitaIoDevice::ux0, thumbnail_buffer, host.pref_path, thumbnail_path);
            host.common_dialog.savedata.icon_buffer[0] = thumbnail_buffer;
            host.common_dialog.savedata.icon_loaded[0] = true;
        }
        host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        list_param = p->listParam.get(host.mem);
        host.common_dialog.savedata.slot_list_size = list_param->slotListSize;
        host.common_dialog.savedata.list_style = list_param->itemStyle;

        slot_param.resize(list_param->slotListSize);
        slot_list.resize(list_param->slotListSize);
        host.common_dialog.savedata.title.resize(list_param->slotListSize);
        host.common_dialog.savedata.subtitle.resize(list_param->slotListSize);
        host.common_dialog.savedata.details.resize(list_param->slotListSize);
        host.common_dialog.savedata.slot_info.resize(list_param->slotListSize);
        host.common_dialog.savedata.slot_id.resize(list_param->slotListSize);
        host.common_dialog.savedata.date.resize(list_param->slotListSize);
        host.common_dialog.savedata.has_date.resize(list_param->slotListSize);
        host.common_dialog.savedata.icon_buffer.resize(list_param->slotListSize);
        host.common_dialog.savedata.icon_loaded.resize(list_param->slotListSize);

        if (list_param->listTitle.get(host.mem) != nullptr) {
            host.common_dialog.savedata.list_title = reinterpret_cast<const char *>(list_param->listTitle.get(host.mem));
        } else {
            switch (p->dispType) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                host.common_dialog.savedata.list_title = "Save";
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
                host.common_dialog.savedata.list_title = "Load";
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                host.common_dialog.savedata.list_title = "Delete";
                break;
            }
        }

        for (int i = 0; i < list_param->slotListSize; i++) {
            slot_list[i] = list_param->slotList.get(host.mem)[i];
            host.common_dialog.savedata.slot_id[i] = slot_list[i].id;
            host.common_dialog.savedata.list_empty_param = slot_list[0].emptyParam.get(host.mem);
            fd = open_file(host.io, construct_slotparam_path(slot_list[i].id).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
            check_save_file(fd, slot_param, i, host, export_name);
        }
        break;
    case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
        user_message = p->userMsgParam.get(host.mem);
        slot_param.resize(1);
        host.common_dialog.savedata.icon_loaded.resize(1);
        host.common_dialog.savedata.slot_info.resize(1);
        host.common_dialog.savedata.title.resize(1);
        host.common_dialog.savedata.subtitle.resize(1);
        host.common_dialog.savedata.details.resize(1);
        host.common_dialog.savedata.has_date.resize(1);
        host.common_dialog.savedata.date.resize(1);
        host.common_dialog.savedata.icon_buffer.resize(1);
        host.common_dialog.savedata.slot_id.resize(1);
        host.common_dialog.savedata.slot_id[0] = user_message->targetSlot.id;
        host.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        host.common_dialog.savedata.msg = reinterpret_cast<const char *>(user_message->msg.get(host.mem));

        fd = open_file(host.io, construct_slotparam_path(user_message->targetSlot.id).c_str(), SCE_O_RDONLY, host.pref_path, export_name);
        check_save_file(fd, slot_param, 0, host, export_name);

        switch (user_message->buttonType) {
        case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK:
            host.common_dialog.savedata.btn_num = 1;
            host.common_dialog.savedata.btn[0] = "OK";
            host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
            break;
        case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO:
            host.common_dialog.savedata.btn_num = 2;
            host.common_dialog.savedata.btn[0] = "NO";
            host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
            host.common_dialog.savedata.btn[1] = "YES";
            host.common_dialog.savedata.btn_val[1] = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
            break;
        case SCE_SAVEDATA_DIALOG_BUTTON_TYPE_NONE:
            host.common_dialog.savedata.btn_num = 0;
            host.common_dialog.savedata.btn_val[0] = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
            break;
        }
        break;
    default:
        LOG_ERROR("Attempt to initialize savedata dialog with unknown mode: {}", log_hex(p->mode));
        break;
    }
    host.common_dialog.type = SAVEDATA_DIALOG;
    return 0;
}

EXPORT(int, sceSaveDataDialogProgressBarInc, SceSaveDataDialogProgressBarTarget target, SceUInt32 delta) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (host.common_dialog.savedata.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.savedata.bar_rate += delta;
    return 0;
}

EXPORT(int, sceSaveDataDialogProgressBarSetValue, SceSaveDataDialogProgressBarTarget target, SceUInt32 rate) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_RUNNING);
    }

    if (host.common_dialog.savedata.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED);
    }
    host.common_dialog.savedata.bar_rate = rate;
    if (host.common_dialog.savedata.bar_rate > 100) {
        host.common_dialog.savedata.bar_rate = 100;
    }
    return 0;
}

EXPORT(int, sceSaveDataDialogSubClose) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    host.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
    return 0;
}

EXPORT(int, sceSaveDataDialogTerm) {
    if (host.common_dialog.type != SAVEDATA_DIALOG) {
        return RET_ERROR(SCE_COMMON_DIALOG_ERROR_NOT_IN_USE);
    }
    host.common_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
    host.common_dialog.type = NO_DIALOG;
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
