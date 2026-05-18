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

#include <lang/state.h>
#include <overlay/font.h>
#include <overlay/shader_precompile_progress.h>
#include <overlay/types.h>

#include <fmt/format.h>

namespace overlay {

shader_precompile_progress::shader_precompile_progress() {
    visible = true;

    m_bg_dimmer.set_pos(0, 0);
    m_bg_dimmer.set_size(virtual_width, virtual_height);
    m_bg_dimmer.back_color = { 0.f, 0.f, 0.f, 1.f };
    m_bg_dimmer.border_radius = 0;

    m_bg_image.set_pos(0, 0);
    m_bg_image.set_size(virtual_width, virtual_height);

    m_card.back_color = { 0.059f, 0.059f, 0.059f, 0.784f }; // rgba(15,15,15,200)
    m_card.border_radius = k_card_radius;

    m_title_label.fore_color = { 0.863f, 0.863f, 0.863f, 1.f }; // rgb(220,220,220)
    m_title_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_title_label.set_font(default_font_name, k_title_font);
    m_title_label.align_text(overlay_element::center);

    m_count_label.fore_color = { 0.627f, 0.627f, 0.627f, 1.f }; // rgb(160,160,160)
    m_count_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_count_label.set_font(default_font_name, k_count_font);
    m_count_label.align_text(overlay_element::right);

    m_bar_track.back_color = { 0.235f, 0.235f, 0.235f, 1.f }; // rgb(60,60,60)
    m_bar_track.border_radius = 2;

    m_bar_fill.back_color = { 1.f, 1.f, 1.f, 0.863f }; // rgba(255,255,255,220)
    m_bar_fill.border_radius = 2;

    m_pct_label.fore_color = { 0.549f, 0.549f, 0.549f, 1.f }; // rgb(140,140,140)
    m_pct_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_pct_label.set_font(default_font_name, k_pct_font);
    m_pct_label.align_text(overlay_element::center);

    layout();
}

void shader_precompile_progress::set_progress(int done, int total) {
    m_done = done;
    m_total = total;

    m_title_label.set_text(lang::get(lang::str::compiling_shaders));
    m_title_label.auto_resize();

    if (total > 0) {
        m_count_label.set_text(fmt::format("{} / {}", done, total));
        m_count_label.auto_resize();

        const int pct = done * 100 / total;
        m_pct_label.set_text(fmt::format("{}%", pct));
        m_pct_label.auto_resize();
    }

    layout();
}

void shader_precompile_progress::set_background_only() {
    m_background_only = true;
    m_done = 0;
    m_total = 0;
}

void shader_precompile_progress::set_background_image(std::unique_ptr<image_info> img) {
    m_bg_data = std::move(img);
    if (m_bg_data) {
        m_bg_image.set_raw_image(m_bg_data.get());
        m_bg_dimmer.back_color = { 0.f, 0.f, 0.f, 0.f };
    } else {
        m_bg_image.clear_image();
        m_bg_dimmer.back_color = { 0.f, 0.f, 0.f, 1.f };
    }
}

void shader_precompile_progress::layout() {
    if (m_total <= 0)
        return;

    const uint16_t card_w = virtual_width * 5 / 8;
    const int16_t card_x = static_cast<int16_t>((virtual_width - card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((virtual_height - k_card_h) / 2);

    m_card.set_pos(card_x, card_y);
    m_card.set_size(card_w, k_card_h);

    const int16_t text_x = static_cast<int16_t>(card_x + k_text_margin);
    const int16_t text_y = static_cast<int16_t>(card_y + 12);
    const uint16_t text_area_w = card_w - k_text_margin * 2;
    m_title_label.set_pos(text_x, text_y);
    m_title_label.set_size(text_area_w, 22);

    const uint16_t count_w = m_count_label.w > 0 ? m_count_label.w : 60;
    const int16_t count_x = static_cast<int16_t>(card_x + card_w - k_text_margin - count_w);
    m_count_label.set_pos(count_x, text_y);
    m_count_label.set_size(count_w, 22);

    const int16_t bar_x = static_cast<int16_t>(card_x + k_text_margin);
    const int16_t bar_y = static_cast<int16_t>(card_y + 46);
    const uint16_t bar_w = text_area_w;

    m_bar_track.set_pos(bar_x, bar_y);
    m_bar_track.set_size(bar_w, k_bar_h);

    if (m_done > 0) {
        const uint16_t fill_w = static_cast<uint16_t>(bar_w * m_done / m_total);
        m_bar_fill.set_pos(bar_x, bar_y);
        m_bar_fill.set_size(fill_w > 0 ? fill_w : 1, k_bar_h);
    } else {
        m_bar_fill.set_pos(bar_x, bar_y);
        m_bar_fill.set_size(0, k_bar_h);
    }

    const int16_t pct_y = static_cast<int16_t>(bar_y + 10);
    const uint16_t pct_w = m_pct_label.w > 0 ? m_pct_label.w : 40;
    const int16_t pct_x = static_cast<int16_t>(bar_x + (bar_w - pct_w) / 2);
    m_pct_label.set_pos(pct_x, pct_y);
    m_pct_label.set_size(pct_w, 18);
}

compiled_resource shader_precompile_progress::get_compiled() {
    compiled_resource result;

    if (!visible)
        return result;

    if (m_bg_data) {
        result.add(m_bg_image.get_compiled());
    }

    if (m_background_only || m_total <= 0)
        return result;

    if (!m_bg_data) {
        result.add(m_bg_dimmer.get_compiled());
    }

    result.add(m_card.get_compiled());

    result.add(m_title_label.get_compiled());

    result.add(m_count_label.get_compiled());

    result.add(m_bar_track.get_compiled());
    if (m_done > 0)
        result.add(m_bar_fill.get_compiled());

    result.add(m_pct_label.get_compiled());

    return result;
}

} // namespace overlay
