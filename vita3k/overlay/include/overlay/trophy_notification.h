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

#include <overlay/animation.h>
#include <overlay/controls.h>
#include <overlay/overlay.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace overlay {

struct trophy_notification : public overlay {
    trophy_notification();

    void enqueue(const std::string &name, const std::vector<uint8_t> &icon_buf,
        const std::vector<uint8_t> &grade_icon_buf);

    void update(uint64_t timestamp_us) override;
    compiled_resource get_compiled() override;

private:
    struct trophy_data {
        std::string name;
        std::vector<uint8_t> icon_buf;
        std::vector<uint8_t> grade_icon_buf;
    };

    void show_next();

    rounded_rect m_background;
    image_view m_icon;
    image_view m_grade_icon;
    label m_name_label;
    label m_detail_label;
    std::unique_ptr<image_info> m_icon_info;
    std::unique_ptr<image_info> m_grade_icon_info;

    animation_scale m_scale_anim;
    std::queue<trophy_data> m_queue;
    std::mutex m_queue_mutex;

    uint64_t m_creation_time_us = 0;
    bool m_showing = false;
    bool m_has_grade_icon = false;

    static constexpr uint16_t k_frame_w = 384;
    static constexpr uint16_t k_frame_h = 60;
    static constexpr uint16_t k_icon_size = 48;
    static constexpr uint16_t k_grade_icon_size = 16;
    static constexpr uint16_t k_margin = 12;
    static constexpr uint16_t k_top_margin = 20;
    static constexpr uint16_t k_font_size = 14;
    static constexpr uint64_t k_hold_duration_us = 4'000'000;
    static constexpr float k_slide_duration_sec = 0.22f;
};

} // namespace overlay
