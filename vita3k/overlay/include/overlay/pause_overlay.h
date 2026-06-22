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

namespace overlay {

struct pause_overlay : public overlay {
    pause_overlay();

    compiled_resource get_compiled() override;

private:
    rounded_rect m_bg_dimmer;
    rounded_rect m_card;
    label m_title_label;
    label m_subtitle_label;

    void layout();

    static constexpr uint16_t k_card_w = 280;
    static constexpr uint16_t k_card_h = 72;
    static constexpr uint16_t k_card_radius = 8;
};

} // namespace overlay
