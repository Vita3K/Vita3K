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

#pragma once

#include <overlay/types.h>

#include <stb_rect_pack.h>
#include <stb_truetype.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace overlay {

// Default font name for the overlay system (resolved via system fonts)
inline constexpr const char *default_font_name = "Arial";

enum class language_class {
    default_ = 0, // Typically latin-1, extended latin, hebrew, arabic and cyrillic
    cjk_base = 1, // The thousands of CJK glyphs occupying pages 2E-9F
    hangul = 2 // Korean jamo
};

struct glyph_load_setup {
    std::vector<std::string> font_names;
    std::vector<std::string> lookup_font_dirs;
};

// Each 'page' holds an indexed block of 256 code points
// The BMP (Basic Multilingual Plane) has 256 allocated pages but not all are necessary
struct codepage {
    static constexpr uint32_t bitmap_width = 1024;
    static constexpr uint32_t bitmap_height = 1024;
    static constexpr uint32_t char_count = 256; // 16x16 grid at max 48pt
    static constexpr uint32_t oversample = 2;

    std::vector<stbtt_packedchar> pack_info;
    std::vector<uint8_t> glyph_data;
    char32_t glyph_base = 0;
    float sampler_z = 0.f;

    void initialize_glyphs(char32_t codepage_id, float font_size, const std::vector<uint8_t> &ttf_data, int font_index = 0);
    stbtt_aligned_quad get_char(char32_t c, float &x_advance, float &y_advance);
};

class font {
private:
    float size_pt = 12.f;
    float size_px = 16.f;
    float em_size = 0.f;
    std::string font_name;
    bool bold = false;

    std::vector<std::pair<char32_t, std::unique_ptr<codepage>>> m_glyph_map;
    bool initialized = false;

    struct {
        char32_t codepage_id = 0;
        codepage *page = nullptr;
    } codepage_cache;

    static char32_t get_page_id(char32_t c) { return c >> 8; }
    static language_class classify(char32_t codepage_id);
    glyph_load_setup get_glyph_files(language_class class_) const;
    codepage *initialize_codepage(char32_t c);

public:
    font(std::string_view ttf_name, float size, bool bold = false);

    stbtt_aligned_quad get_char(char32_t c, float &x_advance, float &y_advance);

    std::vector<vertex> render_text_ex(float &x_advance, float &y_advance, const char32_t *text, size_t char_limit, uint16_t max_width, bool wrap);

    std::vector<vertex> render_text(const char32_t *text, uint16_t max_width = UINT16_MAX, bool wrap = false);

    std::pair<float, float> get_char_offset(const char32_t *text, size_t max_length, uint16_t max_width = UINT16_MAX, bool wrap = false);

    bool matches(std::string_view name, int size, bool requested_bold) const {
        return static_cast<int>(size_pt) == size && font_name == name && bold == requested_bold;
    }
    std::string_view get_name() const { return font_name; }
    float get_size_pt() const { return size_pt; }
    float get_size_px() const { return size_px; }
    float get_em_size() const { return em_size; }
    bool is_bold() const { return bold; }

    // Renderer info
    size3u get_glyph_data_dimensions() const { return { codepage::bitmap_width, codepage::bitmap_height, static_cast<uint32_t>(m_glyph_map.size()) }; }
    const std::vector<uint8_t> &get_glyph_data() const;
};

class fontmgr {
private:
    std::vector<std::unique_ptr<font>> fonts;
    static fontmgr *m_instance;
    static std::vector<std::string> s_system_font_dirs;
    static std::string s_firmware_font_dir;
    static std::string s_fallback_font_dir;
    static int s_system_lang;

    font *find(std::string_view name, int size, bool bold) {
        for (const auto &f : fonts) {
            if (f->matches(name, size, bold))
                return f.get();
        }

        fonts.push_back(std::make_unique<font>(name, static_cast<float>(size), bold));
        return fonts.back().get();
    }

public:
    fontmgr() = default;
    ~fontmgr() {
        if (m_instance) {
            m_instance = nullptr;
        }
    }

    static font *get(std::string_view name, int size, bool bold = false) {
        if (m_instance == nullptr)
            m_instance = new fontmgr;

        return m_instance->find(name, size, bold);
    }

    static void set_system_font_dirs(const std::vector<std::string> &dirs) {
        s_system_font_dirs = dirs;
    }

    static const std::vector<std::string> &get_system_font_dirs() {
        return s_system_font_dirs;
    }

    static void set_firmware_font_dir(const std::string &dir) {
        s_firmware_font_dir = dir;
    }

    static const std::string &get_firmware_font_dir() {
        return s_firmware_font_dir;
    }

    static void set_fallback_font_dir(const std::string &dir) {
        s_fallback_font_dir = dir;
    }

    static const std::string &get_fallback_font_dir() {
        return s_fallback_font_dir;
    }

    // System language (SceSystemParamLang value) for language-aware font selection
    static void set_system_lang(int lang) {
        s_system_lang = lang;
    }

    static int get_system_lang() {
        return s_system_lang;
    }

    static void shutdown() {
        delete m_instance;
        m_instance = nullptr;
        s_system_font_dirs.clear();
        s_firmware_font_dir.clear();
        s_fallback_font_dir.clear();
        s_system_lang = 0;
    }
};

} // namespace overlay
