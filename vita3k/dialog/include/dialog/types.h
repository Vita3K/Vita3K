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

#pragma once

#include <host/app_util.h>
#include <mem/ptr.h>

#define SCE_SAVEDATA_DIALOG_ERROR_PARAM 0x80100b01

enum SceSaveDataDialogMode {
    SCE_SAVEDATA_DIALOG_MODE_FIXED = 1,
    SCE_SAVEDATA_DIALOG_MODE_LIST = 2,
    SCE_SAVEDATA_DIALOG_MODE_USER_MSG = 3,
    SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG = 4,
    SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE = 5,
    SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR = 6
};

enum SceSaveDataDialogFocusPos {
    SCE_SAVEDATA_DIALOG_FOCUS_POS_LISTHEAD = 0,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_LISTTAIL = 1,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_EMPTYHEAD = 2,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_EMPTYTAIL = 3,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATAHEAD = 4,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATATAIL = 5,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATALATEST = 6,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATAOLDEST = 7,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_ID = 8
};

enum SceSaveDataDialogListItemStyle {
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE_SUBTITLE = 0,
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_SUBTITLE_DATE = 1,
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE = 2
};

enum SceSaveDataDialogType {
    SCE_SAVEDATA_DIALOG_TYPE_SAVE = 1,
    SCE_SAVEDATA_DIALOG_TYPE_LOAD = 2,
    SCE_SAVEDATA_DIALOG_TYPE_DELETE = 3
};

enum SceSaveDataDialogButtonType {
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK = 0,
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_NONE = 2
};

enum SceSaveDataDialogSystemMessageType {
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NODATA = 1,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM = 2,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_OVERWRITE = 3,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE = 4,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_PROGRESS = 5,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FINISHED = 6,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_CONFIRM_CANCEL = 7,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FILE_CORRUPTED = 8,
    SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE = 9,
    SCE_SAVEDATE_DIALOG_SYSMSG_TYPE_NODATA_IMPORT = 12
};

enum SceSaveDataDialogProgressBarType {
    SCE_SAVEDATA_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE = 0
};

enum SceSaveDataDialogEnvFlag {
    SCE_SAVEDATA_DIALOG_ENV_FLAG_DEFAULT = 0
};

enum SceSaveDataDialogButtonId {
    SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID = 0,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_OK = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_YES = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_NO = 2
};

enum SceSaveDataDialogProgressBarTarget {
    SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT = 0
};

enum SceSaveDataDialogFinishFlag {
    SCE_SAVEDATA_DIALOG_FINISH_FLAG_DEFAULT = 0
};

enum SceCommonDialogStatus {
    SCE_COMMON_DIALOG_STATUS_NONE = 0,
    SCE_COMMON_DIALOG_STATUS_RUNNING = 1,
    SCE_COMMON_DIALOG_STATUS_FINISHED = 2
};

enum SceCommonDialogResult {
    SCE_COMMON_DIALOG_RESULT_OK,
    SCE_COMMON_DIALOG_RESULT_USER_CANCELED,
    SCE_COMMON_DIALOG_RESULT_ABORTED
};

enum SceCommonDialogErrorCode {
    SCE_COMMON_DIALOG_ERROR_BUSY = 0x80020401,
    SCE_COMMON_DIALOG_ERROR_NULL = 0x80020402,
    SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT = 0x80020403,
    SCE_COMMON_DIALOG_ERROR_NOT_RUNNING = 0x80020404,
    SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED = 0x80020405,
    SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD = 0x80020406,
    SCE_COMMON_DIALOG_ERROR_NOT_CONFIGURED = 0x80020407,
    SCE_COMMON_DIALOG_ERROR_NOT_AVAILABLE = 0x80020408,
    SCE_COMMON_DIALOG_ERROR_NOT_FINISHED = 0x80020410,
    SCE_COMMON_DIALOG_ERROR_NOT_IN_USE = 0x80020411,
    SCE_COMMON_DIALOG_ERROR_INVALID_COLOR_FORMAT = 0x80020420,
    SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_RESOLUTION = 0x80020421,
    SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_STRIDE = 0x80020422,
    SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_TYPE = 0x80020423,
    SCE_COMMON_DIALOG_ERROR_WITHIN_SCENE = 0x80020424,
    SCE_COMMON_DIALOG_ERROR_IME_IN_USE = 0x80020430,
    SCE_COMMON_DIALOG_ERROR_INVALID_LANGUAGE = 0x80020431,
    SCE_COMMON_DIALOG_ERROR_INVALID_ENTER_BUTTON_ASSIGN = 0x80020432,
    SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM = 0x80020433,
    SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR = 0x80020434,
    SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR = 0x80020435,
    SCE_COMMON_DIALOG_ERROR_GXM_IS_UNINITIALIZED = 0x80020436,
    SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL = 0x8002047F
};

