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

#include <overlay/element.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <tuple>

namespace overlay {

std::u32string utf8_to_u32string(std::string_view utf8) {
    std::u32string result;
    result.reserve(utf8.size());

    size_t i = 0;
    while (i < utf8.size()) {
        char32_t cp = 0;
        const auto c = static_cast<uint8_t>(utf8[i]);

        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c >> 5) == 0x06) {
            if (i + 1 >= utf8.size())
                break;
            cp = (static_cast<char32_t>(c & 0x1F) << 6)
                | static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
            i += 2;
        } else if ((c >> 4) == 0x0E) {
            if (i + 2 >= utf8.size())
                break;
            cp = (static_cast<char32_t>(c & 0x0F) << 12)
                | (static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6)
                | static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
            i += 3;
        } else if ((c >> 3) == 0x1E) {
            if (i + 3 >= utf8.size())
                break;
            cp = (static_cast<char32_t>(c & 0x07) << 18)
                | (static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 12)
                | (static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 2]) & 0x3F) << 6)
                | static_cast<char32_t>(static_cast<uint8_t>(utf8[i + 3]) & 0x3F);
            i += 4;
        } else {
            // Invalid byte, skip
            i += 1;
            continue;
        }

        result.push_back(cp);
    }

    return result;
}

void overlay_element::set_sinus_offset(float sinus_modifier) {
    if (sinus_modifier >= 0) {
        static constexpr float PI = 3.14159265f;
        const auto now = std::chrono::steady_clock::now();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        const float pulse_sinus_x = static_cast<float>(us / 1000) * pulse_speed_modifier;
        pulse_sinus_offset = std::fmod(pulse_sinus_x + sinus_modifier * PI, 2.0f * PI);
    }
}

void overlay_element::refresh() {
    m_is_compiled = false;
}

void overlay_element::translate(int16_t _x, int16_t _y) {
    x += _x;
    y += _y;
    m_is_compiled = false;
}

void overlay_element::scale(float _x, float _y, bool origin_scaling) {
    if (origin_scaling) {
        x = static_cast<int16_t>(_x * x);
        y = static_cast<int16_t>(_y * y);
    }

    w = static_cast<uint16_t>(_x * w);
    h = static_cast<uint16_t>(_y * h);
    m_is_compiled = false;
}

void overlay_element::set_pos(int16_t _x, int16_t _y) {
    x = _x;
    y = _y;
    m_is_compiled = false;
}

void overlay_element::set_size(uint16_t _w, uint16_t _h) {
    w = _w;
    h = _h;
    m_is_compiled = false;
}

void overlay_element::set_padding(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) {
    padding_left = left;
    padding_right = right;
    padding_top = top;
    padding_bottom = bottom;
    m_is_compiled = false;
}

void overlay_element::set_padding(uint16_t padding) {
    padding_left = padding_right = padding_top = padding_bottom = padding;
    m_is_compiled = false;
}

void overlay_element::set_margin(uint16_t left, uint16_t top) {
    margin_left = left;
    margin_top = top;
    m_is_compiled = false;
}

void overlay_element::set_margin(uint16_t margin) {
    margin_left = margin_top = margin;
    m_is_compiled = false;
}

void overlay_element::set_text(std::string_view text) {
    std::u32string new_text = utf8_to_u32string(text);
    const bool is_dirty = this->text != new_text;

    if (is_dirty) {
        this->text = std::move(new_text);
        m_is_compiled = false;
    }
}

void overlay_element::set_unicode_text(std::u32string_view text) {
    const bool is_dirty = this->text != text;

    if (is_dirty) {
        this->text = text;
        m_is_compiled = false;
    }
}

void overlay_element::set_font(const char *font_name, uint16_t font_size, bool bold) {
    font_ref = fontmgr::get(font_name, font_size, bold);
    m_is_compiled = false;
}

void overlay_element::align_text(text_align align) {
    alignment = align;
    m_is_compiled = false;
}

void overlay_element::set_wrap_text(bool state) {
    wrap_text = state;
    m_is_compiled = false;
}

font *overlay_element::get_font() const {
    return font_ref ? font_ref : fontmgr::get(default_font_name, 12);
}

