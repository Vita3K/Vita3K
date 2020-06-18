#pragma once

#include <ft2build.h>
#include <map>
#include <pvf/types.h>
#include FT_FREETYPE_H

struct IOState;
struct MemState;

struct PvfLibrary;

struct PvfFont;

struct PvfState {
    std::map<ScePvfLibID, std::shared_ptr<PvfLibrary>> libs;
    std::map<ScePvfFontId, ScePvfLibID> font_to_lib;
    ScePvfLibID next_lib_id = 0;
    ScePvfFontId next_font_id = 0;
};

struct PvfFont {
    ScePvfLibID lib_id;
    ScePvfFontInfo info;

    float hsize;
    float vsize;
    float default_em;

    int get_info(ScePvfFontInfo *info);
    void set_char_size(PvfLibrary &lib, SceFloat32 hsize, SceFloat32 vsize);
    int get_char_info(PvfLibrary &lib, ScePvfCharCode charCode, ScePvfCharInfo *info);
    int render(PvfLibrary &lib, MemState &mem, ScePvfCharCode charCode, ScePvfUserImageBufferRec *buffer);

    ~PvfFont();

    friend struct PvfLibrary;

private:
    FT_Face face;
};

struct PvfLibrary {
    ScePvfLibID lib_id;
    std::map<ScePvfFontId, std::shared_ptr<PvfFont>> fonts;

    bool init();
    ScePvfFontId load_font_file(PvfState &state, MemState &mem, IOState &io, std::string file, const std::string &pref_path);
    ScePvfFontId load_font_file(PvfState &state, MemState &mem, Ptr<uint8_t> data, uint32_t size);
    void set_em(SceFloat32 em);
    void set_resolution(SceFloat32 hsize, SceFloat32 vsize);
    float points_to_pixel(SceFloat32 points, bool use_h = true);
    void update_fonts(PvfState &state);
    std::shared_ptr<PvfFont> get_font(ScePvfFontId font_id);

    ~PvfLibrary();

    friend struct PvfFont;

private:
    ScePvfFontId add_font(PvfState &state, FT_Face face);

    FT_Library library;
    SceFloat32 em;
    SceFloat32 hResolution;
    SceFloat32 vResolution;
};

bool init(PvfState &state);
ScePvfLibID create_pvf_library(PvfState &state);
std::shared_ptr<PvfLibrary> get_pvf_library(PvfState &state, ScePvfLibID lib_id);
std::shared_ptr<PvfFont> get_pvf_font(PvfState &state, ScePvfFontId font_id);
void clean_pvf_library(PvfState &state, ScePvfLibID lib_id);
void clean_pvf_font(PvfState &state, ScePvfFontId font_id);
