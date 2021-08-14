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

#include <module/module.h>

BRIDGE_DECL(sceFontClose)
BRIDGE_DECL(sceFontDoneLib)
BRIDGE_DECL(sceFontFindFont)
BRIDGE_DECL(sceFontFindOptimumFont)
BRIDGE_DECL(sceFontFlush)
BRIDGE_DECL(sceFontGetCharGlyphImage)
BRIDGE_DECL(sceFontGetCharGlyphImage_Clip)
BRIDGE_DECL(sceFontGetCharImageRect)
BRIDGE_DECL(sceFontGetCharInfo)
BRIDGE_DECL(sceFontGetFontInfo)
BRIDGE_DECL(sceFontGetFontInfoByIndexNumber)
BRIDGE_DECL(sceFontGetFontList)
BRIDGE_DECL(sceFontGetNumFontList)
BRIDGE_DECL(sceFontNewLib)
BRIDGE_DECL(sceFontOpen)
BRIDGE_DECL(sceFontOpenUserFile)
BRIDGE_DECL(sceFontOpenUserMemory)
BRIDGE_DECL(sceFontPixelToPointH)
BRIDGE_DECL(sceFontPixelToPointV)
BRIDGE_DECL(sceFontPointToPixelH)
BRIDGE_DECL(sceFontPointToPixelV)
BRIDGE_DECL(sceFontSetAltCharacterCode)
BRIDGE_DECL(sceFontSetResolution)
