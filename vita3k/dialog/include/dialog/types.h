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

#pragma once

#include <host/app_util.h>
#include <mem/ptr.h>
#include <psp2/ime_dialog.h>
#include <psp2/message_dialog.h>

namespace emu {

typedef enum SceSaveDataDialogMode {
    SCE_SAVEDATA_DIALOG_MODE_FIXED = 1,
    SCE_SAVEDATA_DIALOG_MODE_LIST = 2,
    SCE_SAVEDATA_DIALOG_MODE_USER_MSG = 3,
    SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG = 4,
    SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE = 5,
    SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR = 6
} SceSaveDataDialogMode;

typedef enum SceSaveDataDialogFocusPos {
    SCE_SAVEDATA_DIALOG_FOCUS_POS_LISTHEAD = 0,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_LISTTAIL = 1,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_EMPTYHEAD = 2,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_EMPTYTAIL = 3,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATAHEAD = 4,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATATAIL = 5,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATALATEST = 6,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_DATAOLDEST = 7,
    SCE_SAVEDATA_DIALOG_FOCUS_POS_ID = 8
} SceSaveDataDialogFocusPos;

typedef enum SceSaveDataDialogListItemStyle {
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE_SUBTITLE = 0,
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_SUBTITLE_DATE = 1,
    SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE = 2
} SceSaveDataDialogListItemStyle;

typedef enum SceSaveDataDialogType {
    SCE_SAVEDATA_DIALOG_TYPE_SAVE = 1,
    SCE_SAVEDATA_DIALOG_TYPE_LOAD = 2,
    SCE_SAVEDATA_DIALOG_TYPE_DELETE = 3
} SceSaveDataDialogType;

typedef enum SceSaveDataDialogButtonType {
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK = 0,
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_TYPE_NONE = 2
} SceSaveDataDialogButtonType;

typedef enum SceSaveDataDialogSystemMessageType {
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
} SceSaveDataDialogSystemMessageType;

typedef enum SceSaveDataDialogProgressBarType {
    SCE_SAVEDATA_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE = 0
} SceSaveDataDialogProgressBarType;

typedef enum SceSaveDataDialogEnvFlag {
    SCE_SAVEDATA_DIALOG_ENV_FLAG_DEFAULT = 0
} SceSaveDataDialogEnvFlag;

typedef enum SceSaveDataDialogButtonId {
    SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID = 0,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_OK = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_YES = 1,
    SCE_SAVEDATA_DIALOG_BUTTON_ID_NO = 2
} SceSaveDataDialogButtonId;

typedef enum SceSaveDataDialogProgressBarTarget {
    SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT = 0
} SceSaveDataDialogProgressBarTarget;

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

    emu::SceCommonDialogParam commonParam;

    SceUChar8 enterLabel;
    SceChar8 reserved[35];
};

struct SceNpTrophySetupDialogParam {
    SceUInt32 sdkVersion;

    emu::SceCommonDialogParam commonParam;
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

struct SceNpTrophySetupDialogResult {
    SceInt32 result;
    SceUInt8 reserved[128];
};

struct SceMsgDialogUserMessageParam {
    SceInt32 buttonType;
    const Ptr<SceChar8> msg;
    Ptr<emu::SceMsgDialogButtonsParam> buttonParam;
    SceChar8 reserved[28];
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
    Ptr<emu::SceMsgDialogUserMessageParam> userMsgParam;
    Ptr<SceMsgDialogSystemMessageParam> sysMsgParam;
    Ptr<SceMsgDialogErrorCodeParam> errorCodeParam;
    Ptr<emu::SceMsgDialogProgressBarParam> progBarParam;
    SceInt32 flag;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogFixedParam {
    emu::SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogListParam {
    const Ptr<emu::SceAppUtilSaveDataSlot> slotList;
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
    emu::SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogSystemMessageParam {
    SceSaveDataDialogSystemMessageType sysMsgType;
    SceInt32 value;
    emu::SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogErrorCodeParam {
    SceInt32 errorCode;
    emu::SceAppUtilSaveDataSlot targetSlot;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogProgressBarParam {
    SceSaveDataDialogProgressBarType barType;
    SceSaveDataDialogSystemMessageParam sysMsgParam;
    const Ptr<SceChar8> msg;
    emu::SceAppUtilSaveDataSlot targetSlot;
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
    Ptr<emu::SceSaveDataDialogFixedParam> fixedParam;
    Ptr<emu::SceSaveDataDialogListParam> listParam;
    Ptr<emu::SceSaveDataDialogUserMessageParam> userMsgParam;
    Ptr<emu::SceSaveDataDialogSystemMessageParam> sysMsgParam;
    Ptr<emu::SceSaveDataDialogErrorCodeParam> errorCodeParam;
    Ptr<emu::SceSaveDataDialogProgressBarParam> progressBarParam;
    Ptr<emu::SceSaveDataDialogSlotConfigParam> slotConfParam;
    SceSaveDataDialogEnvFlag flag;
    Ptr<void> userdata;
    SceChar8 reserved[32];
};

struct SceSaveDataDialogSlotInfo {
    SceUInt32 isExist;
    emu::SceAppUtilSaveDataSlotParam *slotParam;
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
} // namespace emu
