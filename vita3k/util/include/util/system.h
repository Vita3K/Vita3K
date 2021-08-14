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

enum SceSystemParamLang {
    SCE_SYSTEM_PARAM_LANG_JAPANESE,
    SCE_SYSTEM_PARAM_LANG_ENGLISH_US,
    SCE_SYSTEM_PARAM_LANG_FRENCH,
    SCE_SYSTEM_PARAM_LANG_SPANISH,
    SCE_SYSTEM_PARAM_LANG_GERMAN,
    SCE_SYSTEM_PARAM_LANG_ITALIAN,
    SCE_SYSTEM_PARAM_LANG_DUTCH,
    SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT,
    SCE_SYSTEM_PARAM_LANG_RUSSIAN,
    SCE_SYSTEM_PARAM_LANG_KOREAN,
    SCE_SYSTEM_PARAM_LANG_CHINESE_T,
    SCE_SYSTEM_PARAM_LANG_CHINESE_S,
    SCE_SYSTEM_PARAM_LANG_FINNISH,
    SCE_SYSTEM_PARAM_LANG_SWEDISH,
    SCE_SYSTEM_PARAM_LANG_DANISH,
    SCE_SYSTEM_PARAM_LANG_NORWEGIAN,
    SCE_SYSTEM_PARAM_LANG_POLISH,
    SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR,
    SCE_SYSTEM_PARAM_LANG_ENGLISH_GB,
    SCE_SYSTEM_PARAM_LANG_TURKISH,
    SCE_SYSTEM_PARAM_LANG_MAX_VALUE = 0xFFFFFFFF
};

enum SceSystemParamEnterButtonAssign {
    SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE,
    SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS,
    SCE_SYSTEM_PARAM_ENTER_BUTTON_MAX_VALUE = 0xFFFFFFFF
};
