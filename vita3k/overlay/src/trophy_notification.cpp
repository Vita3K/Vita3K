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
#include <overlay/trophy_notification.h>
#include <overlay/types.h>

namespace overlay {

trophy_notification::trophy_notification() {
    visible = false;

    m_background.back_color = { 0.96f, 0.96f, 0.96f, 1.0f };
    m_background.border_color = { 0.96f, 0.96f, 0.96f, 1.0f };
    m_background.border_radius = 14;

    m_icon.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_icon.set_size(k_icon_size, k_icon_size);

    m_grade_icon.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_grade_icon.set_size(k_grade_icon_size, k_grade_icon_size);

    m_name_label.fore_color = { 0.133f, 0.133f, 0.133f, 1.f };
    m_name_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_name_label.set_font(default_font_name, k_font_size);

    m_detail_label.fore_color = { 0.333f, 0.333f, 0.333f, 1.f };
    m_detail_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_detail_label.set_font(default_font_name, k_font_size - 2);

    m_scale_anim.duration_sec = k_slide_duration_sec;
    m_scale_anim.type = animation_type::ease_out_back;
}

void trophy_notification::enqueue(const std::string &name, const std::vector<uint8_t> &icon_buf,
    const std::vector<uint8_t> &grade_icon_buf) {
    std::lock_guard lock(m_queue_mutex);
    m_queue.push({ name, icon_buf, grade_icon_buf });

    if (!m_showing) {
        show_next();
    }
}

void trophy_notification::show_next() {
    // Caller must hold m_queue_mutex
    if (m_queue.empty()) {
        m_showing = false;
        visible = false;
        return;
    }

    auto data = std::move(m_queue.front());
    m_queue.pop();

    m_name_label.set_text(data.name);
    m_name_label.auto_resize();

    m_detail_label.set_text(lang::get(lang::str::trophy_earned));
    m_detail_label.auto_resize();

    if (!data.icon_buf.empty()) {
        m_icon_info = std::make_unique<image_info>(data.icon_buf);
        m_icon.set_raw_image(m_icon_info.get());
    } else {
        m_icon_info.reset();
        m_icon.clear_image();
    }

    m_has_grade_icon = false;
    if (!data.grade_icon_buf.empty()) {
        m_grade_icon_info = std::make_unique<image_info>(data.grade_icon_buf);
        if (m_grade_icon_info->w > 0) {
            m_grade_icon.set_raw_image(m_grade_icon_info.get());
            m_has_grade_icon = true;
        }
    }
    if (!m_has_grade_icon) {
        m_grade_icon_info.reset();
        m_grade_icon.clear_image();
    }

    const int16_t frame_x = static_cast<int16_t>(virtual_width - k_frame_w - k_margin);
    const int16_t frame_y = static_cast<int16_t>(k_top_margin);

    m_background.set_size(k_frame_w, k_frame_h);
    m_background.set_pos(frame_x, frame_y);

    const int16_t icon_x = static_cast<int16_t>(frame_x + k_margin);
    const int16_t icon_y = static_cast<int16_t>(frame_y + (k_frame_h - k_icon_size) / 2);
    m_icon.set_pos(icon_x, icon_y);

    int16_t text_x = static_cast<int16_t>(frame_x + k_margin + k_icon_size + k_margin);

    const uint16_t total_text_h = m_name_label.h + 2 + m_detail_label.h;
    const int16_t text_base_y = static_cast<int16_t>(frame_y + (k_frame_h - total_text_h) / 2);

    if (m_has_grade_icon) {
        const int16_t grade_y = static_cast<int16_t>(text_base_y + (m_name_label.h - k_grade_icon_size) / 2);
        m_grade_icon.set_pos(text_x, grade_y);
        text_x = static_cast<int16_t>(text_x + k_grade_icon_size + 4);
    }

    m_name_label.set_pos(text_x, text_base_y);
    m_detail_label.set_pos(
        static_cast<int16_t>(frame_x + k_margin + k_icon_size + k_margin),
        static_cast<int16_t>(text_base_y + m_name_label.h + 2));

    m_scale_anim.pivot_x = static_cast<float>(frame_x + k_frame_w);
    m_scale_anim.pivot_y = static_cast<float>(frame_y);
    m_scale_anim.current = 0.f;
    m_scale_anim.end = 1.f;
    m_scale_anim.type = animation_type::ease_out_back;
    m_scale_anim.on_finish = {};
    m_scale_anim.active = true;

    m_creation_time_us = 0;
    m_showing = true;
    visible = true;
}

void trophy_notification::update(uint64_t timestamp_us) {
    if (!m_showing)
        return;

    if (!m_creation_time_us) {
        m_creation_time_us = timestamp_us;
        return;
    }

    const uint64_t elapsed = timestamp_us - m_creation_time_us;

    if (elapsed > k_hold_duration_us) {
        if (!m_scale_anim.active || m_scale_anim.end == 1.f) {
            m_scale_anim.current = 1.f;
            m_scale_anim.end = 0.f;
            m_scale_anim.type = animation_type::ease_in_quad;
            m_scale_anim.on_finish = [this]() {
                std::lock_guard lock(m_queue_mutex);
                show_next();
            };
            m_scale_anim.active = true;
        }
    }

    if (m_scale_anim.active)
        m_scale_anim.update(timestamp_us);
}

compiled_resource trophy_notification::get_compiled() {
    if (!m_showing || !m_creation_time_us || !visible)
        return {};

    auto result = m_background.get_compiled();
    result.add(m_icon.get_compiled());
    if (m_has_grade_icon) {
        result.add(m_grade_icon.get_compiled());
    }
    result.add(m_name_label.get_compiled());
    result.add(m_detail_label.get_compiled());

    m_scale_anim.apply(result);

    return result;
}

} // namespace overlay