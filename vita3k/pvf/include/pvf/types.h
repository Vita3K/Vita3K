#pragma once

#include <mem/ptr.h>
#include <util/types.h>

typedef uint32_t ScePvfFontIndex;

enum ScePvfLanguageCode {
    SCE_PVF_DEFAULT_LANGUAGE_CODE = 0,
    SCE_PVF_LANGUAGE_J = 1,
    SCE_PVF_LANGUAGE_LATIN = 2,
    SCE_PVF_LANGUAGE_K = 3,
    SCE_PVF_LANGUAGE_C = 4,
    SCE_PVF_LANGUAGE_CJK = 5
};

enum ScePvfStyleCode {
    SCE_PVF_DEFAULT_STYLE_CODE = 0,
    SCE_PVF_STYLE_REGULAR = 1,
    SCE_PVF_STYLE_OBLIQUE = 2,
    SCE_PVF_STYLE_NARROW = 3,
    SCE_PVF_STYLE_NARROW_OBLIQUE = 4,
    SCE_PVF_STYLE_BOLD = 5,
    SCE_PVF_STYLE_BOLD_OBLIQUE = 6,
    SCE_PVF_STYLE_BLACK = 7,
    SCE_PVF_STYLE_BLACK_OBLIQUE = 8,
    SCE_PVF_STYLE_L = 101,
    SCE_PVF_STYLE_M = 102,
    SCE_PVF_STYLE_DB = 103,
    SCE_PVF_STYLE_B = 104,
    SCE_PVF_STYLE_EB = 105,
    SCE_PVF_STYLE_UB = 106
};

typedef uint16_t ScePvfCharCode;

enum ScePvfImageBufferPixelFormatType {
    SCE_PVF_USERIMAGE_DIRECT4_L = 0,
    SCE_PVF_USERIMAGE_DIRECT8 = 2
};

enum ScePvfErrorCode {
    SCE_PVF_OK = 0x0,
    SCE_PVF_ERROR_NOGLYPH = 0x8046000d,
    SCE_PVF_ERROR_NOFILE = 0x80460004
};

typedef void *(*ScePvfFontChcheFindFunc)(void *chcheInstance, uint32_t hashValue, void *key, bool *result);
typedef signed int (*ScePvfFontCacheLockFunc)(void *cacheInstance);

typedef signed int (*ScePvfFontCacheLockFunc)(void *cacheInstance);
typedef signed int (*ScePvfFontChcheReadFromCacheFunc)(void *cacheInstance, void *cacheSlot, void *data0);

typedef signed int (*ScePvfFontChcheUnlockFunc)(void *cacheInstance);
typedef signed int (*ScePvfFontChcheUnlockFunc)(void *cacheInstance);
typedef signed int (*ScePvfFontChcheWriteToCacheFunc)(void *cacheInstance, void *cacheSlot, void *data0, int size);
typedef signed int (*ScePvfFontChcheWriteKeyValueToCacheFunc)(void *cacheInstance, void *chcheSlot, void *key);
typedef void (*ScePvfFreeFunc)(void *userData, void *ptr);
typedef void *(*ScePvfReallocFunc)(void *userData, void *old_ptr, unsigned int size);

typedef uint32_t ScePvfFontId;

struct ScePvfCacheSystemInterface {
    void *cacheInstance;
    ScePvfFontChcheFindFunc findFunc;
    ScePvfFontCacheLockFunc lockFunc;
    ScePvfFontChcheReadFromCacheFunc read0FromCacheFunc;
    ScePvfFontChcheReadFromCacheFunc read1FromCacheFunc;
    ScePvfFontChcheUnlockFunc unlockFunc;
    ScePvfFontChcheWriteToCacheFunc write0ToCacheFunc;
    ScePvfFontChcheWriteToCacheFunc write1ToCacheFunc;
    ScePvfFontChcheWriteKeyValueToCacheFunc writeKeyValueToCacheFunc;
};

typedef void *(*ScePvfAllocFunc)(void *userData, unsigned int size);
typedef unsigned int ScePvfError;
typedef uint16_t ScePvfCharCode;

typedef uint32_t ScePvfLibID;

struct ScePvfInitRec {
    ScePvfAllocFunc allocFunc;
    ScePvfCacheSystemInterface *cache;
    ScePvfFreeFunc freeFunc;
    unsigned int maxNumFonts;
    ScePvfReallocFunc reallocFunc;
    void *reserved;
    void *userData;
};

struct ScePvfIrect {
    uint16_t height;
    uint16_t width;
};

struct ScePvfUserImageBufferRec {
    uint32_t pixelFormat;
    int32_t xPos64;
    int32_t yPos64;
    ScePvfIrect rect;
    uint16_t bytesPerLine;
    uint16_t reserved;
    Address buffer;
};

#define SCE_PVF_FONTFILENAME_LENGTH 64
#define SCE_PVF_FONTNAME_LENGTH 64
#define SCE_PVF_STYLENAME_LENGTH 64

struct ScePvfFontStyleInfo {
    float weight;
    uint16_t familyCode;
    uint16_t style;
    uint16_t subStyle;
    uint16_t languageCode;
    uint16_t regionCode;
    uint16_t countryCode;
    uint8_t fontName[SCE_PVF_FONTNAME_LENGTH];
    uint8_t styleName[SCE_PVF_STYLENAME_LENGTH];
    uint8_t fileName[SCE_PVF_FONTFILENAME_LENGTH];
    uint32_t extraAttributes;
    uint32_t expireDate;
};

struct ScePvfFGlyphMetricsInfo {
    float width;
    float height;
    float ascender;
    float descender;
    float horizontalBearingX;
    float horizontalBearingY;
    float verticalBearingX;
    float verticalBearingY;
    float horizontalAdvance;
    float verticalAdvance;
};

struct ScePvfIGlyphMetricsInfo {
    uint32_t width64;
    uint32_t height64;
    int32_t ascender64;
    int32_t descender64;
    int32_t horizontalBearingX64;
    int32_t horizontalBearingY64;
    int32_t verticalBearingX64;
    int32_t verticalBearingY64;
    int32_t horizontalAdvance64;
    int32_t verticalAdvance64;
};

struct ScePvfFontInfo {
    ScePvfIGlyphMetricsInfo maxIGlyphMetrics;
    ScePvfFGlyphMetricsInfo maxFGlyphMetrics;
    uint32_t numChars;
    ScePvfFontStyleInfo fontStyleInfo;
    uint8_t reserved[4];
};

struct ScePvfCharInfo {
    uint32_t bitmapWidth;
    uint32_t bitmapHeight;
    int32_t bitmapLeft;
    int32_t bitmapTop;
    ScePvfIGlyphMetricsInfo glyphMetrics;
    uint8_t reserved[4];
};
