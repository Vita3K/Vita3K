#include <iomanip>
#include <iostream>
#include <pvf/state.h>
#include <util/log.h>
#include FT_TRUETYPE_TABLES_H
#include FT_GLYPH_H

void debug_buffer(ScePvfUserImageBufferRec *buffer, uint8_t *nBuffer) {
    for (int py = 0; py < buffer->rect.height; py++) {
        for (int px = 0; px < buffer->bytesPerLine; px++) {
            std::cout << std::setfill('0') << std::setw(4) << (int)nBuffer[py * buffer->bytesPerLine + px] << " ";
        }
        std::cout << std::endl;
    }
}

int32_t fix(float x) {
    auto factor = 1 << 6;
    return static_cast<int32_t>(x * factor);
}

float unfix(int32_t x) {
    auto factor = 1 << 6;
    return static_cast<float>(x) / static_cast<float>(factor);
}

PvfFont::~PvfFont() {
    FT_Done_Face(face);
}

void PvfFont::set_char_size(PvfLibrary &lib, SceFloat32 hsize, SceFloat32 vsize) {
    this->hsize = hsize;
    this->vsize = vsize;
    FT_Set_Char_Size(this->face, fix(hsize), fix(vsize), lib.hResolution, lib.vResolution);
}

int PvfFont::render(PvfLibrary &lib, MemState &mem, ScePvfCharCode code, ScePvfUserImageBufferRec *buffer) {
    auto index = FT_Get_Char_Index(this->face, code);
    if (index == 0) {
        return 1;
    }

    auto error = FT_Load_Glyph(this->face, index, FT_LOAD_TARGET_NORMAL);
    if (error) {
        return 1;
    }

    FT_Glyph glyph;
    error = FT_Get_Glyph(face->glyph, &glyph);
    if (error) {
        return 1;
    }

    FT_Glyph glyph2;
    error = FT_Get_Glyph(face->glyph, &glyph2);
    if (error) {
        return 1;
    }

    error = FT_Glyph_To_Bitmap(&glyph2, FT_RENDER_MODE_NORMAL, 0, true);
    if (error) {
        FT_Done_Glyph(glyph);
        return 1;
    }

    auto glyph_bitmap = (FT_BitmapGlyph)glyph2;

    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_GRIDFIT, &bbox);

    auto nBufferPtr = Ptr<uint8_t>(buffer->buffer);
    auto nBuffer = nBufferPtr.get(mem);

    auto bheight = static_cast<int>(buffer->rect.height);
    auto gheight = static_cast<int>(glyph_bitmap->bitmap.rows);
    auto bwidth = static_cast<int>(buffer->rect.width);
    auto gwidth = static_cast<int>(glyph_bitmap->bitmap.width);

    auto y = static_cast<int>(unfix(face->glyph->metrics.vertBearingY));
    auto x = static_cast<int>(unfix(face->glyph->metrics.horiBearingX));

    int yEnd = gheight + y;
    int xEnd = gwidth + x;

    for (int py = 0; py < buffer->rect.height; py++) {
        for (int px = 0; px < buffer->bytesPerLine; px++) {
            if (x <= px && px < xEnd && y <= py && py < yEnd) {
                uint8_t intensity = glyph_bitmap->bitmap.buffer[(py - y) * glyph_bitmap->bitmap.pitch + (px - x)];
                nBuffer[py * buffer->bytesPerLine + px] = intensity;
            } else {
                nBuffer[py * buffer->bytesPerLine + px] = 0;
            }
        }
    }

    FT_Done_Glyph(glyph);

    return 0;
}

