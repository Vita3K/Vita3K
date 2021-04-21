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

#include <mem/ptr.h>

#define SCE_IME_MAX_PREEDIT_LENGTH 30
#define SCE_IME_MAX_TEXT_LENGTH 2048

enum SceImeLanguage {
    SCE_IME_LANGUAGE_DANISH = 0x00000001ULL,
    SCE_IME_LANGUAGE_GERMAN = 0x00000002ULL,
    SCE_IME_LANGUAGE_ENGLISH_US = 0x00000004ULL,
    SCE_IME_LANGUAGE_SPANISH = 0x00000008ULL,
    SCE_IME_LANGUAGE_FRENCH = 0x00000010ULL,
    SCE_IME_LANGUAGE_ITALIAN = 0x00000020ULL,
    SCE_IME_LANGUAGE_DUTCH = 0x00000040ULL,
    SCE_IME_LANGUAGE_NORWEGIAN = 0x00000080ULL,
    SCE_IME_LANGUAGE_POLISH = 0x00000100ULL,
    SCE_IME_LANGUAGE_PORTUGUESE_PT = 0x00000200ULL,
    SCE_IME_LANGUAGE_RUSSIAN = 0x00000400ULL,
    SCE_IME_LANGUAGE_FINNISH = 0x00000800ULL,
    SCE_IME_LANGUAGE_SWEDISH = 0x00001000ULL,
    SCE_IME_LANGUAGE_JAPANESE = 0x00002000ULL,
    SCE_IME_LANGUAGE_KOREAN = 0x00004000ULL,
    SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE = 0x00008000ULL,
    SCE_IME_LANGUAGE_TRADITIONAL_CHINESE = 0x00010000ULL,
    SCE_IME_LANGUAGE_PORTUGUESE_BR = 0x00020000ULL,
    SCE_IME_LANGUAGE_ENGLISH_GB = 0x00040000ULL,
    SCE_IME_LANGUAGE_TURKISH = 0x00080000ULL
};

enum SceImeType {
    SCE_IME_TYPE_DEFAULT = 0,
    SCE_IME_TYPE_BASIC_LATIN = 1,
    SCE_IME_TYPE_NUMBER = 2,
    SCE_IME_TYPE_EXTENDED_NUMBER = 3,
    SCE_IME_TYPE_URL = 4,
    SCE_IME_TYPE_MAIL = 5
};

enum ImeEvent {
    SCE_IME_EVENT_OPEN = 0,
    SCE_IME_EVENT_UPDATE_TEXT = 1,
    SCE_IME_EVENT_UPDATE_CARET = 2,
    SCE_IME_EVENT_CHANGE_SIZE = 3,
    SCE_IME_EVENT_PRESS_CLOSE = 4,
    SCE_IME_EVENT_PRESS_ENTER = 5,
};

enum SceImeEnterLabel {
    SCE_IME_ENTER_LABEL_DEFAULT = 0,
    SCE_IME_ENTER_LABEL_SEND = 1,
    SCE_IME_ENTER_LABEL_SEARCH = 2,
    SCE_IME_ENTER_LABEL_GO = 3
};

struct SceImeParam {
    SceUInt32 sdkVersion;
    SceUInt32 inputMethod;
    SceUInt64 supportedLanguages;
    SceBool languagesForced;
    SceUInt32 type;
    SceUInt32 option;
    Ptr<void> work;
    Ptr<void> arg;
    Ptr<void> handler;
    Ptr<void> filter;
    Ptr<SceWChar16> initialText;
    SceUInt32 maxTextLength;
    Ptr<SceWChar16> inputTextBuffer;
    SceUChar8 enterLabel;
    SceUChar8 reserved[7];
};

struct SceImeRect {
    SceUInt32 x;
    SceUInt32 y;
    SceUInt32 width;
    SceUInt32 height;
};

struct SceImeEditText {
    SceUInt32 preeditIndex;
    SceUInt32 preeditLength;
    SceUInt32 caretIndex;
    Ptr<SceWChar16> str;
    SceUInt32 editIndex;
    SceInt32 editLengthChange;
};

union SceImeEventParam {
    SceImeRect rect;
    SceImeEditText text;
    SceUInt32 caretIndex;
    SceUChar8 reserved[40];
};

struct SceImeEvent {
    SceUInt32 id;
    SceImeEventParam param;
};

struct SceImeCaret {
    SceUInt32 x;
    SceUInt32 y;
    SceUInt32 height;
    SceUInt32 index;
};

struct SceImePreeditGeometry {
    SceUInt32 x;
    SceUInt32 y;
    SceUInt32 height;
};

typedef void (*SceImeEventHandler)(void *arg, const SceImeEvent *e);

typedef SceInt32 (*SceImeTextFilter)(
    SceWChar16 *outText,
    SceUInt32 *outTextLength,
    const SceWChar16 *srcText,
    SceUInt32 srcTextLength);
