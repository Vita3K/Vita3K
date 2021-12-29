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

#include "ScePgf.h"

typedef void *SceFontLibHandle;
typedef void *SceFontHandle;
struct SceFontNewLibParams {
    void *userData;

    unsigned int numFonts;

    // Cache system instance handle
    void *cache;

    // Memory allocation functions (allocFunc, freeFunc)
    void *(*allocFunc)(
        void *, // Pointer to user data
        unsigned int // Request size
    );

    void (*freeFunc)(
        void *, // Pointer to user data
        void * // Pointer to area to be freed
    );

    // File access functions
    void *(*openFunc)
        // Open (Read only)
        (
            void *, // Pointer to user data
            void *, // File name
            signed int * // Error value
        );
    signed int(*closeFunc)
        // Close
        (
            void *, // Pointer to user data
            void * // Identifier for file access
        );
    unsigned int(*readFunc)
        // Read
        (
            void *, // Pointer to user data
            void *, // Identifier for file access
            void *, // Buffer address
            unsigned int, // Number of bytes per read unit
            unsigned int, // Number of reads
            signed int * // Error value
        );
    signed int (*seekFunc)(
        void *, // Pointer to user data
        void *, // Identifier for file access
        unsigned int // Seek destination (absolute position from beginning of file)
    );
    // Callback when error occurs
    int (*onErrorFunc)(
        void *, // Pointer to user data
        int // Error value
    );
    // Callback when reading ends
    int (*whenDoneReadFunc)(
        void *, // Pointer to user data
        int // Status value
    );
};

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

EXPORT(void *, sceFontNewLib, SceFontNewLibParams *fontInitParams, signed int errorCode) {
    return nullptr;
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

BRIDGE_IMPL(sceFontClose)
BRIDGE_IMPL(sceFontDoneLib)
BRIDGE_IMPL(sceFontFindFont)
BRIDGE_IMPL(sceFontFindOptimumFont)
BRIDGE_IMPL(sceFontFlush)
BRIDGE_IMPL(sceFontGetCharGlyphImage)
BRIDGE_IMPL(sceFontGetCharGlyphImage_Clip)
BRIDGE_IMPL(sceFontGetCharImageRect)
BRIDGE_IMPL(sceFontGetCharInfo)
BRIDGE_IMPL(sceFontGetFontInfo)
BRIDGE_IMPL(sceFontGetFontInfoByIndexNumber)
BRIDGE_IMPL(sceFontGetFontList)
BRIDGE_IMPL(sceFontGetNumFontList)
BRIDGE_IMPL(sceFontNewLib)
BRIDGE_IMPL(sceFontOpen)
BRIDGE_IMPL(sceFontOpenUserFile)
BRIDGE_IMPL(sceFontOpenUserMemory)
BRIDGE_IMPL(sceFontPixelToPointH)
BRIDGE_IMPL(sceFontPixelToPointV)
BRIDGE_IMPL(sceFontPointToPixelH)
BRIDGE_IMPL(sceFontPointToPixelV)
BRIDGE_IMPL(sceFontSetAltCharacterCode)
BRIDGE_IMPL(sceFontSetResolution)
