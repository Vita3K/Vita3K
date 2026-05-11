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

#include <overlay/controls.h>

#include <util/fs.h>
#include <util/log.h>

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

namespace overlay {

image_info::image_info(const std::string &filename, bool grayscaled) {
    std::vector<uint8_t> bytes;
    if (!fs_utils::read_data(fs::path(filename), bytes)) {
        LOG_ERROR("Image resource file '{}' could not be opened", filename);
        return;
    }
    load_data(bytes, grayscaled);
}

image_info::image_info(const std::vector<uint8_t> &bytes, bool grayscaled) {
    load_data(bytes, grayscaled);
}

image_info::~image_info() {
    if (data)
        stbi_image_free(data);
}

void image_info::load_data(const std::vector<uint8_t> &bytes, bool grayscaled) {
    data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &bpp, grayscaled ? STBI_grey_alpha : STBI_rgb_alpha);
    channels = grayscaled ? 2 : 4;
    dirty = true;

    if (data && grayscaled) {
        data_grey.resize(4 * w * h);

        for (size_t i = 0, n = 0; i < data_grey.size(); i += 4, n += 2) {
            const uint8_t grey = data[n];
            const uint8_t alpha = data[n + 1];

            data_grey[i + 0] = grey;
            data_grey[i + 1] = grey;
            data_grey[i + 2] = grey;
            data_grey[i + 3] = alpha;
        }
    } else {
        data_grey.clear();
    }
}

static std::string s_icons_dir;

void resource_config::set_icons_dir(const std::string &dir) {
    s_icons_dir = dir;
}

const std::string &resource_config::get_icons_dir() {
    return s_icons_dir;
}

std::unique_ptr<image_info> resource_config::load_icon(const std::string &path) {
    auto info = std::make_unique<image_info>(path);
    if (!info->get_data()) {
        LOG_WARN("Overlay icon not found: {}", path);
        return nullptr;
    }
    return info;
}

void resource_config::load_files() {
    texture_raw_data.clear();

    const auto &icons = get_icons_dir();
    if (icons.empty())
        return;

    // Standard UI icon files (indexed by standard_image_resource enum, starting at 1)
    static constexpr const char *texture_resource_files[] = {
        "fade_top.png",
        "fade_bottom.png",
        "select.png",
        "start.png",
        "cross.png",
        "circle.png",
        "triangle.png",
        "square.png",
        "L1.png",
        "R1.png",
        "L2.png",
        "R2.png",
        "save.png",
        "new.png"
    };

    for (const auto &res : texture_resource_files) {
        texture_raw_data.push_back(load_icon(icons + res));
    }
}

void resource_config::free_resources() {
    texture_raw_data.clear();
}

compiled_resource &rounded_rect::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    compiled_resources.clear();

    if (!is_visible()) {
        m_is_compiled = true;
        return compiled_resources;
    }

    overlay_element::get_compiled();
    auto &config = compiled_resources.draw_commands.front().config;
    configure_sdf(config, sdf_function::rounded_box);
    config.effect.sdf.br = std::min({ static_cast<float>(border_radius), config.effect.sdf.hx, config.effect.sdf.hy });

    return compiled_resources;
}

compiled_resource &ellipse::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    compiled_resources.clear();

    if (!is_visible()) {
        m_is_compiled = true;
        return compiled_resources;
    }

    rounded_rect::get_compiled();
    auto &config = compiled_resources.draw_commands.front().config;
    configure_sdf(config, sdf_function::ellipse);

    return compiled_resources;
}

compiled_resource &image_view::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    compiled_resources.clear();

    if (!is_visible()) {
        m_is_compiled = true;
        return compiled_resources;
    }

    auto &result = overlay_element::get_compiled();
    auto &cmd_img = result.draw_commands.front();

    cmd_img.config.set_image_resource(image_resource_ref);
    cmd_img.config.color = fore_color;
    cmd_img.config.external_data_ref = external_ref;
    cmd_img.config.blur_strength = blur_strength;

    // Make padding work for images (treat them as the content instead of the 'background')
    auto &verts = cmd_img.verts;

    verts[0] += vertex(static_cast<float>(padding_left), static_cast<float>(padding_top), 0.f, 0.f);
    verts[1] += vertex(-static_cast<float>(padding_right), static_cast<float>(padding_top), 0.f, 0.f);
    verts[2] += vertex(static_cast<float>(padding_left), -static_cast<float>(padding_bottom), 0.f, 0.f);
    verts[3] += vertex(-static_cast<float>(padding_right), -static_cast<float>(padding_bottom), 0.f, 0.f);

    m_is_compiled = true;
    return compiled_resources;
}

