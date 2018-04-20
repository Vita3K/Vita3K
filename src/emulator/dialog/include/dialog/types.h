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

#include <psp2/ime_dialog.h>
#include <mem/ptr.h>

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
        SceUInt64 supportedLanguages;         //!< Dialog languages (One or more ::SceImeLanguage)
        SceBool languagesForced;
        SceUInt32 type;                       //!< Dialog type (One of ::SceImeType)
        SceUInt32 option;                     //!< Dialog options (One or more ::SceImeOption)
        SceUInt32 filter;

        SceUInt32 dialogMode;                 //!< Dialog mode (One of ::SceImeDialogDialogMode)
        SceUInt32 textBoxMode;                //!< Textbox mode (One of ::SceImeDialogTextboxMode)
        const Ptr<SceWChar16> title;
        SceUInt32 maxTextLength;
        Ptr<SceWChar16> initialText;
        Ptr<SceWChar16> inputTextBuffer;

        emu::SceCommonDialogParam commonParam;

        SceUChar8 enterLabel;
        SceChar8 reserved[35];
    };
}
