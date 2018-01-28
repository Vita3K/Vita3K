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

#include <module/module.h>

// ScePvf
BRIDGE_DECL(__scePvfSetFt2DoneLibCHook)
BRIDGE_DECL(__scePvfSetFt2LibCHook)
BRIDGE_DECL(scePvfClose)
BRIDGE_DECL(scePvfDoneLib)
BRIDGE_DECL(scePvfFindFont)
BRIDGE_DECL(scePvfFindOptimumFont)
BRIDGE_DECL(scePvfFlush)
BRIDGE_DECL(scePvfGetCharGlyphImage)
BRIDGE_DECL(scePvfGetCharGlyphImage_Clip)
BRIDGE_DECL(scePvfGetCharGlyphOutline)
BRIDGE_DECL(scePvfGetCharImageRect)
BRIDGE_DECL(scePvfGetCharInfo)
BRIDGE_DECL(scePvfGetFontInfo)
BRIDGE_DECL(scePvfGetFontInfoByIndexNumber)
BRIDGE_DECL(scePvfGetFontList)
BRIDGE_DECL(scePvfGetKerningInfo)
BRIDGE_DECL(scePvfGetNumFontList)
BRIDGE_DECL(scePvfGetVertCharGlyphImage)
BRIDGE_DECL(scePvfGetVertCharGlyphImage_Clip)
BRIDGE_DECL(scePvfGetVertCharGlyphOutline)
BRIDGE_DECL(scePvfGetVertCharImageRect)
BRIDGE_DECL(scePvfGetVertCharInfo)
BRIDGE_DECL(scePvfIsElement)
BRIDGE_DECL(scePvfIsVertElement)
BRIDGE_DECL(scePvfNewLib)
BRIDGE_DECL(scePvfOpen)
BRIDGE_DECL(scePvfOpenDefaultJapaneseFontOnSharedMemory)
BRIDGE_DECL(scePvfOpenDefaultLatinFontOnSharedMemory)
BRIDGE_DECL(scePvfOpenUserFile)
BRIDGE_DECL(scePvfOpenUserFileWithSubfontIndex)
BRIDGE_DECL(scePvfOpenUserMemory)
BRIDGE_DECL(scePvfOpenUserMemoryWithSubfontIndex)
BRIDGE_DECL(scePvfPixelToPointH)
BRIDGE_DECL(scePvfPixelToPointV)
BRIDGE_DECL(scePvfPointToPixelH)
BRIDGE_DECL(scePvfPointToPixelV)
BRIDGE_DECL(scePvfReleaseCharGlyphOutline)
BRIDGE_DECL(scePvfSetAltCharacterCode)
BRIDGE_DECL(scePvfSetCharSize)
BRIDGE_DECL(scePvfSetEM)
BRIDGE_DECL(scePvfSetEmboldenRate)
BRIDGE_DECL(scePvfSetResolution)
BRIDGE_DECL(scePvfSetSkewValue)