void image_view::set_image_resource(uint8_t resource_id) {
    image_resource_ref = resource_id;
    external_ref = nullptr;
}

void image_view::set_raw_image(const image_info_base *raw_image) {
    image_resource_ref = image_resource_id::raw_image;
    external_ref = raw_image;
}

void image_view::clear_image() {
    image_resource_ref = image_resource_none;
    external_ref = nullptr;
}

void image_view::set_blur_strength(uint8_t strength) {
    blur_strength = strength;
}

image_button::image_button() {
    clip_text = false;
}

image_button::image_button(uint16_t _w, uint16_t _h) {
    clip_text = false;
    set_size(_w, _h);
}

void image_button::set_text_vertical_adjust(int16_t offset) {
    m_text_offset_y = offset;
}

void image_button::set_size(uint16_t /*_w*/, uint16_t h) {
    image_view::set_size(h, h);
    m_text_offset_x = (h / 2) + text_horizontal_offset;
}

compiled_resource &image_button::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    compiled_resources.clear();

    if (!is_visible()) {
        m_is_compiled = true;
        return compiled_resources;
    }

    auto &compiled = image_view::get_compiled();
    for (auto &cmd : compiled.draw_commands) {
        if (cmd.config.texture_ref == font_file) {
            // Text, translate geometry to the right
            for (auto &v : cmd.verts) {
                v.values[0] += m_text_offset_x;
                v.values[1] += m_text_offset_y;
            }
        }
    }

    m_is_compiled = true;
    return compiled_resources;
}

label::label(std::string_view text) {
    set_text(text);
}

bool label::auto_resize(bool grow_only, uint16_t limit_w, uint16_t limit_h) {
    uint16_t new_width, new_height;
    uint16_t old_width = w, old_height = h;
    measure_text(new_width, new_height, true);

    new_width += padding_left + padding_right;
    new_height += padding_top + padding_bottom;

    if (new_width > limit_w && wrap_text)
        measure_text(new_width, new_height, false);

    if (grow_only) {
        new_width = std::max(w, new_width);
        new_height = std::max(h, new_height);
    }

    w = std::min(new_width, limit_w);
    h = std::min(new_height, limit_h);

    bool size_changed = old_width != new_width || old_height != new_height;
    return size_changed;
}

graph::graph() {
    m_label.set_font("Arial", 8);
    m_label.alignment = text_align::center;
    m_label.fore_color = { 1.f, 1.f, 1.f, 1.f };
    m_label.back_color = { 0.f, 0.f, 0.f, .7f };

    back_color = { 0.f, 0.f, 0.f, 0.5f };
}

void graph::set_pos(int16_t _x, int16_t _y) {
    m_label.set_pos(_x, _y);
    overlay_element::set_pos(_x, _y + m_label.h);
}

void graph::set_size(uint16_t _w, uint16_t _h) {
    m_label.set_size(_w, m_label.h);
    overlay_element::set_size(_w, _h);
}

void graph::set_title(const char *title) {
    m_title = title;
}

void graph::set_font(const char *font_name, uint16_t font_size, bool bold) {
    m_label.set_font(font_name, font_size, bold);
}

void graph::set_font_size(uint16_t font_size) {
    const auto *current_font = m_label.get_font();
    m_label.set_font(current_font->get_name().data(), font_size, current_font->is_bold());
}

void graph::set_count(uint32_t datapoint_count) {
    m_datapoint_count = datapoint_count;

    if (m_datapoints.empty()) {
        m_datapoints.resize(m_datapoint_count, -1.0f);
    } else if (m_datapoint_count < m_datapoints.size()) {
        std::copy(m_datapoints.begin() + m_datapoints.size() - m_datapoint_count, m_datapoints.end(), m_datapoints.begin());
        m_datapoints.resize(m_datapoint_count);
    } else {
        m_datapoints.insert(m_datapoints.begin(), m_datapoint_count - m_datapoints.size(), -1.0f);
    }
}

void graph::set_color(color4f color) {
    m_color = color;
}

void graph::set_guide_interval(float guide_interval) {
    m_guide_interval = guide_interval;
}

void graph::set_labels_visible(bool show_min_max, bool show_1p_avg) {
    m_show_min_max = show_min_max;
    m_show_1p_avg = show_1p_avg;
}

void graph::set_one_percent_sort_high(bool sort_1p_high) {
    m_1p_sort_high = sort_1p_high;
}

uint16_t graph::get_height() const {
    return h + m_label.h + m_label.padding_top + m_label.padding_bottom;
}

