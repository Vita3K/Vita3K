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

#include <overlay/compiled_resource.h>
#include <overlay/font.h>

#include <string>
#include <string_view>
#include <vector>

namespace overlay {

// Helper to convert UTF-8 string to UTF-32
std::u32string utf8_to_u32string(std::string_view utf8);

struct overlay_element {
    enum text_align {
        left = 0,
        center,
        right
    };

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    std::u32string text;
    font *font_ref = nullptr;
    text_align alignment = left;
    bool wrap_text = false;
    bool clip_text = true;

    color4f back_color = { 0.f, 0.f, 0.f, 1.f };
    color4f fore_color = { 1.f, 1.f, 1.f, 1.f };
    bool pulse_effect_enabled = false;
    float pulse_sinus_offset = 0.0f;
    float pulse_speed_modifier = 0.005f;

    uint8_t border_size = 0;
    color4f border_color = { 0.f, 0.f, 0.f, 1.f };

    void set_sinus_offset(float sinus_modifier);

    compiled_resource compiled_resources;

    bool visible = true;

    uint16_t padding_left = 0;
    uint16_t padding_right = 0;
    uint16_t padding_top = 0;
    uint16_t padding_bottom = 0;

    uint16_t margin_left = 0;
    uint16_t margin_top = 0;

    float horizontal_scroll_offset = 0.0f;
    float vertical_scroll_offset = 0.0f;

    overlay_element() = default;
    overlay_element(uint16_t _w, uint16_t _h)
        : w(_w)
        , h(_h) {}
    virtual ~overlay_element() = default;

    virtual void refresh();
    virtual bool is_compiled() { return m_is_compiled; }
    virtual void translate(int16_t _x, int16_t _y);
    virtual void scale(float _x, float _y, bool origin_scaling);
    virtual void set_pos(int16_t _x, int16_t _y);
    virtual void set_size(uint16_t _w, uint16_t _h);
    virtual void set_padding(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom);
    virtual void set_padding(uint16_t padding);
    virtual void set_margin(uint16_t left, uint16_t top);
    virtual void set_margin(uint16_t margin);
    virtual void set_text(std::string_view text);
    virtual void set_unicode_text(std::u32string_view text);
    virtual void set_font(const char *font_name, uint16_t font_size, bool bold = false);
    virtual void align_text(text_align align);
    virtual void set_wrap_text(bool state);
    virtual font *get_font() const;
    virtual std::vector<vertex> render_text(const char32_t *string, float x, float y);
    virtual compiled_resource &get_compiled();
    void measure_text(uint16_t &width, uint16_t &height, bool ignore_word_wrap = false) const;
    virtual void set_selected(bool selected) { static_cast<void>(selected); }
    virtual void set_visible(bool visible) {
        this->visible = visible;
        m_is_compiled = false;
    }
    virtual bool is_visible() const { return visible; }

    uint16_t compute_vertically_centered(uint16_t element_height);
    uint16_t compute_horizontally_centered(uint16_t element_width);

    uint16_t compute_vertically_centered(const overlay_element *other) { return compute_vertically_centered(other->h); }
    uint16_t compute_horizontally_centered(const overlay_element *other) { return compute_horizontally_centered(other->w); }

protected:
    bool m_is_compiled = false;

    void configure_sdf(compiled_resource::command_config &config, sdf_function func);
};

} // namespace overlay
