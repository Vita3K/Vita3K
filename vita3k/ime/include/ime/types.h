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

#define SCE_IME_MAX_PREEDIT_LENGTH 30
#define SCE_IME_MAX_TEXT_LENGTH 2048

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
