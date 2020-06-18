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

#include <mem/mem.h>
#include <pvf/state.h>

EXPORT(int, __scePvfSetFt2DoneLibCHook) {
    return UNIMPLEMENTED();
}

EXPORT(int, __scePvfSetFt2LibCHook) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfClose, ScePvfFontId font_id) {
    clean_pvf_font(host.pvf, font_id);
    return SCE_PVF_OK;
}

EXPORT(ScePvfError, scePvfDoneLib, ScePvfLibID lib_id) {
    clean_pvf_library(host.pvf, lib_id);
    return SCE_PVF_OK;
}

EXPORT(ScePvfFontIndex, scePvfFindFont, ScePvfLibID libID, ScePvfFontStyleInfo *fontStyleInfo, ScePvfError *errorCode) {
    STUBBED("deal with styleInfo correctly");
    ScePvfFontIndex out;
    switch (fontStyleInfo->countryCode) {
    case SCE_PVF_LANGUAGE_LATIN:
        out = 0;
        break;
    case SCE_PVF_LANGUAGE_K:
        out = 1;
        break;
    case SCE_PVF_LANGUAGE_C:
        out = 2;
        break;
    default:
        out = 3;
    }
    *errorCode = SCE_PVF_OK;
    return out;
}

static std::string get_path_for_index(ScePvfFontIndex index) {
    std::ostringstream path;
    path << "sa0:data/font/pvf";
    switch (index) {
    case 0:
        path << "/latin0.pvf";
        break;
    case 1:
        path << "/kr0.pvf";
        break;
    case 2:
        path << "/cn0.pvf";
        break;
    default:
        path << "/jpn0.pvf";
    }
    return path.str();
}

EXPORT(ScePvfFontIndex, scePvfFindOptimumFont, ScePvfLibID libID, ScePvfFontStyleInfo *fontStyleInfo, ScePvfError *errorCode) {
    STUBBED("deal with bold and regular");
    ScePvfFontIndex out;
    switch (fontStyleInfo->countryCode) {
    case SCE_PVF_LANGUAGE_LATIN:
        out = 0;
        break;
    case SCE_PVF_LANGUAGE_K:
        out = 1;
        break;
    case SCE_PVF_LANGUAGE_C:
        out = 2;
        break;
    default:
        out = 3;
    }
    *errorCode = SCE_PVF_OK;
    return out;
}

EXPORT(int, scePvfFlush) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfGetCharGlyphImage, ScePvfFontId fontID, uint16_t charCode, ScePvfUserImageBufferRec *imageBuffer) {
    auto font = get_pvf_font(host.pvf, fontID);
    auto lib = get_pvf_library(host.pvf, font->lib_id);
    if (font->render(*lib, host.mem, charCode, imageBuffer)) {
        return SCE_PVF_ERROR_NOGLYPH;
    }

    return SCE_PVF_OK;
}

// TODO clip it
EXPORT(ScePvfError, scePvfGetCharGlyphImage_Clip, ScePvfFontId fontID, uint16_t charCode, ScePvfUserImageBufferRec *imageBuffer, int32_t clipX, int32_t clipY, uint32_t clipWidth, uint32_t clipHeight) {
    auto font = get_pvf_font(host.pvf, fontID);
    auto lib = get_pvf_library(host.pvf, font->lib_id);
    if (font->render(*lib, host.mem, charCode, imageBuffer)) {
        return SCE_PVF_ERROR_NOGLYPH;
    }

    return SCE_PVF_OK;
}

EXPORT(int, scePvfGetCharGlyphOutline) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfGetCharImageRect, ScePvfFontId fontID, ScePvfCharCode charCode, ScePvfIrect *rect) {
    auto font = get_pvf_font(host.pvf, fontID);
    auto lib = get_pvf_library(host.pvf, font->lib_id);
    ScePvfCharInfo info;
    if (font->get_char_info(*lib, charCode, &info)) {
        return SCE_PVF_ERROR_NOGLYPH;
    }
    rect->width = info.bitmapWidth;
    rect->height = info.bitmapHeight;
    return SCE_PVF_OK;
}