uint32_t graph::get_datapoint_count() const {
    return m_datapoint_count;
}

void graph::record_datapoint(float datapoint, bool update_metrics) {
    assert(datapoint >= 0.0f);

    m_datapoints.push_back(datapoint);

    // Cull vector when it gets large
    if (m_datapoints.size() > static_cast<size_t>(m_datapoint_count) * 16) {
        std::copy(m_datapoints.begin() + m_datapoints.size() - m_datapoint_count, m_datapoints.end(), m_datapoints.begin());
        m_datapoints.resize(m_datapoint_count);
    }

    if (!update_metrics) {
        return;
    }

    m_min = std::numeric_limits<float>::max();
    m_max = 0.0f;
    m_avg = 0.0f;
    m_1p = 0.0f;

    std::vector<float> valid_datapoints;
    size_t valid_count = 0;

    for (size_t i = m_datapoints.size() - m_datapoint_count; i < m_datapoints.size(); i++) {
        const float &dp = m_datapoints[i];

        if (dp < 0)
            continue;

        m_min = std::min(m_min, dp);
        m_max = std::max(m_max, dp);
        m_avg += dp;
        valid_count++;

        if (m_show_1p_avg) {
            valid_datapoints.push_back(dp);
        }
    }

    m_min = std::min(m_min, m_max);

    if (valid_count > 0) {
        m_avg /= static_cast<float>(valid_count);
    }

    if (m_show_1p_avg && !valid_datapoints.empty()) {
        const size_t i_1p = valid_datapoints.size() / 100;
        const size_t n_1p = i_1p + 1;

        if (m_1p_sort_high)
            std::nth_element(valid_datapoints.begin(), valid_datapoints.begin() + i_1p, valid_datapoints.end(), std::greater<float>());
        else
            std::nth_element(valid_datapoints.begin(), valid_datapoints.begin() + i_1p, valid_datapoints.end());

        m_1p = std::accumulate(valid_datapoints.begin(), valid_datapoints.begin() + n_1p, 0.0f) / static_cast<float>(n_1p);
    }
}

void graph::update() {
    std::string fps_info = m_title;

    if (m_show_1p_avg) {
        fps_info += fmt::format("\n1%:{:4.1f} av:{:4.1f}", m_1p, m_avg);
    }

    if (m_show_min_max) {
        fps_info += fmt::format("\nmn:{:4.1f} mx:{:4.1f}", m_min, m_max);
    }

    m_label.set_text(fps_info);
    m_label.set_padding(4, 4, 0, 4);

    m_label.auto_resize();
    m_label.refresh();

    set_size(std::max(m_label.w, w), h);
}

compiled_resource &graph::get_compiled() {
    if (is_compiled()) {
        return compiled_resources;
    }

    overlay_element::get_compiled();

    const float normalize_factor = static_cast<float>(h) / (m_max != 0.0f ? m_max : 1.0f);

    const bool guides_too_dense = (m_max / m_guide_interval) > (h / 3.0f);

    if (m_guide_interval > 0 && !guides_too_dense) {
        auto &cmd_guides = compiled_resources.append({});
        auto &config_guides = cmd_guides.config;

        config_guides.color = { 1.f, 1.f, 1.f, .2f };
        config_guides.primitives = primitive_type::line_list;

        auto &verts_guides = compiled_resources.draw_commands.back().verts;

        for (auto y_off = m_guide_interval; y_off < m_max; y_off += m_guide_interval) {
            const float guide_y = y + h - y_off * normalize_factor;
            verts_guides.emplace_back(static_cast<float>(x), guide_y);
            verts_guides.emplace_back(static_cast<float>(x + w), guide_y);
        }
    }

    auto &cmd_graph = compiled_resources.append({});
    auto &config_graph = cmd_graph.config;

    config_graph.color = m_color;
    config_graph.primitives = primitive_type::line_strip;

    auto &verts_graph = compiled_resources.draw_commands.back().verts;

    float x_stride = w;
    if (m_datapoint_count > 2) {
        x_stride /= (m_datapoint_count - 1);
    }

    if (m_datapoints.size() < m_datapoint_count)
        return compiled_resources;

    const size_t tail_index_offset = m_datapoints.size() - m_datapoint_count;

    for (uint32_t i = 0; i < m_datapoint_count; ++i) {
        const float x_line = x + i * x_stride;
        const float y_line = y + h - (std::max(0.0f, m_datapoints[i + tail_index_offset]) * normalize_factor);
        verts_graph.emplace_back(x_line, y_line);
    }

    compiled_resources.add(m_label.get_compiled());

    return compiled_resources;
}

} // namespace overlay
