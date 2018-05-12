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

enum DialogType {
    NO_DIALOG,
    IME_DIALOG,
    MESSAGE_DIALOG,
    TROPHY_SETUP_DIALOG
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
};

struct TrophyState {
    uint32_t tick;
};

struct DialogState {
    DialogType type = NO_DIALOG;
    SceCommonDialogStatus status = SCE_COMMON_DIALOG_STATUS_NONE;
    ImeState ime;
    MsgState msg;
    TrophyState trophy;
};