EXPORT(ScePvfError, scePvfGetCharInfo, ScePvfFontId fontID, ScePvfCharCode charCode, ScePvfCharInfo *charInfo) {
    auto font = get_pvf_font(host.pvf, fontID);
    auto lib = get_pvf_library(host.pvf, font->lib_id);
    if (font->get_char_info(*lib, charCode, charInfo)) {
        return SCE_PVF_ERROR_NOGLYPH;
    }

    return SCE_PVF_OK;
}

EXPORT(ScePvfError, scePvfGetFontInfo, ScePvfFontId fontID, ScePvfFontInfo *fontInfo) {
    auto font = get_pvf_font(host.pvf, fontID);
    if (font->get_info(fontInfo)) {
        return SCE_PVF_ERROR_NOFILE;
    }

    return SCE_PVF_OK;
}

EXPORT(int, scePvfGetFontInfoByIndexNumber, ScePvfLibID libID, ScePvfFontStyleInfo *fontStyleInfo, ScePvfFontIndex fontIndex) {
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

EXPORT(ScePvfLibID, scePvfNewLib, ScePvfInitRec *initParam, ScePvfError *errorCode) {
    *errorCode = SCE_PVF_OK;
    return create_pvf_library(host.pvf);
}

EXPORT(ScePvfError, scePvfOpen, ScePvfLibID libID, ScePvfFontIndex index, uint32_t mode, ScePvfError *errorCode) {
    auto path = get_path_for_index(index);
    auto lib = get_pvf_library(host.pvf, libID);
    auto out = lib->load_font_file(host.pvf, host.mem, host.io, path, host.pref_path);
    if (out == 0) {
        *errorCode = SCE_PVF_ERROR_NOFILE;
    } else {
        *errorCode = SCE_PVF_OK;
    }
    return out;
}

EXPORT(int, scePvfOpenDefaultJapaneseFontOnSharedMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, scePvfOpenDefaultLatinFontOnSharedMemory) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfOpenUserFile, ScePvfLibID libID, const char *filename, uint32_t mode, uint32_t *errorCode) {
    auto lib = get_pvf_library(host.pvf, libID);
    auto out = lib->load_font_file(host.pvf, host.mem, host.io, filename, host.pref_path);
    if (out == 0) {
        *errorCode = SCE_PVF_ERROR_NOFILE;
    } else {
        *errorCode = SCE_PVF_OK;
    }
    return out;
}

EXPORT(int, scePvfOpenUserFileWithSubfontIndex) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfOpenUserMemory, ScePvfLibID libID, Ptr<uint8_t> addr, uint32_t size, ScePvfError *errorCode) {
    auto lib = get_pvf_library(host.pvf, libID);
    auto out = lib->load_font_file(host.pvf, host.mem, addr, size);
    if (out == 0) {
        *errorCode = SCE_PVF_ERROR_NOFILE;
    } else {
        *errorCode = SCE_PVF_OK;
    }
    return out;
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

EXPORT(ScePvfError, scePvfSetCharSize, ScePvfFontId fontID, SceFloat32 hSize, SceFloat32 vSize) {
    auto font = get_pvf_font(host.pvf, fontID);
    auto lib = get_pvf_library(host.pvf, font->lib_id);
    font->set_char_size(*lib, hSize, vSize);
    return SCE_PVF_OK;
}

EXPORT(ScePvfError, scePvfSetEM, ScePvfLibID libID, SceFloat32 em) {
    auto lib = get_pvf_library(host.pvf, libID);
    lib->set_em(em);
    lib->update_fonts(host.pvf);
    return SCE_PVF_OK;
}

EXPORT(int, scePvfSetEmboldenRate) {
    return UNIMPLEMENTED();
}

EXPORT(ScePvfError, scePvfSetResolution, ScePvfLibID libID, SceFloat32 hSize, SceFloat32 vSize) {
    auto lib = get_pvf_library(host.pvf, libID);
    lib->set_resolution(hSize, vSize);
    lib->update_fonts(host.pvf);
    return SCE_PVF_OK;
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