#define SCE_IME_DIALOG_MAX_TITLE_LENGTH 128
#define SCE_IME_DIALOG_MAX_TEXT_LENGTH 2048

enum SceImeDialogButton {
    SCE_IME_DIALOG_BUTTON_NONE = 0,
    SCE_IME_DIALOG_BUTTON_CLOSE = 1,
    SCE_IME_DIALOG_BUTTON_ENTER = 2
};

enum SceImeOption {
    SCE_IME_OPTION_MULTILINE = 0x01,
    SCE_IME_OPTION_NO_AUTO_CAPITALIZATION = 0x02,
    SCE_IME_OPTION_NO_ASSISTANCE = 0x04
};

enum SceImeDialogDialogMode {
    SCE_IME_DIALOG_DIALOG_MODE_DEFAULT = 0,
    SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL = 1
};

enum SceMsgDialogButtonId {
    SCE_MSG_DIALOG_BUTTON_ID_INVALID = 0,
    SCE_MSG_DIALOG_BUTTON_ID_OK = 1,
    SCE_MSG_DIALOG_BUTTON_ID_YES = 1,
    SCE_MSG_DIALOG_BUTTON_ID_NO = 2,
    SCE_MSG_DIALOG_BUTTON_ID_RETRY = 3,
    SCE_MSG_DIALOG_BUTTON_ID_BUTTON1 = 1,
    SCE_MSG_DIALOG_BUTTON_ID_BUTTON2 = 2,
    SCE_MSG_DIALOG_BUTTON_ID_BUTTON3 = 3
};

enum SceMsgDialogSystemMessageType {
    SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID = 0,
    SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT = 1,
    SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE = 2,
    SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION = 3,
    SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED = 4,
    SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL = 5,
    SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL = 6,
    SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE = 7,
    SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION = 8,
    SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED = 100,
    SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION = 101,
    SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION = 102,
    SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE = 103
};

enum SceMsgDialogProgressBarTarget {
    SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT = 0
};

enum SceMsgDialogButtonType {
    SCE_MSG_DIALOG_BUTTON_TYPE_OK = 0,
    SCE_MSG_DIALOG_BUTTON_TYPE_YESNO = 1,
    SCE_MSG_DIALOG_BUTTON_TYPE_NONE = 2,
    SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL = 3,
    SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL = 4,
    SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS = 5
};

typedef enum SceMsgDialogMode {
    SCE_MSG_DIALOG_MODE_INVALID = 0,
    SCE_MSG_DIALOG_MODE_USER_MSG = 1,
    SCE_MSG_DIALOG_MODE_SYSTEM_MSG = 2,
    SCE_MSG_DIALOG_MODE_ERROR_CODE = 3,
    SCE_MSG_DIALOG_MODE_PROGRESS_BAR = 4
} SceMsgDialogMode;

struct SceCommonDialogInfobarParam {
    SceInt32 visibility;
    SceInt32 color;
    SceInt32 transparency;
    SceUInt8 reserved[32];
};

struct SceCommonDialogColor {
    SceUInt8 r;
    SceUInt8 g;
    SceUInt8 b;
    SceUInt8 a;
};

struct SceCommonDialogParam {
    Ptr<SceCommonDialogInfobarParam> infobarParam;
    Ptr<SceCommonDialogColor> bgColor;
    Ptr<SceCommonDialogColor> dimmerColor;
    SceUInt8 reserved[60];
    SceUInt32 magic;
};

struct SceImeDialogParam {
    SceUInt32 sdkVersion;

    SceUInt32 inputMethod;
    SceUInt64 supportedLanguages;
    SceBool languagesForced;
    SceUInt32 type;
    SceUInt32 option;
    SceUInt32 filter;

    SceUInt32 dialogMode;
    SceUInt32 textBoxMode;
    const Ptr<SceWChar16> title;
    SceUInt32 maxTextLength;
    Ptr<SceWChar16> initialText;
    Ptr<SceWChar16> inputTextBuffer;

    SceCommonDialogParam commonParam;

    SceUChar8 enterLabel;
    SceChar8 reserved[35];
};

struct SceImeDialogResult {
    SceInt32 result;
    SceInt32 button;
    SceChar8 reserved[28];
};

struct SceNpTrophySetupDialogParam {
    SceUInt32 sdkVersion;

    SceCommonDialogParam commonParam;
    SceUInt32 context;
    SceUInt32 options;

