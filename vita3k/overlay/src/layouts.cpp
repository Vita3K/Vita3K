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

#include <overlay/layouts.h>

#include <util/log.h>

#include <algorithm>

namespace overlay {

layout_container::layout_container() {
    back_color.a = 0.f;
}

void layout_container::translate(int16_t _x, int16_t _y) {
    overlay_element::translate(_x, _y);

    for (auto &itm : m_items)
        itm->translate(_x, _y);
}

void layout_container::set_pos(int16_t _x, int16_t _y) {
    const int16_t dx = _x - x;
    const int16_t dy = _y - y;
    translate(dx, dy);
}

bool layout_container::is_compiled() {
    if (m_is_compiled && std::any_of(m_items.cbegin(), m_items.cend(), [](const auto &item) { return item && !item->is_compiled(); })) {
        m_is_compiled = false;
    }

    return m_is_compiled;
}

compiled_resource &layout_container::get_compiled() {
    if (!is_compiled()) {
        compiled_resource result = overlay_element::get_compiled();

        for (auto &itm : m_items)
            result.add(itm->get_compiled());

        compiled_resources = result;
    }

    return compiled_resources;
}

void layout_container::add_spacer(uint16_t size) {
    std::unique_ptr<overlay_element> spacer_element = std::make_unique<spacer>();
    spacer_element->set_size(size, size);
    add_element(spacer_element);
}

void layout_container::clear_items() {
    m_items.clear();
    advance_pos = 0;
    scroll_offset_value = 0;
}

overlay_element *vertical_layout::add_element(std::unique_ptr<overlay_element> &item, int offset) {
    if (auto_resize) {
        item->set_pos(item->x + x, h + pack_padding + y);
        h += item->h + pack_padding;
        w = std::max(w, item->w);
    } else {
        item->set_pos(item->x + x, advance_pos + pack_padding + y);
        advance_pos += item->h + pack_padding;
    }

    if (offset < 0) {
        m_items.push_back(std::move(item));
        return m_items.back().get();
    }

    overlay_element *result = item.get();
    m_items.insert(m_items.begin() + offset, std::move(item));
    return result;
}

compiled_resource &vertical_layout::get_compiled() {
    if (scroll_offset_value == 0 && auto_resize)
        return layout_container::get_compiled();

    if (!is_compiled()) {
        compiled_resource result = overlay_element::get_compiled();
        const float global_y_offset = static_cast<float>(-scroll_offset_value);

        for (auto &item : m_items) {
            if (!item) {
                LOG_ERROR("Found null item in overlay::vertical_layout");
                continue;
            }

            const int32_t item_y_limit = static_cast<int32_t>(item->y) + item->h - scroll_offset_value - y;
            const int32_t item_y_base = static_cast<int32_t>(item->y) - scroll_offset_value - y;

            if (item_y_base > h) {
                break;
            }

            if (item_y_limit < 0) {
                continue;
            }

            if (item_y_limit > h || item_y_base < 0) {
                const areaf clip_rect = areaf::from_rect(x, y, w, h);
                result.add(item->get_compiled(), 0.f, global_y_offset, clip_rect);
            } else {
                result.add(item->get_compiled(), 0.f, global_y_offset);
            }
        }

        compiled_resources = result;
    }

    return compiled_resources;
}

uint16_t vertical_layout::get_scroll_offset_px() {
    return scroll_offset_value;
}

overlay_element *horizontal_layout::add_element(std::unique_ptr<overlay_element> &item, int offset) {
    if (auto_resize) {
        item->set_pos(w + pack_padding + x, item->y + y);
        w += item->w + pack_padding;
        h = std::max(h, item->h);
    } else {
        item->set_pos(advance_pos + pack_padding + x, item->y + y);
        advance_pos += item->w + pack_padding;
    }

    if (offset < 0) {
        m_items.push_back(std::move(item));
        return m_items.back().get();
    }

    auto result = item.get();
    m_items.insert(m_items.begin() + offset, std::move(item));
    return result;
}

compiled_resource &horizontal_layout::get_compiled() {
    if (scroll_offset_value == 0 && auto_resize)
        return layout_container::get_compiled();

    if (!is_compiled()) {
        compiled_resource result = overlay_element::get_compiled();
        const float global_x_offset = static_cast<float>(-scroll_offset_value);

        for (auto &item : m_items) {
            if (!item) {
                LOG_ERROR("Found null item in overlay::horizontal_layout");
                continue;
            }

            const int32_t item_x_limit = static_cast<int32_t>(item->x) + item->w - scroll_offset_value - x;
            const int32_t item_x_base = static_cast<int32_t>(item->x) - scroll_offset_value - x;

            if (item_x_base > w) {
                break;
            }

            if (item_x_limit < 0) {
                continue;
            }

            if (item_x_limit > w || item_x_base < 0) {
                const areaf clip_rect = areaf::from_rect(x, y, w, h);
                result.add(item->get_compiled(), global_x_offset, 0.f, clip_rect);
            } else {
                result.add(item->get_compiled(), global_x_offset, 0.f);
            }
        }

        compiled_resources = result;
    }

    return compiled_resources;
}

uint16_t horizontal_layout::get_scroll_offset_px() {
    return scroll_offset_value;
}

overlay_element *box_layout::add_element(std::unique_ptr<overlay_element> &item, int offset) {
    if (offset < 0) {
        m_items.push_back(std::move(item));
        return m_items.back().get();
    }

    overlay_element *result = item.get();
    m_items.insert(m_items.begin() + offset, std::move(item));
    return result;
}

} // namespace overlay
