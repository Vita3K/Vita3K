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

#include <dialog/types.h>
#include <psp2/ime_dialog.h>

#include <io/vfs.h>

enum DialogType {
    NO_DIALOG,
    IME_DIALOG,
    MESSAGE_DIALOG,
    TROPHY_SETUP_DIALOG,
    SAVEDATA_DIALOG
};

struct ImeState {
    std::string title;
    uint32_t max_length;
    bool multiline;
    bool cancelable;
    uint16_t *result;
    char text[256];
    uint32_t status;
};

struct MsgState {
    int32_t mode;
    std::string message;
    uint8_t btn_num;
    std::string btn[3];
    uint32_t btn_val[3];
    uint32_t status;
    uint32_t bar_rate = 0;
    bool has_progress_bar = false;
};

struct TrophyState {
    uint32_t tick;
};

struct SavedataState {
    uint8_t btn_num = 0;
    std::string btn[2];
    uint32_t btn_val[2];
    uint32_t button_id = emu::SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;

    std::vector<vfs::FileBuffer> icon_buffer;
    std::vector<bool> icon_loaded;

    uint32_t mode;
    uint32_t mode_to_display;

    uint32_t display_type;
    std::vector<uint32_t> slot_id;
    std::vector<emu::SceSaveDataDialogSlotInfo> slot_info;
    Ptr<void> userdata;

    std::string msg;
    std::vector<std::string> title;
    std::vector<std::string> subtitle;
    std::vector<std::string> details;
    std::vector<SceDateTime> date;
    std::vector<bool> has_date = { false };

    uint32_t bar_rate = 0;
    bool has_progress_bar = false;

    emu::SceAppUtilSaveDataSlotEmptyParam *list_empty_param = nullptr;
    uint32_t slot_list_size = 0;
    uint32_t list_style;
    std::string list_title;
    uint32_t selected_save = 0;
};

struct DialogState {
    DialogType type = NO_DIALOG;
    SceCommonDialogStatus status = SCE_COMMON_DIALOG_STATUS_NONE;
    SceCommonDialogResult result = SCE_COMMON_DIALOG_RESULT_OK;
    ImeState ime;
    MsgState msg;
    TrophyState trophy;
    SavedataState savedata;
};