    SceChar8 reserved[128];
};

struct SceMsgDialogButtonsParam {
    const Ptr<char> msg1;
    SceInt32 fontSize1;
    const Ptr<char> msg2;
    SceInt32 fontSize2;
    const Ptr<char> msg3;
    SceInt32 fontSize3;
    SceChar8 reserved[32];
};

struct SceMsgDialogResult {
    SceInt32 mode;
    SceInt32 result;
    SceInt32 buttonId;
    SceChar8 reserved[32];
};

struct SceNpTrophySetupDialogResult {
    SceInt32 result;
    SceUInt8 reserved[128];
};

struct SceMsgDialogUserMessageParam {
    SceInt32 buttonType;
    const Ptr<SceChar8> msg;
    Ptr<SceMsgDialogButtonsParam> buttonParam;
    SceChar8 reserved[28];
};

struct SceMsgDialogSystemMessageParam {
    SceInt32 sysMsgType;
    SceInt32 value;
    SceChar8 reserved[32];
};

struct SceMsgDialogErrorCodeParam {
    SceInt32 errorCode;
    SceChar8 reserved[32];
};

struct SceMsgDialogProgressBarParam {
    SceInt32 barType;
    SceMsgDialogSystemMessageParam sysMsgParam;
    const Ptr<SceChar8> msg;
    SceInt32 reserved[8];
};

struct SceMsgDialogParam {
    SceUInt32 sdkVersion;
    SceCommonDialogParam commonParam;
    SceInt32 mode;
    Ptr<SceMsgDialogUserMessageParam> userMsgParam;
    Ptr<SceMsgDialogSystemMessageParam> sysMsgParam;
    Ptr<SceMsgDialogErrorCodeParam> errorCodeParam;
    Ptr<SceMsgDialogProgressBarParam> progBarParam;
    SceInt32 flag;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogFixedParam {
    SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogListParam {
    const Ptr<SceAppUtilSaveDataSlot> slotList;
    SceUInt slotListSize;
    SceSaveDataDialogFocusPos focusPos;
    SceAppUtilSaveDataSlotId focusId;
    const Ptr<SceChar8> listTitle;
    SceSaveDataDialogListItemStyle itemStyle;
    SceChar8 reserved[28];
};

struct SceSaveDataDialogUserMessageParam {
    SceSaveDataDialogButtonType buttonType;
    const Ptr<SceChar8> msg;
    SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogSystemMessageParam {
    SceSaveDataDialogSystemMessageType sysMsgType;
    SceInt32 value;
    SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogErrorCodeParam {
    SceInt32 errorCode;
    SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogFinishParam {
    SceSaveDataDialogFinishFlag flag;
    SceChar8 reserved[36];
};

struct SceAppUtilMountPoint {
    int8_t data[16];
};

struct SceSaveDataDialogProgressBarParam {
    SceSaveDataDialogProgressBarType barType;
    SceSaveDataDialogSystemMessageParam sysMsgParam;
    const Ptr<SceChar8> msg;
    SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogAppSubDirParam {
    SceChar8 srcDir[64];
    SceChar8 dstDir[64];
    SceChar8 reserved[32];
};

struct SceSaveDataDialogSlotConfigParam {
    const Ptr<SceAppUtilMountPoint> mountPoint;
    const Ptr<SceSaveDataDialogAppSubDirParam> appSubDir;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogParam {
    SceUInt32 sdkVersion;
    SceCommonDialogParam commonParam;
    SceSaveDataDialogMode mode;
    SceSaveDataDialogType dispType;
    Ptr<SceSaveDataDialogFixedParam> fixedParam;
    Ptr<SceSaveDataDialogListParam> listParam;
    Ptr<SceSaveDataDialogUserMessageParam> userMsgParam;
    Ptr<SceSaveDataDialogSystemMessageParam> sysMsgParam;
    Ptr<SceSaveDataDialogErrorCodeParam> errorCodeParam;
    Ptr<SceSaveDataDialogProgressBarParam> progressBarParam;
    Ptr<SceSaveDataDialogSlotConfigParam> slotConfParam;
    SceSaveDataDialogEnvFlag flag;
    Ptr<void> userdata;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogSlotInfo {
    SceUInt32 isExist;
    SceAppUtilSaveDataSlotParam *slotParam;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogResult {
    SceInt32 mode;
    SceInt32 result;
    SceInt32 buttonId;
    SceInt32 slotId;
    Ptr<SceSaveDataDialogSlotInfo> slotInfo;
    Ptr<void> userdata;
    SceChar8 reserved[32];
};
