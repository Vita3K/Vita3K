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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#include <overlay/font.h>

#include <util/fs.h>
#include <util/log.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cmath>
#include <cstring>
#include <iterator>

namespace overlay {

fontmgr *fontmgr::m_instance = nullptr;
std::vector<std::string> fontmgr::s_system_font_dirs;
std::string fontmgr::s_firmware_font_dir;
int fontmgr::s_system_lang = 0;

static bool file_exists(const std::string &path) {
    return fs::exists(fs::path(path));
}

void codepage::initialize_glyphs(char32_t codepage_id, float font_size, const std::vector<uint8_t> &ttf_data, int font_index) {
    glyph_base = (codepage_id * 256);
    glyph_data.resize(bitmap_width * bitmap_height);
    pack_info.resize(256);

    stbtt_pack_context context;
    if (!stbtt_PackBegin(&context, glyph_data.data(), bitmap_width, bitmap_height, 0, 1, nullptr)) {
        LOG_ERROR("Font packing failed");
        return;
    }

    stbtt_PackSetOversampling(&context, oversample, oversample);

    if (!stbtt_PackFontRange(&context, ttf_data.data(), font_index, font_size, (codepage_id * 256), 256, pack_info.data())) {
        LOG_ERROR("Font packing failed");
        stbtt_PackEnd(&context);
        return;
    }

    stbtt_PackEnd(&context);
}

stbtt_aligned_quad codepage::get_char(char32_t c, float &x_advance, float &y_advance) {
    stbtt_aligned_quad quad;
    stbtt_GetPackedQuad(pack_info.data(), bitmap_width, bitmap_height, (c - glyph_base), &x_advance, &y_advance, &quad, false);

    quad.t0 += sampler_z;
    quad.t1 += sampler_z;
    return quad;
}

font::font(std::string_view ttf_name, float size, bool bold)
    : bold(bold) {
    // Convert pt to px
    size_px = std::ceil(size * 96.f / 72.f);
    size_pt = size;

    font_name = std::string(ttf_name);
    initialized = true;
}

language_class font::classify(char32_t codepage_id) {
    switch (codepage_id) {
    case 0x00: // Extended ASCII
    case 0x04: // Cyrillic
        return language_class::default_;

    case 0x11: // Hangul jamo
    case 0x31: // Compatibility jamo 3130-318F
        return language_class::hangul;

    case 0xFF: // Halfwidth and Fullwidth Forms
        return language_class::cjk_base;

    default:
        if (codepage_id >= 0xAC && codepage_id <= 0xD7) {
            // Hangul syllables + jamo extended block B
            return language_class::hangul;
        }

        if (codepage_id >= 0x2E && codepage_id <= 0x9F) {
            // Generic CJK blocks, mostly chinese and japanese kana
            return language_class::cjk_base;
        }

        return language_class::default_;
    }
}

glyph_load_setup font::get_glyph_files(language_class class_) const {
    glyph_load_setup result;
    result.font_names.push_back(font_name);

    // Vita firmware font directory
    const auto &fw_dir = fontmgr::get_firmware_font_dir();
    if (!fw_dir.empty()) {
        result.lookup_font_dirs.push_back(fw_dir);
    }

    // System font directories
    const auto &system_dirs = fontmgr::get_system_font_dirs();
    result.lookup_font_dirs.insert(result.lookup_font_dirs.end(), system_dirs.begin(), system_dirs.end());

    // Platform-specific hardcoded font directories
    if (system_dirs.empty()) {
#ifdef _WIN32
        {
            const char *windir = std::getenv("WINDIR");
            if (windir) {
                result.lookup_font_dirs.push_back(std::string(windir) + "\\Fonts\\");
            }
        }
#elif defined(__APPLE__)
        result.lookup_font_dirs.push_back("/System/Library/Fonts/");
        result.lookup_font_dirs.push_back("/Library/Fonts/");
        {
            const char *home = std::getenv("HOME");
            if (home) {
                result.lookup_font_dirs.push_back(std::string(home) + "/Library/Fonts/");
            }
        }
#elif defined(__ANDROID__)
        result.lookup_font_dirs.push_back("/system/fonts/");
#else
        // Linux / BSD
        result.lookup_font_dirs.push_back("/usr/share/fonts/");
        result.lookup_font_dirs.push_back("/usr/share/fonts/truetype/");
        result.lookup_font_dirs.push_back("/usr/share/fonts/TTF/");
        result.lookup_font_dirs.push_back("/usr/share/fonts/truetype/dejavu/");
        result.lookup_font_dirs.push_back("/usr/share/fonts/truetype/liberation/");
        result.lookup_font_dirs.push_back("/usr/share/fonts/noto/");
        {
            const char *home = std::getenv("HOME");
            if (home) {
                result.lookup_font_dirs.push_back(std::string(home) + "/.local/share/fonts/");
            }
        }
#endif
    }

    switch (class_) {
    case language_class::default_: {
        result.font_names.clear();

        if (!font_name.empty()) {
            result.font_names.push_back(font_name);
        }
        result.font_names.emplace_back(bold ? "ltn4.pvf" : "ltn0.pvf");
        // System fonts
        result.font_names.emplace_back("Arial.ttf");
        result.font_names.emplace_back("arial.ttf");
#ifdef __APPLE__
        result.font_names.emplace_back("DejaVuSans.ttf");
        result.font_names.emplace_back("NotoSans-Regular.ttf");
        result.font_names.emplace_back("Roboto-Regular.ttf");
        result.font_names.emplace_back("OpenSans-Regular.ttf");
        result.font_names.emplace_back("FreeSans.ttf");
#elif defined(__ANDROID__)
        result.font_names.emplace_back("Roboto-Regular.ttf");
        result.font_names.emplace_back("DroidSans.ttf");
        result.font_names.emplace_back("NotoSans-Regular.ttf");
#elif !defined(_WIN32)
        result.font_names.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        result.font_names.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
#endif
        break;
    }
    case language_class::cjk_base: {
        result.font_names.clear();

        // Vita firmware fonts first
        if (bold) {
            result.font_names.emplace_back("jpn4.pvf");
            result.font_names.emplace_back("cn4.pvf");
        }
        result.font_names.emplace_back("jpn0.pvf");
        result.font_names.emplace_back("cn0.pvf");

        // Known system fonts
        result.font_names.emplace_back("Yu Gothic.ttf");
        result.font_names.emplace_back("YuGothR.ttc");
#ifdef _WIN32
        result.font_names.emplace_back("msyh.ttc");
        result.font_names.emplace_back("simsunb.ttc");
        result.font_names.emplace_back("simsun.ttc");
        result.font_names.emplace_back("SimsunExtG.ttf");
#elif defined(__APPLE__)
        result.font_names.emplace_back("Hiragino Sans GB.ttc");
        result.font_names.emplace_back("PingFang.ttc");
#elif defined(__ANDROID__)
        result.font_names.emplace_back("NotoSansCJK-Regular.ttc");
        result.font_names.emplace_back("NotoSansJP-Regular.otf");
        result.font_names.emplace_back("NotoSansSC-Regular.otf");
#else
        result.font_names.emplace_back("NotoSansCJK-Regular.ttc");
        result.font_names.emplace_back("NotoSansSC-Regular.otf");
        result.font_names.emplace_back("wqy-microhei.ttc");
#endif
        break;
    }
    case language_class::hangul: {
        result.font_names.clear();

        // Vita firmware font first
        if (bold) {
            result.font_names.emplace_back("kr4.pvf");
        }
        result.font_names.emplace_back("kr0.pvf");

        // Known system fonts
        result.font_names.emplace_back("Malgun Gothic.ttf");
        result.font_names.emplace_back("malgun.ttf");
#ifdef __APPLE__
        result.font_names.emplace_back("AppleSDGothicNeo.ttc");
#elif defined(__ANDROID__)
        result.font_names.emplace_back("NotoSansKR-Regular.otf");
        result.font_names.emplace_back("NotoSansCJK-Regular.ttc");
#elif !defined(_WIN32)
        result.font_names.emplace_back("NotoSansKR-Regular.otf");
        result.font_names.emplace_back("NotoSansCJK-Regular.ttc");
#endif
        break;
    }
    }

    return result;
}

codepage *font::initialize_codepage(char32_t c) {
    const auto codepage_id = get_page_id(c);
    const auto class_ = classify(codepage_id);
    const auto fs_settings = get_glyph_files(class_);

    std::vector<uint8_t> bytes;
    std::vector<uint8_t> fallback_bytes;
    std::string fallback_file;
    int found_font_index = 0;
    int fallback_font_index = 0;
    bool font_found = false;

    const auto try_load_font = [&](const std::string &file_path) -> bool {
        if (!fs_utils::read_data(fs::path(file_path), bytes))
            return false;

        // Try all font indices in the file
        const int num_fonts = stbtt_GetNumberOfFonts(bytes.data());
        const int max_indices = (num_fonts > 0) ? num_fonts : 1;
        for (int fi = 0; fi < max_indices; fi++) {
            stbtt_fontinfo info;
            const int offset = stbtt_GetFontOffsetForIndex(bytes.data(), fi);
            if (offset < 0)
                break;
            if (stbtt_InitFont(&info, bytes.data(), offset) != 0) {
                if (stbtt_FindGlyphIndex(&info, c) != 0) {
                    font_found = true;
                    found_font_index = fi;
                    break;
                }
            }
        }

        if (!font_found) {
            if (fallback_bytes.empty()) {
                fallback_bytes = std::move(bytes);
                fallback_file = file_path;
                fallback_font_index = 0;
            }
            bytes.clear();
        } else {
            LOG_DEBUG("Overlay font: loaded '{}' (index {}) for character 0x{:x} (codepage {})",
                file_path, found_font_index, static_cast<uint32_t>(c), static_cast<uint32_t>(get_page_id(c)));
        }

        return font_found;
    };

    for (const std::string &font_file : fs_settings.font_names) {
        if (file_exists(font_file)) {
            // Check for absolute paths or fonts in the working directory
            if (try_load_font(font_file)) {
                break;
            }
            continue;
        }

        std::string extension;
        if (const auto extension_start = font_file.find_last_of('.');
            extension_start != std::string::npos) {
            extension = font_file.substr(extension_start + 1);
        }

        std::string file_name = font_file;
        if (extension.length() != 3) {
            file_name += ".ttf";
        }

        for (const auto &font_dir : fs_settings.lookup_font_dirs) {
            const std::string file_path = font_dir + file_name;
            if (file_exists(file_path)) {
                if (try_load_font(file_path)) {
                    break;
                }
            }
        }

        if (font_found) {
            break;
        }
    }

    if (!font_found) {
        if (fallback_bytes.empty()) {
            LOG_ERROR("Failed to initialize font for character 0x{:x} on codepage {}. No suitable font file found.",
                static_cast<uint32_t>(c), static_cast<uint32_t>(codepage_id));

            // Create a safe empty codepage with zeroed glyph data
            codepage_cache.page = nullptr;
            auto page = std::make_unique<codepage>();
            page->pack_info.resize(codepage::char_count);
            page->glyph_data.resize(codepage::bitmap_width * codepage::bitmap_height, 0);
            page->glyph_base = codepage_id * 256;
            page->sampler_z = static_cast<float>(m_glyph_map.size());
            auto ret = page.get();
            m_glyph_map.emplace_back(codepage_id, std::move(page));
            return ret;
        }

        LOG_WARN("Font for character 0x{:x} on codepage {} not found in preferred fonts. Falling back to '{}'",
            static_cast<uint32_t>(c), static_cast<uint32_t>(codepage_id), fallback_file);
        bytes = std::move(fallback_bytes);
        found_font_index = fallback_font_index;
    }

    codepage_cache.page = nullptr;
    auto page = std::make_unique<codepage>();
    page->initialize_glyphs(codepage_id, size_px, bytes, found_font_index);
    page->sampler_z = static_cast<float>(m_glyph_map.size());

    auto ret = page.get();
    m_glyph_map.emplace_back(codepage_id, std::move(page));

    if (codepage_id == 0) {
        // Latin-1:
        float unused;
        get_char('m', em_size, unused);
    }

    return ret;
}

stbtt_aligned_quad font::get_char(char32_t c, float &x_advance, float &y_advance) {
    if (!initialized)
        return {};

    const auto page_id = get_page_id(c);

    if (codepage_cache.codepage_id == page_id && codepage_cache.page) [[likely]] {
        return codepage_cache.page->get_char(c, x_advance, y_advance);
    } else {
        codepage_cache.codepage_id = page_id;
        codepage_cache.page = nullptr;

        for (const auto &e : m_glyph_map) {
            if (e.first == page_id) {
                codepage_cache.page = e.second.get();
                break;
            }
        }

        if (!codepage_cache.page) [[unlikely]] {
            codepage_cache.page = initialize_codepage(c);
        }

        return codepage_cache.page->get_char(c, x_advance, y_advance);
    }
}

std::vector<vertex> font::render_text_ex(float &x_advance, float &y_advance, const char32_t *text, size_t char_limit, uint16_t max_width, bool wrap) {
    x_advance = 0.f;
    y_advance = 0.f;
    std::vector<vertex> result;

    if (!initialized) {
        return result;
    }

    for (size_t i = 0, begin_of_word = 0; i < char_limit; i++) {
        switch (const auto &c = text[i]) {
        case '\0': {
            return result;
        }
        case '\n': {
            x_advance = 0.f;
            y_advance += size_px + 2.f;
            begin_of_word = result.size();
            continue;
        }
        case '\r': {
            x_advance = 0.f;
            begin_of_word = result.size();
            continue;
        }
        default: {
            const bool is_whitespace = c == ' ';
            stbtt_aligned_quad quad{};

            if (is_whitespace) {
                if (x_advance <= 0.f) {
                    quad.x0 = quad.x1 = x_advance;
                    quad.y0 = quad.y1 = y_advance;
                } else {
                    const float x_advance_old = x_advance;
                    const float y_advance_old = y_advance;

                    quad = get_char(c, x_advance, y_advance);

                    if (x_advance > max_width) {
                        quad.x0 = quad.x1 = x_advance_old;
                        quad.y0 = quad.y1 = y_advance_old;
                    }
                }
            } else {
                quad = get_char(c, x_advance, y_advance);
            }

            result.emplace_back(quad.x0, quad.y0, quad.s0, quad.t0);
            result.emplace_back(quad.x1, quad.y0, quad.s1, quad.t0);
            result.emplace_back(quad.x0, quad.y1, quad.s0, quad.t1);
            result.emplace_back(quad.x1, quad.y1, quad.s1, quad.t1);

            if (is_whitespace) {
                begin_of_word = result.size();
            }

            if (x_advance > max_width) {
                if (wrap) {
                    y_advance += size_px + 2.f;

                    if (is_whitespace) {
                        x_advance = 0.f;
                        break;
                    }

                    const float base_x = result[begin_of_word].x();

                    for (size_t n = begin_of_word; n < result.size(); ++n) {
                        result[n].x() -= base_x;
                        result[n].y() += size_px + 2.f;
                    }

                    x_advance = result.back().x();
                } else {
                    // TODO: Ellipsize
                }
            }

            break;
        }
        } // switch
    }
    return result;
}

std::vector<vertex> font::render_text(const char32_t *text, uint16_t max_width, bool wrap) {
    float unused_x, unused_y;
    return render_text_ex(unused_x, unused_y, text, SIZE_MAX, max_width, wrap);
}

std::pair<float, float> font::get_char_offset(const char32_t *text, size_t max_length, uint16_t max_width, bool wrap) {
    float loc_x, loc_y;
    render_text_ex(loc_x, loc_y, text, max_length, max_width, wrap);
    return { loc_x, loc_y };
}

const std::vector<uint8_t> &font::get_glyph_data() const {
    constexpr uint32_t page_size = codepage::bitmap_width * codepage::bitmap_height;
    const auto size = page_size * m_glyph_map.size();

    static thread_local std::vector<uint8_t> bytes;
    bytes.resize(size);

    uint8_t *data = bytes.data();

    for (const auto &e : m_glyph_map) {
        std::memcpy(data, e.second->glyph_data.data(), page_size);
        data += page_size;
    }
    return bytes;
}

} // namespace overlay
