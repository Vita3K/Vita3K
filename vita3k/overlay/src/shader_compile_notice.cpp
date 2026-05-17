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

#include <overlay/shader_compile_notice.h>
#include <overlay/types.h>

#include <fmt/format.h>

namespace overlay {

shader_compile_notice::shader_compile_notice() {
    visible = true;
    m_last_update_time = std::chrono::steady_clock::now();

    m_background.back_color = { 0.f, 0.f, 0.f, 0.65f };
    m_background.border_radius = 4;
    m_background.set_pos(k_margin, static_cast<int16_t>(virtual_height - k_margin - k_pad_y_top - k_pad_y_bot - k_font_size));

    m_label.fore_color = { 1.f, 1.f, 1.f, 1.f };
    m_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_label.set_font(default_font_name, k_font_size);
}

void shader_compile_notice::update_count(uint32_t total_compiled, bool is_vulkan) {
    m_compiled_count = total_compiled;
    m_last_update_time = std::chrono::steady_clock::now();
    visible = true;

    const char *kind = is_vulkan ? "pipeline" : "shader";
    const char *plural = (total_compiled == 1) ? "" : "s";
    std::string text = fmt::format("{} {}{} compiled", total_compiled, kind, plural);
    m_label.set_text(text);
    m_label.auto_resize();

    m_background.set_size(
        m_label.w + 2 * k_pad_x,
        m_label.h + k_pad_y_top + k_pad_y_bot);

    m_background.set_pos(
        k_margin,
        static_cast<int16_t>(virtual_height - k_margin - m_background.h));

    m_label.set_pos(
        static_cast<int16_t>(m_background.x + k_pad_x),
        static_cast<int16_t>(m_background.y + k_pad_y_top));
}

bool shader_compile_notice::should_hide() const {
    const auto elapsed = std::chrono::steady_clock::now() - m_last_update_time;
    return elapsed >= k_hide_delay;
}

compiled_resource shader_compile_notice::get_compiled() {
    compiled_resource result;

    if (!visible)
        return result;

    auto &bg = m_background.get_compiled();
    result.add(bg);

    auto &lbl = m_label.get_compiled();
    result.add(lbl);

    return result;
}

} // namespace overlay
