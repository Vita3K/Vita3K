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

#include "ScePvf.h"

EXPORT(int, __scePvfSetFt2DoneLibCHook) {
    return UNIMPLEMENTED();
}

EXPORT(int, __scePvfSetFt2LibCHook) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfDoneLib) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfFindFont) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfFindOptimumFont) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetCharGlyphImage) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetCharGlyphImage_Clip) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetCharGlyphOutline) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetCharImageRect) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetCharInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetFontInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetFontInfoByIndexNumber) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetFontList) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetKerningInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetNumFontList) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetVertCharGlyphImage) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetVertCharGlyphImage_Clip) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetVertCharGlyphOutline) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetVertCharImageRect) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfGetVertCharInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfIsElement) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfIsVertElement) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfNewLib) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenDefaultJapaneseFontOnSharedMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenDefaultLatinFontOnSharedMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenUserFile) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenUserFileWithSubfontIndex) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenUserMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenUserMemoryWithSubfontIndex) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfPixelToPointH) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfPixelToPointV) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfPointToPixelH) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfPointToPixelV) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfReleaseCharGlyphOutline) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetAltCharacterCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetCharSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetEM) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetEmboldenRate) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetResolution) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfSetSkewValue) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(__scePvfSetFt2DoneLibCHook)
BRIDGE_IMPL(__scePvfSetFt2LibCHook)
BRIDGE_IMPL(scePvfClose)
BRIDGE_IMPL(scePvfDoneLib)
BRIDGE_IMPL(scePvfFindFont)
BRIDGE_IMPL(scePvfFindOptimumFont)
BRIDGE_IMPL(scePvfFlush)
BRIDGE_IMPL(scePvfGetCharGlyphImage)
BRIDGE_IMPL(scePvfGetCharGlyphImage_Clip)
BRIDGE_IMPL(scePvfGetCharGlyphOutline)
BRIDGE_IMPL(scePvfGetCharImageRect)
BRIDGE_IMPL(scePvfGetCharInfo)
BRIDGE_IMPL(scePvfGetFontInfo)
BRIDGE_IMPL(scePvfGetFontInfoByIndexNumber)
BRIDGE_IMPL(scePvfGetFontList)
BRIDGE_IMPL(scePvfGetKerningInfo)
BRIDGE_IMPL(scePvfGetNumFontList)
BRIDGE_IMPL(scePvfGetVertCharGlyphImage)
BRIDGE_IMPL(scePvfGetVertCharGlyphImage_Clip)
BRIDGE_IMPL(scePvfGetVertCharGlyphOutline)
BRIDGE_IMPL(scePvfGetVertCharImageRect)
BRIDGE_IMPL(scePvfGetVertCharInfo)
BRIDGE_IMPL(scePvfIsElement)
BRIDGE_IMPL(scePvfIsVertElement)
BRIDGE_IMPL(scePvfNewLib)
BRIDGE_IMPL(scePvfOpen)
BRIDGE_IMPL(scePvfOpenDefaultJapaneseFontOnSharedMemory)
BRIDGE_IMPL(scePvfOpenDefaultLatinFontOnSharedMemory)
BRIDGE_IMPL(scePvfOpenUserFile)
BRIDGE_IMPL(scePvfOpenUserFileWithSubfontIndex)
BRIDGE_IMPL(scePvfOpenUserMemory)
BRIDGE_IMPL(scePvfOpenUserMemoryWithSubfontIndex)
BRIDGE_IMPL(scePvfPixelToPointH)
BRIDGE_IMPL(scePvfPixelToPointV)
BRIDGE_IMPL(scePvfPointToPixelH)
BRIDGE_IMPL(scePvfPointToPixelV)
BRIDGE_IMPL(scePvfReleaseCharGlyphOutline)
BRIDGE_IMPL(scePvfSetAltCharacterCode)
BRIDGE_IMPL(scePvfSetCharSize)
BRIDGE_IMPL(scePvfSetEM)
BRIDGE_IMPL(scePvfSetEmboldenRate)
BRIDGE_IMPL(scePvfSetResolution)
BRIDGE_IMPL(scePvfSetSkewValue)
