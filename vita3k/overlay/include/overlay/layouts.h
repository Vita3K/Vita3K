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

#pragma once

#include <overlay/controls.h>

#include <cassert>
#include <memory>
#include <vector>

namespace overlay {

struct layout_container : public overlay_element {
    std::vector<std::unique_ptr<overlay_element>> m_items;
    uint16_t advance_pos = 0;
    uint16_t pack_padding = 0;
    uint16_t scroll_offset_value = 0;
    bool auto_resize = true;

    virtual overlay_element *add_element(std::unique_ptr<overlay_element> &item, int offset = -1) = 0;

    template <typename T>
        requires std::is_base_of_v<overlay_element, T>
    T *add_element(std::unique_ptr<T> &ptr, int offset = -1) {
        auto *raw = dynamic_cast<overlay_element *>(ptr.release());
        assert(raw);
        std::unique_ptr<overlay_element> e{ raw };
        return static_cast<T *>(add_element(e, offset));
    }

    overlay_element *add_element() {
        auto ptr = std::make_unique<overlay_element>();
        return add_element(ptr);
    }

    virtual void clear_items();

    layout_container();

    void translate(int16_t _x, int16_t _y) override;
    void set_pos(int16_t _x, int16_t _y) override;

    bool is_compiled() override;

    compiled_resource &get_compiled() override;

    virtual uint16_t get_scroll_offset_px() = 0;
    void add_spacer(uint16_t size = 0);
};

struct vertical_layout : public layout_container {
    using layout_container::add_element;
    overlay_element *add_element(std::unique_ptr<overlay_element> &item, int offset = -1) override;
    compiled_resource &get_compiled() override;
    uint16_t get_scroll_offset_px() override;
};

struct horizontal_layout : public layout_container {
    using layout_container::add_element;
    overlay_element *add_element(std::unique_ptr<overlay_element> &item, int offset = -1) override;
    compiled_resource &get_compiled() override;
    uint16_t get_scroll_offset_px() override;
};

struct box_layout : public layout_container {
    using layout_container::add_element;
    overlay_element *add_element(std::unique_ptr<overlay_element> &item, int offset = -1) override;
    uint16_t get_scroll_offset_px() override { return 0; }
};

} // namespace overlay
