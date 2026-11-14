// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <module/module.h>

typedef void *SceFontLibHandle;
typedef void *SceFontHandle;

enum SceFontErrorCode : uint32_t {
    SCE_FONT_ERROR_OUT_OF_MEMORY = 0x80460001,
    SCE_FONT_ERROR_INVALID_LIBID = 0x80460002,
    SCE_FONT_ERROR_INVALID_PARAMETER = 0x80460003,
    SCE_FONT_ERROR_HANDLER_OPEN_FAILED = 0x80460005,
    SCE_FONT_ERROR_TOO_MANY_OPEN_FONTS = 0x80460009,
    SCE_FONT_ERROR_INVALID_FONT_DATA = 0x8046000A,
};

enum SceFontFamilyCode {
    SCE_FONT_FAMILY_DEFAULT = 0,
    SCE_FONT_FAMILY_SANS_SERIF = 1,
    SCE_FONT_FAMILY_SERIF = 2,
    SCE_FONT_FAMILY_ROUNDED = 3,
};

enum SceFontStyleCode {
    SCE_FONT_STYLE_DEFAULT = 0,
    SCE_FONT_STYLE_REGULAR = 1,
    SCE_FONT_STYLE_ITALIC = 2,
    SCE_FONT_STYLE_NARROW = 3,
    SCE_FONT_STYLE_NARROW_ITALIC = 4,
    SCE_FONT_STYLE_BOLD = 5,
    SCE_FONT_STYLE_BOLD_ITALIC = 6,
    SCE_FONT_STYLE_BLACK = 7,
    SCE_FONT_STYLE_BLACK_ITALIC = 8,
    SCE_FONT_STYLE_L = 101,
    SCE_FONT_STYLE_M = 102,
    SCE_FONT_STYLE_DB = 103,
    SCE_FONT_STYLE_B = 104,
    SCE_FONT_STYLE_EB = 105,
    SCE_FONT_STYLE_UB = 106,
};

enum SceFontLanguageCode {
    SCE_FONT_LANGUAGE_DEFAULT = 0,
    SCE_FONT_LANGUAGE_JAPANESE = 1,
    SCE_FONT_LANGUAGE_LATIN = 2,
    SCE_FONT_LANGUAGE_KOREAN = 3,
    SCE_FONT_LANGUAGE_CHINESE = 4,
    SCE_FONT_LANGUAGE_CJK = 5,
};

enum SceFontPixelFormatCode {
    SCE_FONT_PIXELFORMAT_4 = 0,
    SCE_FONT_PIXELFORMAT_4_REV = 1,
    SCE_FONT_PIXELFORMAT_8 = 2,
    SCE_FONT_PIXELFORMAT_24 = 3,
    SCE_FONT_PIXELFORMAT_32 = 4,
};

struct SceFontNewLibParams {
    Ptr<void> userData;
    unsigned int numFonts;
    Ptr<void> cacheData;
    Ptr<void *(void *, unsigned int)> allocFunc;
    Ptr<void(void *, void *)> freeFunc;
    Ptr<void> openFunc;
    Ptr<void> closeFunc;
    Ptr<void> readFunc;
    Ptr<void> seekFunc;
    Ptr<void> errorFunc;
    Ptr<void> ioFinishFunc;
};
static_assert(sizeof(SceFontNewLibParams) == 0x2C);

struct SceFontStyle {
    float fontH;
    float fontV;
    float fontHRes;
    float fontVRes;
    float fontWeight;
    unsigned short fontFamily;
    unsigned short fontStyle;
    unsigned short fontStyleSub;
    unsigned short fontLanguage;
    unsigned short fontRegion;
    unsigned short fontCountry;
    char fontName[64];
    char fontFileName[64];
    unsigned int fontAttributes;
    unsigned int fontExpire;
};