int PvfFont::get_char_info(PvfLibrary &lib, ScePvfCharCode code, ScePvfCharInfo *info) {
    auto index = FT_Get_Char_Index(this->face, code);
    if (index == 0) {
        return 1;
    }

    auto error = FT_Load_Glyph(this->face, index, 0);
    if (error) {
        return 1;
    }

    FT_Glyph glyph;
    error = FT_Get_Glyph(face->glyph, &glyph);
    if (error) {
        return 1;
    }

    FT_Glyph glyph2;
    error = FT_Get_Glyph(face->glyph, &glyph2);
    if (error) {
        return 1;
    }

    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_GRIDFIT, &bbox);
    auto g = this->face->glyph;

    error = FT_Glyph_To_Bitmap(&glyph2, FT_RENDER_MODE_NORMAL, 0, true);
    if (error) {
        FT_Done_Glyph(glyph);
        return 1;
    }

    auto glyph_bitmap = (FT_BitmapGlyph)glyph;

    info->bitmapWidth = glyph_bitmap->bitmap.width;
    info->bitmapTop = glyph_bitmap->top;
    info->bitmapHeight = glyph_bitmap->bitmap.rows;
    info->bitmapLeft = glyph_bitmap->left;

    info->glyphMetrics.ascender64 = bbox.yMax;
    info->glyphMetrics.descender64 = bbox.yMin;
    info->glyphMetrics.height64 = bbox.yMax - bbox.yMin;
    info->glyphMetrics.width64 = 0;

    info->glyphMetrics.horizontalAdvance64 = g->metrics.horiAdvance;
    info->glyphMetrics.verticalAdvance64 = g->metrics.vertAdvance;
    info->glyphMetrics.horizontalBearingX64 = g->metrics.horiBearingX;
    info->glyphMetrics.verticalBearingX64 = g->metrics.vertBearingX;
    info->glyphMetrics.horizontalBearingY64 = g->metrics.horiBearingY;
    info->glyphMetrics.verticalBearingY64 = g->metrics.vertBearingY;

    FT_Done_Glyph(glyph);

    return 0;
}

int PvfFont::get_info(ScePvfFontInfo *info) {
    TT_Header *header = reinterpret_cast<TT_Header *>(FT_Get_Sfnt_Table(face, FT_SFNT_HEAD));
    if (header == nullptr) {
        return 1;
    }

    TT_PCLT *pclt = reinterpret_cast<TT_PCLT *>(FT_Get_Sfnt_Table(face, FT_SFNT_PCLT));

    if (pclt != nullptr) {
        info->fontStyleInfo.weight = pclt->StrokeWeight;
        info->fontStyleInfo.style = pclt->Style;
        info->fontStyleInfo.familyCode = pclt->TypeFamily;
        info->fontStyleInfo.subStyle = pclt->SerifStyle;
        memcpy(info->fontStyleInfo.fontName, pclt->FileName, sizeof(pclt->FileName));
        memcpy(info->fontStyleInfo.styleName, "Regular", sizeof("Regular"));
    }

    info->maxIGlyphMetrics.ascender64 = face->size->metrics.ascender;
    info->maxIGlyphMetrics.descender64 = face->size->metrics.descender;
    info->maxIGlyphMetrics.height64 = face->size->metrics.height;
    info->maxIGlyphMetrics.width64 = 0;
    info->maxIGlyphMetrics.horizontalAdvance64 = face->size->metrics.max_advance;
    info->maxIGlyphMetrics.verticalAdvance64 = face->size->metrics.max_advance;
    info->maxIGlyphMetrics.horizontalBearingX64 = 0;
    info->maxIGlyphMetrics.horizontalBearingY64 = 0;
    info->maxIGlyphMetrics.verticalBearingX64 = 0;
    info->maxIGlyphMetrics.verticalBearingY64 = 0;

    info->maxFGlyphMetrics.ascender = unfix(info->maxIGlyphMetrics.ascender64);
    info->maxFGlyphMetrics.descender = unfix(info->maxIGlyphMetrics.descender64);
    info->maxFGlyphMetrics.height = unfix(info->maxIGlyphMetrics.height64);
    info->maxFGlyphMetrics.width = 0;
    info->maxFGlyphMetrics.horizontalAdvance = unfix(info->maxIGlyphMetrics.horizontalAdvance64);
    info->maxFGlyphMetrics.verticalAdvance = unfix(info->maxIGlyphMetrics.verticalAdvance64);
    info->maxFGlyphMetrics.horizontalBearingX = 0;
    info->maxFGlyphMetrics.horizontalBearingY = 0;
    info->maxFGlyphMetrics.verticalBearingX = 0;
    info->maxFGlyphMetrics.verticalBearingY = 0;

    info->numChars = face->num_glyphs;
    return 0;
}
