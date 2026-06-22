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

#include <chrono>
#include <string>

namespace overlay {

struct shader_compile_notice : public overlay {
    shader_compile_notice();

    void update_count(uint32_t total_compiled, bool is_vulkan);
    bool should_hide() const;

    compiled_resource get_compiled() override;

private:
    label m_label;
    rounded_rect m_background;
    uint32_t m_compiled_count = 0;
    std::chrono::steady_clock::time_point m_last_update_time;

    static constexpr uint16_t k_pad_x = 10;
    static constexpr uint16_t k_pad_y_top = 2;
    static constexpr uint16_t k_pad_y_bot = 10;
    static constexpr uint16_t k_margin = 8;
    static constexpr uint16_t k_font_size = 13;
    static constexpr auto k_hide_delay = std::chrono::seconds(3);
};

} // namespace overlay