struct SceFontInfo {
    unsigned int maxGlyphWidthI;
    unsigned int maxGlyphHeightI;
    unsigned int maxGlyphAscenderI;
    unsigned int maxGlyphDescenderI;
    unsigned int maxGlyphLeftXI;
    unsigned int maxGlyphBaseYI;
    unsigned int minGlyphCenterXI;
    unsigned int maxGlyphTopYI;
    unsigned int maxGlyphAdvanceXI;
    unsigned int maxGlyphAdvanceYI;
    float maxGlyphWidthF;
    float maxGlyphHeightF;
    float maxGlyphAscenderF;
    float maxGlyphDescenderF;
    float maxGlyphLeftXF;
    float maxGlyphBaseYF;
    float minGlyphCenterXF;
    float maxGlyphTopYF;
    float maxGlyphAdvanceXF;
    float maxGlyphAdvanceYF;
    short maxGlyphWidth;
    short maxGlyphHeight;
    unsigned int charMapLength; // Number of elements in the font's charmap.
    unsigned int shadowMapLength; // Number of elements in the font's shadow charmap.
    SceFontStyle fontStyle;
    uint8_t BPP; // Font's BPP.
    uint8_t pad[3];
};

struct SceFontGlyphImage {
    unsigned int pixelFormat;
    int xPos64;
    int yPos64;
    unsigned short bufWidth;
    unsigned short bufHeight;
    unsigned short bytesPerLine;
    unsigned short pad;
    unsigned int bufferPtr;
};

struct SceFontImageRect {
    unsigned short width;
    unsigned short height;
};

struct SceFontCharInfo {
    unsigned int bitmapWidth;
    unsigned int bitmapHeight;
    unsigned int bitmapLeft;
    unsigned int bitmapTop;
    unsigned int sfp26Width;
    unsigned int sfp26Height;
    int sfp26Ascender;
    int sfp26Descender;
    int sfp26BearingHX;
    int sfp26BearingHY;
    int sfp26BearingVX;
    int sfp26BearingVY;
    int sfp26AdvanceH;
    int sfp26AdvanceV;
    short shadowFlags;
    short shadowId;
};

EXPORT(int, sceFontClose, SceFontHandle fontHandle) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontDoneLib, SceFontLibHandle libHandle) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontFindFont, SceFontLibHandle libHandle, SceFontStyle *fontStyle, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontFindOptimumFont, SceFontLibHandle libHandle, SceFontStyle *fontStyle, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontFlush, SceFontHandle fontHandle) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetCharGlyphImage, SceFontHandle fontHandle, unsigned int charCode, SceFontGlyphImage *glyphImage) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetCharGlyphImage_Clip, SceFontHandle fontHandle, unsigned int charCode, SceFontGlyphImage *glyphImage, int clipXPos, int clipYPos, int clipWidth, int clipHeight) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetCharImageRect, SceFontHandle fontHandle, unsigned int charCode, SceFontImageRect *charRect) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetCharInfo, SceFontHandle fontHandle, unsigned int charCode, SceFontCharInfo *charInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetFontInfo, SceFontHandle fontHandle, SceFontInfo *fontInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetFontInfoByIndexNumber, SceFontLibHandle libHandle, SceFontStyle *fontStyle, int unknown, int fontIndex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetFontList, SceFontLibHandle libHandle, SceFontStyle *fontStyle, int numFonts) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontGetNumFontList, SceFontLibHandle libHandle, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontNewLib, SceFontNewLibParams *fontInitParams, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontOpen, SceFontLibHandle libHandle, int i, int mode, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontOpenUserFile, SceFontLibHandle libHandle, char *file, int mode, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontOpenUserMemory, SceFontLibHandle libHandle, void *pMemoryFont, SceSize pMemoryFontSize, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontPixelToPointH, SceFontLibHandle libHandle, float fontPixelsH, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontPixelToPointV, SceFontLibHandle libHandle, float fontPixelsV, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontPointToPixelH, SceFontLibHandle libHandle, float fontPointsH, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontPointToPixelV, SceFontLibHandle libHandle, float fontPointsV, unsigned int *errorCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontSetAltCharacterCode, SceFontLibHandle libHandle, unsigned int charCode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFontSetResolution, SceFontLibHandle libHandle, float hRes, float vRes) {
    return UNIMPLEMENTED();
}
