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
#include <overlay/pause_overlay.h>
#include <overlay/types.h>

namespace overlay {

pause_overlay::pause_overlay() {
    visible = true;

    m_bg_dimmer.set_pos(0, 0);
    m_bg_dimmer.set_size(virtual_width, virtual_height);
    m_bg_dimmer.back_color = { 0.f, 0.f, 0.f, 0.5f };
    m_bg_dimmer.border_radius = 0;

    m_card.back_color = { 0.059f, 0.059f, 0.059f, 0.667f };
    m_card.border_radius = k_card_radius;

    m_title_label.fore_color = { 0.863f, 0.863f, 0.863f, 1.f };
    m_title_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_title_label.set_font(default_font_name, 16);
    m_title_label.set_text(lang::get(lang::str::emulation_paused));
    m_title_label.align_text(overlay_element::center);

    m_subtitle_label.fore_color = { 0.627f, 0.627f, 0.627f, 1.f };
    m_subtitle_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_subtitle_label.set_font(default_font_name, 12);
    m_subtitle_label.set_text(lang::get(lang::str::press_ps_to_continue));
    m_subtitle_label.align_text(overlay_element::center);

    layout();
}

void pause_overlay::layout() {
    const int16_t card_x = static_cast<int16_t>((virtual_width - k_card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((virtual_height - k_card_h) / 2);

    m_card.set_pos(card_x, card_y);
    m_card.set_size(k_card_w, k_card_h);

    m_title_label.set_pos(card_x, static_cast<int16_t>(card_y + 10));
    m_title_label.set_size(k_card_w, 26);

    m_subtitle_label.set_pos(card_x, static_cast<int16_t>(card_y + 38));
    m_subtitle_label.set_size(k_card_w, 22);
}

compiled_resource pause_overlay::get_compiled() {
    compiled_resource result;

    if (!visible)
        return result;

    result.add(m_bg_dimmer.get_compiled());
    result.add(m_card.get_compiled());
    result.add(m_title_label.get_compiled());
    result.add(m_subtitle_label.get_compiled());

    return result;
}

} // namespace overlay
