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

#pragma once

#include <overlay/controls.h>
#include <overlay/overlay.h>

#include <memory>
#include <string>

namespace overlay {

struct shader_precompile_progress : public overlay {
    shader_precompile_progress();

    void set_progress(int done, int total);
    void set_background_only();
    void set_background_image(std::unique_ptr<image_info> img);

    compiled_resource get_compiled() override;

private:
    image_view m_bg_image;
    rounded_rect m_bg_dimmer;

    rounded_rect m_card;

    label m_title_label;

    label m_count_label;

    rounded_rect m_bar_track;
    rounded_rect m_bar_fill;

    label m_pct_label;

    std::unique_ptr<image_info> m_bg_data;
    bool m_background_only = false;

    int m_done = 0;
    int m_total = 0;

    void layout();

    static constexpr uint16_t k_card_h = 80;
    static constexpr uint16_t k_card_radius = 8;
    static constexpr uint16_t k_text_margin = 16;
    static constexpr uint16_t k_title_font = 13;
    static constexpr uint16_t k_count_font = 12;
    static constexpr uint16_t k_pct_font = 11;
    static constexpr uint16_t k_bar_h = 4;
};

} // namespace overlay
