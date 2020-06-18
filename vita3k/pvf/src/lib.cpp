#include <cstring>
#include <io/functions.h>
#include <io/state.h>
#include <mem/mem.h>
#include <pvf/state.h>
#include <util/log.h>

bool PvfLibrary::init() {
    auto error = FT_Init_FreeType(&this->library);
    if (error) {
        LOG_INFO("error {}", error);
        return error;
    }
}

PvfLibrary::~PvfLibrary() {
    FT_Done_FreeType(this->library);
}

void PvfLibrary::set_resolution(SceFloat32 hsize, SceFloat32 vsize) {
    this->hResolution = hsize;
    this->vResolution = vsize;
}

std::shared_ptr<PvfFont> PvfLibrary::get_font(ScePvfFontId font_id) {
    return this->fonts[font_id];
}

void PvfLibrary::update_fonts(PvfState &state) {
    for (const auto &font : fonts) {
        font.second->set_char_size(*this, font.second->hsize, font.second->vsize);
    }
}

void PvfLibrary::set_em(SceFloat32 em) {
    this->em = em;
}

float PvfLibrary::points_to_pixel(SceFloat32 points, bool use_h) {
    if (use_h) {
        return points * this->hResolution / 72.0;
    }
    return points * this->vResolution / 72.0;
}

ScePvfFontId PvfLibrary::load_font_file(PvfState &state, MemState &mem, IOState &io, std::string file, const std::string &pref_path) {
    FT_Face face;

    const auto path = expand_path(io, file.c_str(), pref_path);
    auto error = FT_New_Face(this->library, path.c_str(), 0, &face);
    if (error) {
        return 0;
    }

    return add_font(state, face);
}

ScePvfFontId PvfLibrary::load_font_file(PvfState &state, MemState &mem, Ptr<uint8_t> data, uint32_t size) {
    auto font = std::shared_ptr<PvfFont>(new PvfFont);

    FT_Face face;
    auto error = FT_New_Memory_Face(library, data.get(mem), size, 0, &face);
    if (error) {
        return 0;
    }

    return add_font(state, face);
}

ScePvfFontId PvfLibrary::add_font(PvfState &state, FT_Face face) {
    auto font = std::shared_ptr<PvfFont>(new PvfFont);
    auto error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (error) {
        return 0;
    }

    font->default_em = face->units_per_EM;

    ++state.next_font_id;
    auto id = state.next_font_id;
    font->face = face;
    font->lib_id = this->lib_id;
    state.font_to_lib.emplace(id, this->lib_id);
    fonts.emplace(id, font);
    return id;
}
