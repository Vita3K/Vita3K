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

#include <mem/ptr.h>
#include <psp2/ime_dialog.h>

namespace emu {

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
}