std::vector<vertex> overlay_element::render_text(const char32_t *string, float x, float y) {
    auto renderer = get_font();

    const uint16_t clip_width = clip_text ? w : UINT16_MAX;
    std::vector<vertex> result = renderer->render_text(string, clip_width, wrap_text);

    if (!result.empty()) {
        const auto apply_transform = [&]() {
            const float size_px = renderer->get_size_px();

            for (vertex &v : result) {
                v.x() += x + padding_left;
                v.y() += y + padding_top + size_px;
            }
        };

        if (alignment == text_align::left) {
            apply_transform();
        } else {
            // Scan for lines and measure them
            std::vector<std::tuple<uint32_t, uint32_t, float>> lines;
            uint32_t line_begin = 0;
            uint32_t line_end = 0;
            uint32_t word_end = 0;
            uint32_t ctr = 0;
            float text_extents_w = w;

            for (const auto &c : text) {
                switch (c) {
                case '\r':
                    continue;
                case '\n':
                    lines.emplace_back(line_begin, std::min(word_end, line_end), text_extents_w);
                    word_end = line_end = line_begin = ctr;
                    text_extents_w = w;
                    continue;
                default:
                    ctr += 4;

                    if (c == ' ') {
                        if (line_end == line_begin) {
                            word_end = line_end = line_begin = ctr;
                        } else {
                            line_end = ctr;
                        }
                    } else {
                        word_end = line_end = ctr;
                        text_extents_w = std::max(result[ctr - 1].x(), text_extents_w);
                    }
                    continue;
                }
            }

            // Add final line
            lines.emplace_back(line_begin, std::min(word_end, line_end), std::max<float>(text_extents_w, static_cast<float>(w)));

            const float offset_extent = (alignment == text_align::center ? 0.5f : 1.0f);
            const float size_px = renderer->get_size_px() * 0.5f;

            apply_transform();

            const auto move_line = [&result, &offset_extent](uint32_t begin, uint32_t end, float max_region_w) {
                const float line_length = result[end - 1].x() - result[begin].x();

                if (line_length < max_region_w) {
                    const float offset = (max_region_w - line_length) * offset_extent;
                    for (auto n = begin; n < end; ++n) {
                        result[n].x() += offset;
                    }
                }
            };

            for (const auto &[begin, end, max_region_w] : lines) {
                if (begin >= end)
                    continue;

                if (std::fabs(result[end - 1].y() - result[begin + 3].y()) < size_px) {
                    move_line(begin, end, max_region_w);
                    continue;
                }

                for (uint32_t i_begin = begin, i_next = begin + 4;; i_next += 4) {
                    const bool is_last_glyph = i_next >= end;

                    if (is_last_glyph || (result[i_next - 1].y() - result[i_begin + 3].y() >= size_px)) {
                        const uint32_t i_end = i_next - (is_last_glyph ? 0 : 4);
                        move_line(i_begin, i_end, max_region_w);
                        i_begin = i_end;

                        if (is_last_glyph) {
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

void overlay_element::configure_sdf(compiled_resource::command_config &config, sdf_function func) {
    const float rx = static_cast<float>(x) + padding_left;
    const float rw = static_cast<float>(w) - (padding_left + padding_right);
    const float ry = static_cast<float>(y) + padding_top;
    const float rh = static_cast<float>(h) - (padding_top + padding_bottom);

    config.active_effect = compiled_resource::effect_type::sdf;
    config.effect.sdf = {};
    config.effect.sdf.func = func;
    config.effect.sdf.cx = rx + (rw / 2.f);
    config.effect.sdf.cy = ry + (rh / 2.f);
    config.effect.sdf.hx = rw / 2.f;
    config.effect.sdf.hy = rh / 2.f;
    config.effect.sdf.br = 0.f;
    config.effect.sdf.bw = border_size;
    config.effect.sdf.border_color = border_color;

    config.disable_vertex_snap = true;
}

compiled_resource &overlay_element::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    m_is_compiled = true;
    compiled_resources.clear();

    if (!is_visible()) {
        return compiled_resources;
    }

    compiled_resource compiled_resources_temp = {};
    auto &cmd_bg = compiled_resources_temp.append({});
    auto &config = cmd_bg.config;

    config.color = back_color;
    config.pulse_glow = pulse_effect_enabled;
    config.pulse_sinus_offset = pulse_sinus_offset;
    config.pulse_speed_modifier = pulse_speed_modifier;

    if (border_size != 0
        && border_color.a > 0.f
        && w > border_size
        && h > border_size) {
        configure_sdf(config, sdf_function::box);
    }

    auto &verts = compiled_resources_temp.draw_commands.front().verts;
    verts.resize(4);

    verts[0].vec4(x, y, 0.f, 0.f);
    verts[1].vec4(static_cast<float>(x + w), y, 1.f, 0.f);
    verts[2].vec4(x, static_cast<float>(y + h), 0.f, 1.f);
    verts[3].vec4(static_cast<float>(x + w), static_cast<float>(y + h), 1.f, 1.f);

    compiled_resources.add(std::move(compiled_resources_temp), margin_left, margin_top);

    if (!text.empty()) {
        compiled_resources_temp.clear();
        auto &cmd_text = compiled_resources_temp.append({});

        cmd_text.config.set_font(get_font());
        cmd_text.config.color = fore_color;
        cmd_text.verts = render_text(text.c_str(), static_cast<float>(x), static_cast<float>(y));

        if (!cmd_text.verts.empty())
            compiled_resources.add(std::move(compiled_resources_temp), margin_left - horizontal_scroll_offset, margin_top - vertical_scroll_offset);
    }

    return compiled_resources;
}

void overlay_element::measure_text(uint16_t &width, uint16_t &height, bool ignore_word_wrap) const {
    if (text.empty()) {
        width = height = 0;
        return;
    }

    auto renderer = get_font();

    float text_width = 0.f;
    float unused = 0.f;
    float max_w = 0.f;
    float last_word = 0.f;
    height = static_cast<uint16_t>(renderer->get_size_px());

    for (auto c : text) {
        if (c == '\n') {
            height += static_cast<uint16_t>(renderer->get_size_px() + 2);
            max_w = std::max(max_w, text_width);
            text_width = 0.f;
            last_word = 0.f;
            continue;
        }

        if (c == ' ') {
            last_word = text_width;
        }

        renderer->get_char(c, text_width, unused);

        if (!ignore_word_wrap && wrap_text && text_width >= w) {
            if ((text_width - last_word) < w) {
                max_w = std::max(max_w, last_word);
                text_width -= (last_word + renderer->get_em_size());
                height += static_cast<uint16_t>(renderer->get_size_px() + 2);
            }
        }
    }

    max_w = std::max(max_w, text_width);
    width = static_cast<uint16_t>(std::ceil(max_w));
}

uint16_t overlay_element::compute_vertically_centered(uint16_t element_height) {
    const uint16_t half_height = h / 2;
    const uint16_t element_half_height = element_height / 2;
    return std::max(half_height, element_half_height) - element_half_height;
}

uint16_t overlay_element::compute_horizontally_centered(uint16_t element_width) {
    const uint16_t half_width = w / 2;
    const uint16_t element_half_width = element_width / 2;
    return std::max(half_width, element_half_width) - element_half_width;
}

} // namespace overlay
