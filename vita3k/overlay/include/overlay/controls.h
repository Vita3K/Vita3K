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

#include <overlay/element.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace overlay {

struct image_info_base {
    int w = 0, h = 0, channels = 0;
    int bpp = 0;
    mutable bool dirty = false;

    image_info_base() = default;
    virtual ~image_info_base() = default;
    virtual const uint8_t *get_data() const = 0;
    virtual size_t size_bytes() const { return static_cast<size_t>(w * h * 4); }
};

struct image_info : public image_info_base {
private:
    uint8_t *data = nullptr;
    std::vector<uint8_t> data_grey;

public:
    using image_info_base::image_info_base;
    image_info(image_info &) = delete;
    image_info(const std::string &filename, bool grayscaled = false);
    image_info(const std::vector<uint8_t> &bytes, bool grayscaled = false);
    ~image_info() override;

    void load_data(const std::vector<uint8_t> &bytes, bool grayscaled = false);
    const uint8_t *get_data() const override { return channels == 4 ? data : data_grey.empty() ? nullptr
                                                                                               : data_grey.data(); }
};

struct resource_config {
    enum standard_image_resource : uint8_t {
        fade_top = 1,
        fade_bottom,
        select,
        start,
        cross,
        circle,
        triangle,
        square,
        L1,
        R1,
        L2,
        R2,
        save,
        new_entry
    };

    std::vector<std::unique_ptr<image_info>> texture_raw_data;

    resource_config() = default;

    void load_files();
    void free_resources();

    // Icons directory management (static_assets/icons/)
    static void set_icons_dir(const std::string &dir);
    static const std::string &get_icons_dir();

    static std::unique_ptr<image_info> load_icon(const std::string &path);
};

struct spacer : public overlay_element {
    using overlay_element::overlay_element;
    compiled_resource &get_compiled() override {
        m_is_compiled = true;
        return compiled_resources;
    }
};

struct rounded_rect : public overlay_element {
    uint16_t border_radius = 5;

    using overlay_element::overlay_element;
    compiled_resource &get_compiled() override;
};

struct ellipse : public rounded_rect {
    using rounded_rect::rounded_rect;
    compiled_resource &get_compiled() override;
};

struct image_view : public overlay_element {
protected:
    uint8_t image_resource_ref = image_resource_none;
    const void *external_ref = nullptr;
    uint8_t blur_strength = 0;

public:
    using overlay_element::overlay_element;

    compiled_resource &get_compiled() override;

    void set_image_resource(uint8_t resource_id);
    void set_raw_image(const image_info_base *raw_image);
    void clear_image();
    void set_blur_strength(uint8_t strength);
};

struct image_button : public image_view {
    uint16_t text_horizontal_offset = 25;
    uint16_t m_text_offset_x = 0;
    int16_t m_text_offset_y = 0;

    image_button();
    image_button(uint16_t _w, uint16_t _h);

    void set_text_vertical_adjust(int16_t offset);
    void set_size(uint16_t _w, uint16_t h) override;

    compiled_resource &get_compiled() override;
};

struct label : public overlay_element {
    label() = default;
    label(std::string_view text);

    bool auto_resize(bool grow_only = false, uint16_t limit_w = UINT16_MAX, uint16_t limit_h = UINT16_MAX);
};

struct graph : public overlay_element {
private:
    std::string m_title;
    std::vector<float> m_datapoints;
    uint32_t m_datapoint_count = 0;
    color4f m_color;
    float m_min = 0.f;
    float m_max = 0.f;
    float m_avg = 0.f;
    float m_1p = 0.f;
    float m_guide_interval = 0.f;
    label m_label;

    bool m_show_min_max = false;
    bool m_show_1p_avg = false;
    bool m_1p_sort_high = false;

public:
    graph();
    void set_pos(int16_t _x, int16_t _y) override;
    void set_size(uint16_t _w, uint16_t _h) override;
    void set_title(const char *title);
    void set_font(const char *font_name, uint16_t font_size, bool bold = false) override;
    void set_font_size(uint16_t font_size);
    void set_count(uint32_t datapoint_count);
    void set_color(color4f color);
    void set_guide_interval(float guide_interval);
    void set_labels_visible(bool show_min_max, bool show_1p_avg);
    void set_one_percent_sort_high(bool sort_1p_high);
    uint16_t get_height() const;
    uint32_t get_datapoint_count() const;
    void record_datapoint(float datapoint, bool update_metrics);
    void update();
    compiled_resource &get_compiled() override;
};

} // namespace overlay
