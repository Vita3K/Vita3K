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

#include <SceLibPvf/exports.h>

EXPORT(int, __scePvfSetFt2DoneLibCHook) {
    return unimplemented("__scePvfSetFt2DoneLibCHook");
}

EXPORT(int, __scePvfSetFt2LibCHook) {
    return unimplemented("__scePvfSetFt2LibCHook");
}

EXPORT(int, scePvfClose) {
    return unimplemented("scePvfClose");
}

EXPORT(int, scePvfDoneLib) {
    return unimplemented("scePvfDoneLib");
}

EXPORT(int, scePvfFindFont) {
    return unimplemented("scePvfFindFont");
}

EXPORT(int, scePvfFindOptimumFont) {
    return unimplemented("scePvfFindOptimumFont");
}

EXPORT(int, scePvfFlush) {
    return unimplemented("scePvfFlush");
}

EXPORT(int, scePvfGetCharGlyphImage) {
    return unimplemented("scePvfGetCharGlyphImage");
}

EXPORT(int, scePvfGetCharGlyphImage_Clip) {
    return unimplemented("scePvfGetCharGlyphImage_Clip");
}

EXPORT(int, scePvfGetCharGlyphOutline) {
    return unimplemented("scePvfGetCharGlyphOutline");
}

EXPORT(int, scePvfGetCharImageRect) {
    return unimplemented("scePvfGetCharImageRect");
}

EXPORT(int, scePvfGetCharInfo) {
    return unimplemented("scePvfGetCharInfo");
}

EXPORT(int, scePvfGetFontInfo) {
    return unimplemented("scePvfGetFontInfo");
}

EXPORT(int, scePvfGetFontInfoByIndexNumber) {
    return unimplemented("scePvfGetFontInfoByIndexNumber");
}

EXPORT(int, scePvfGetFontList) {
    return unimplemented("scePvfGetFontList");
}

EXPORT(int, scePvfGetKerningInfo) {
    return unimplemented("scePvfGetKerningInfo");
}

EXPORT(int, scePvfGetNumFontList) {
    return unimplemented("scePvfGetNumFontList");
}

EXPORT(int, scePvfGetVertCharGlyphImage) {
    return unimplemented("scePvfGetVertCharGlyphImage");
}

EXPORT(int, scePvfGetVertCharGlyphImage_Clip) {
    return unimplemented("scePvfGetVertCharGlyphImage_Clip");
}

EXPORT(int, scePvfGetVertCharGlyphOutline) {
    return unimplemented("scePvfGetVertCharGlyphOutline");
}

EXPORT(int, scePvfGetVertCharImageRect) {
    return unimplemented("scePvfGetVertCharImageRect");
}

EXPORT(int, scePvfGetVertCharInfo) {
    return unimplemented("scePvfGetVertCharInfo");
}

EXPORT(int, scePvfIsElement) {
    return unimplemented("scePvfIsElement");
}

EXPORT(int, scePvfIsVertElement) {
    return unimplemented("scePvfIsVertElement");
}

EXPORT(int, scePvfNewLib) {
    return unimplemented("scePvfNewLib");
}

EXPORT(int, scePvfOpen) {
    return unimplemented("scePvfOpen");
}

EXPORT(int, scePvfOpenDefaultJapaneseFontOnSharedMemory) {
    return unimplemented("scePvfOpenDefaultJapaneseFontOnSharedMemory");
}

EXPORT(int, scePvfOpenDefaultLatinFontOnSharedMemory) {
    return unimplemented("scePvfOpenDefaultLatinFontOnSharedMemory");
}

EXPORT(int, scePvfOpenUserFile) {
    return unimplemented("scePvfOpenUserFile");
}

EXPORT(int, scePvfOpenUserFileWithSubfontIndex) {
    return unimplemented("scePvfOpenUserFileWithSubfontIndex");
}

EXPORT(int, scePvfOpenUserMemory) {
    return unimplemented("scePvfOpenUserMemory");
}

EXPORT(int, scePvfOpenUserMemoryWithSubfontIndex) {
    return unimplemented("scePvfOpenUserMemoryWithSubfontIndex");
}

EXPORT(int, scePvfPixelToPointH) {
    return unimplemented("scePvfPixelToPointH");
}

EXPORT(int, scePvfPixelToPointV) {
    return unimplemented("scePvfPixelToPointV");
}

EXPORT(int, scePvfPointToPixelH) {
    return unimplemented("scePvfPointToPixelH");
}

EXPORT(int, scePvfPointToPixelV) {
    return unimplemented("scePvfPointToPixelV");
}

EXPORT(int, scePvfReleaseCharGlyphOutline) {
    return unimplemented("scePvfReleaseCharGlyphOutline");
}

EXPORT(int, scePvfSetAltCharacterCode) {
    return unimplemented("scePvfSetAltCharacterCode");
}

EXPORT(int, scePvfSetCharSize) {
    return unimplemented("scePvfSetCharSize");
}

EXPORT(int, scePvfSetEM) {
    return unimplemented("scePvfSetEM");
}

EXPORT(int, scePvfSetEmboldenRate) {
    return unimplemented("scePvfSetEmboldenRate");
}

EXPORT(int, scePvfSetResolution) {
    return unimplemented("scePvfSetResolution");
}

EXPORT(int, scePvfSetSkewValue) {
    return unimplemented("scePvfSetSkewValue");
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
