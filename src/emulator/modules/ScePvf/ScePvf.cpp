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
    return unimplemented(export_name);
}

EXPORT(int, __scePvfSetFt2LibCHook) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfClose) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfDoneLib) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfFindFont) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfFindOptimumFont) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfFlush) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetCharGlyphImage) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetCharGlyphImage_Clip) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetCharGlyphOutline) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetCharImageRect) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetCharInfo) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetFontInfo) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetFontInfoByIndexNumber) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetFontList) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetKerningInfo) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetNumFontList) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetVertCharGlyphImage) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetVertCharGlyphImage_Clip) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetVertCharGlyphOutline) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetVertCharImageRect) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfGetVertCharInfo) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfIsElement) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfIsVertElement) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfNewLib) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpen) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenDefaultJapaneseFontOnSharedMemory) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenDefaultLatinFontOnSharedMemory) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenUserFile) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenUserFileWithSubfontIndex) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenUserMemory) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfOpenUserMemoryWithSubfontIndex) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfPixelToPointH) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfPixelToPointV) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfPointToPixelH) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfPointToPixelV) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfReleaseCharGlyphOutline) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetAltCharacterCode) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetCharSize) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetEM) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetEmboldenRate) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetResolution) {
    return unimplemented(export_name);
}

EXPORT(int, scePvfSetSkewValue) {
    return unimplemented(export_name);
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
