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
#include <overlay/common_dialog.h>

#include <dialog/state.h>
#include <dialog/types.h>
#include <ime/state.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <util/system.h>

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fmt/format.h>

namespace overlay {

namespace {
constexpr const char *k_common_dialog_font = "ltn.pvf";

uint16_t measure_label_width(std::string_view text, uint16_t font_size, bool bold) {
    if (text.empty())
        return 0;

    overlay_element tmp;
    tmp.set_font(k_common_dialog_font, font_size, bold);
    tmp.set_text(text);
    tmp.set_size(UINT16_MAX, font_size + 4);

    uint16_t text_w = 0;
    uint16_t text_h = 0;
    tmp.measure_text(text_w, text_h, true);
    return text_w;
}

void submit_ime_dialog(DialogState &dialog) {
    if (dialog.active_ime) {
        std::lock_guard lock(dialog.active_ime->mutex);

        const size_t copy_len = std::min(static_cast<size_t>(dialog.active_ime->str.length()),
            static_cast<size_t>(dialog.ime.max_length));
        if (dialog.ime.result) {
            memcpy(dialog.ime.result, dialog.active_ime->str.c_str(), copy_len * sizeof(uint16_t));
            dialog.ime.result[copy_len] = 0;
        }

        const std::string utf8 = string_utils::utf16_to_utf8(dialog.active_ime->str);
        snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
        dialog.active_ime->event_id = SCE_IME_EVENT_PRESS_ENTER;
    }

    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
}

void cancel_ime_dialog(DialogState &dialog) {
    if (dialog.active_ime) {
        std::lock_guard lock(dialog.active_ime->mutex);
        dialog.active_ime->event_id = SCE_IME_EVENT_PRESS_CLOSE;
    }

    dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}
} // namespace

common_dialog_overlay::common_dialog_overlay() {
    m_dim_background.set_size(k_vw, k_vh);
    m_dim_background.back_color = k_color_bg_dim;

    m_card_background.border_radius = 8;

    m_fade_anim.duration_sec = 0.15f;
    m_fade_anim.current = { 0.f, 0.f, 0.f, 0.f };
    m_fade_anim.end = { 1.f, 1.f, 1.f, 1.f };
    m_fade_anim.active = true;

    visible = false;
}

std::unique_ptr<image_info> common_dialog_overlay::load_icon_white(const std::string &path) {
    std::string resolved_path = path;
    if (!path.empty() && fs::path(path).is_relative()) {
        const auto &icons_dir = resource_config::get_icons_dir();
        if (!icons_dir.empty())
            resolved_path = icons_dir + path;
    }

    auto img = std::make_unique<image_info>(resolved_path);
    if (!img->get_data() || img->w == 0 || img->h == 0)
        return img;

    // The image is loaded as RGBA. We repaint all pixels white, keeping alpha.
    auto *pixels = const_cast<uint8_t *>(img->get_data());
    const size_t pixel_count = static_cast<size_t>(img->w) * img->h;
    for (size_t i = 0; i < pixel_count; i++) {
        pixels[i * 4 + 0] = 255; // R
        pixels[i * 4 + 1] = 255; // G
        pixels[i * 4 + 2] = 255; // B
        // alpha unchanged
    }
    img->dirty = true;
    return img;
}

void common_dialog_overlay::ensure_icons_loaded() {
    if (m_icons_loaded)
        return;
    m_icons_loaded = true;
    m_cross_icon_data = load_icon_white("cross.png");
    m_info_icon_btn_data = load_icon_white("info.png");
    m_back_arrow_icon_data = load_icon_white("doublearrow.png");
    // Flip horizontally so the arrows point left (back)
    if (m_back_arrow_icon_data && m_back_arrow_icon_data->get_data()) {
        auto *pixels = const_cast<uint8_t *>(m_back_arrow_icon_data->get_data());
        const int w = m_back_arrow_icon_data->w;
        const int h = m_back_arrow_icon_data->h;
        for (int row = 0; row < h; row++) {
            for (int x = 0; x < w / 2; x++) {
                const int left = (row * w + x) * 4;
                const int right = (row * w + (w - 1 - x)) * 4;
                for (int c = 0; c < 4; c++)
                    std::swap(pixels[left + c], pixels[right + c]);
            }
        }
        m_back_arrow_icon_data->dirty = true;
    }
}

std::string common_dialog_overlay::format_date_time(int date_format, const void *sce_dt_raw) {
    const auto *dt = static_cast<const SceDateTime *>(sce_dt_raw);
    if (!dt)
        return {};

    std::string date_str;
    switch (date_format) {
    case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD:
        date_str = fmt::format("{}/{}/{}", dt->year, dt->month, dt->day);
        break;
    case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY:
        date_str = fmt::format("{}/{}/{}", dt->day, dt->month, dt->year);
        break;
    case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY:
        date_str = fmt::format("{}/{}/{}", dt->month, dt->day, dt->year);
        break;
    default:
        break;
    }
    return date_str + fmt::format(" {}:{:02d}", dt->hour, dt->minute);
}

bool common_dialog_overlay::poll_dialog(DialogState &dialog, int date_format, int sys_button) {
    m_dialog = &dialog;
    m_date_format = date_format;
    m_sys_button = sys_button;

    std::lock_guard<std::recursive_mutex> lock(dialog.mutex);

    if (m_dismissing) {
        if (!m_dismiss_fade_anim.active && !m_dismiss_scale_anim.active) {
            if (m_dismiss_is_substate) {
                // Sub-state dismiss finished — rebuild the new sub-state
                m_dismissing = false;
                m_dismiss_is_substate = false;
                clear_all();
                m_active_type = SAVEDATA_DIALOG;
                build_savedata_dialog(dialog, date_format);
            } else {
                clear_all();
                m_active_type = 0;
                m_dismissing = false;
                visible = false;
            }
        }
        return m_dismissing || visible; // keep active until dismiss completes
    }

    if (dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
        if (m_active_type != 0) {
            const bool is_card_dialog = (m_active_type == MESSAGE_DIALOG
                || (m_active_type == SAVEDATA_DIALOG && !m_savedata_list_mode && !m_savedata_info_mode));
            if (is_card_dialog && !m_dismissing) {
                m_dismissing = true;

                m_dismiss_fade_anim.reset(0);
                m_dismiss_fade_anim.current = { 1.f, 1.f, 1.f, 1.f };
                m_dismiss_fade_anim.end = { 0.f, 0.f, 0.f, 0.f };
                m_dismiss_fade_anim.duration_sec = 0.13f;
                m_dismiss_fade_anim.active = true;

                m_dismiss_scale_anim.reset(0);
                m_dismiss_scale_anim.current = 1.f;
                m_dismiss_scale_anim.end = 0.85f;
                m_dismiss_scale_anim.type = animation_type::ease_in_quad;
                m_dismiss_scale_anim.duration_sec = 0.13f;
                m_dismiss_scale_anim.pivot_x = static_cast<float>(k_vw) / 2.f;
                m_dismiss_scale_anim.pivot_y = static_cast<float>(k_vh) / 2.f;
                m_dismiss_scale_anim.active = true;

                return true; // keep active during dismiss
            }

            clear_all();
            m_active_type = 0;
            visible = false;
        }
        return false;
    }

    const int new_type = static_cast<int>(dialog.type);

    // Dialog type changed — rebuild
    if (m_active_type != new_type) {
        clear_all();
        m_active_type = new_type;

        switch (dialog.type) {
        case IME_DIALOG:
            build_ime_dialog(dialog);
            break;
        case MESSAGE_DIALOG:
            build_message_dialog(dialog);
            break;
        case TROPHY_SETUP_DIALOG:
            build_trophy_setup(dialog);
            break;
        case SAVEDATA_DIALOG:
            build_savedata_dialog(dialog, date_format);
            break;
        case NETCHECK_DIALOG:
            dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            return false;
        default:
            break;
        }

        m_fade_anim.current = { 0.f, 0.f, 0.f, 0.f };
        m_fade_anim.end = { 1.f, 1.f, 1.f, 1.f };
        m_fade_anim.active = true;

        m_scale_anim.reset(0);
        m_scale_anim.current = 0.85f;
        m_scale_anim.end = 1.f;
        m_scale_anim.type = animation_type::ease_out_back;
        m_scale_anim.duration_sec = 0.20f;
        m_scale_anim.pivot_x = static_cast<float>(k_vw) / 2.f;
        m_scale_anim.pivot_y = static_cast<float>(k_vh) / 2.f;
        m_scale_anim.active = true;

        visible = true;
    }

    if (m_active_type == SAVEDATA_DIALOG) {
        const auto &sd = dialog.savedata;
        if (sd.mode_to_display != m_savedata_mode_display
            || sd.draw_info_window != m_savedata_info_mode
            || sd.btn_num != m_savedata_btn_num) {
            const bool info_toggle_only = (sd.mode_to_display == m_savedata_mode_display
                && sd.btn_num == m_savedata_btn_num
                && sd.draw_info_window != m_savedata_info_mode
                && m_savedata_mode_display == SCE_SAVEDATA_DIALOG_MODE_LIST);

            if (info_toggle_only) {
                if (sd.draw_info_window) {
                    // Entering info view, just build info elements
                    m_buttons.clear();
                    m_focus_initialized = false;
                    m_selected_button = 0;
                    build_savedata_info(dialog, date_format);
                } else {
                    // Leaving info view, clear only info elements
                    m_savedata_info_mode = false;
                    m_savedata_list_mode = true;
                    m_info_icon.clear_image();
                    m_info_icon_data.reset();
                    m_buttons.clear();
                    m_selected_button = 0;
                    m_focus_initialized = false;
                    m_cancel_btn_focused = false;
                }
            } else {
                const bool was_card = (m_savedata_mode_display != SCE_SAVEDATA_DIALOG_MODE_LIST
                    && !m_savedata_info_mode);
                const bool entering_card = (sd.mode_to_display != SCE_SAVEDATA_DIALOG_MODE_LIST
                    && !sd.draw_info_window);
                const bool leaving_card = was_card && !entering_card;

                if (leaving_card && !m_dismissing) {
                    m_dismissing = true;
                    m_dismiss_is_substate = true;

                    m_dismiss_fade_anim.reset(0);
                    m_dismiss_fade_anim.current = { 1.f, 1.f, 1.f, 1.f };
                    m_dismiss_fade_anim.end = { 0.f, 0.f, 0.f, 0.f };
                    m_dismiss_fade_anim.duration_sec = 0.13f;
                    m_dismiss_fade_anim.active = true;

                    m_dismiss_scale_anim.reset(0);
                    m_dismiss_scale_anim.current = 1.f;
                    m_dismiss_scale_anim.end = 0.85f;
                    m_dismiss_scale_anim.type = animation_type::ease_in_quad;
                    m_dismiss_scale_anim.duration_sec = 0.13f;
                    m_dismiss_scale_anim.pivot_x = static_cast<float>(k_vw) / 2.f;
                    m_dismiss_scale_anim.pivot_y = static_cast<float>(k_vh) / 2.f;
                    m_dismiss_scale_anim.active = true;
                } else {
                    clear_all();
                    m_active_type = SAVEDATA_DIALOG;
                    build_savedata_dialog(dialog, date_format);

                    if (entering_card) {
                        m_fade_anim.current = { 0.f, 0.f, 0.f, 0.f };
                        m_fade_anim.end = { 1.f, 1.f, 1.f, 1.f };
                        m_fade_anim.active = true;

                        m_scale_anim.reset(0);
                        m_scale_anim.current = 0.85f;
                        m_scale_anim.end = 1.f;
                        m_scale_anim.type = animation_type::ease_out_back;
                        m_scale_anim.duration_sec = 0.20f;
                        m_scale_anim.pivot_x = static_cast<float>(k_vw) / 2.f;
                        m_scale_anim.pivot_y = static_cast<float>(k_vh) / 2.f;
                        m_scale_anim.active = true;
                    }
                }
            }
        }
    }

    update_progress(dialog, m_active_type == SAVEDATA_DIALOG);

    if (m_active_type == MESSAGE_DIALOG) {
        if (dialog.msg.message != m_last_message) {
            m_last_message = dialog.msg.message;
            m_message_label.set_text(m_last_message);
        }
    } else if (m_active_type == SAVEDATA_DIALOG && !m_savedata_list_mode) {
        if (dialog.savedata.msg != m_last_message) {
            m_last_message = dialog.savedata.msg;
            m_message_label.set_text(m_last_message);
        }
    }

    // Sync ime text, preedit and caret from the shared Ime struct.
    if (m_active_type == IME_DIALOG && dialog.active_ime) {
        auto &aime = *dialog.active_ime;
        std::lock_guard lock(aime.mutex);

        const std::string current_text = string_utils::utf16_to_utf8(aime.str);
        const uint32_t caret = std::min(aime.caretIndex, static_cast<uint32_t>(aime.str.size()));

        // Build preedit string
        std::u16string preedit_u16;
        if (aime.edit_text.preeditLength > 0) {
            const uint32_t pi = aime.edit_text.preeditIndex;
            const uint32_t pl = aime.edit_text.preeditLength;
            if (pi + pl <= aime.str.size())
                preedit_u16 = aime.str.substr(pi, pl);
        }
        const std::string current_preedit = preedit_u16.empty()
            ? std::string()
            : string_utils::utf16_to_utf8(preedit_u16);

        if (current_text != m_ime_last_text || current_preedit != m_ime_preedit) {
            m_ime_last_text = current_text;
            m_ime_preedit = current_preedit;
            m_ime_text = utf8_to_u32string(current_text);

            m_ime_text_label.set_text(current_text);
        }

        m_ime_cursor = std::min(caret, static_cast<uint32_t>(m_ime_text.size()));

        // Sync the UTF-8 text buffer back so dialog.ime.text stays current.
        snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", current_text.c_str());
    }

    // Trophy setup auto-completion
    if (m_active_type == TROPHY_SETUP_DIALOG) {
        const uint64_t now = SDL_GetTicks();
        if (now >= dialog.trophy.tick) {
            dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            clear_all();
            m_active_type = 0;
            visible = false;
            return false;
        }
    }

    return true;
}

bool common_dialog_overlay::is_confirm_button(pad_button button) const {
    if (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        return button == pad_button::circle;
    return button == pad_button::cross;
}

bool common_dialog_overlay::is_cancel_button(pad_button button) const {
    if (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        return button == pad_button::cross;
    return button == pad_button::circle;
}

void common_dialog_overlay::update(uint64_t timestamp_us) {
    if (m_fade_anim.active)
        m_fade_anim.update(timestamp_us);
    if (m_scale_anim.active)
        m_scale_anim.update(timestamp_us);
    if (m_dismiss_fade_anim.active)
        m_dismiss_fade_anim.update(timestamp_us);
    if (m_dismiss_scale_anim.active)
        m_dismiss_scale_anim.update(timestamp_us);

    m_focus_pulse_time += 0.032f;
    if (m_focus_pulse_time > 6.283f)
        m_focus_pulse_time -= 6.283f;

    if (m_focus_initialized) {
        const float dt = 0.032f;
        const float t = 1.f - std::exp(-k_focus_lerp_speed * dt);
        auto lerp = [](float a, float b, float f) { return a + (b - a) * f; };
        m_focus_current.x = lerp(m_focus_current.x, m_focus_target.x, t);
        m_focus_current.y = lerp(m_focus_current.y, m_focus_target.y, t);
        m_focus_current.w = lerp(m_focus_current.w, m_focus_target.w, t);
        m_focus_current.h = lerp(m_focus_current.h, m_focus_target.h, t);
        m_focus_current.radius = lerp(m_focus_current.radius, m_focus_target.radius, t);
    }
}

void common_dialog_overlay::clear_all() {
    m_buttons.clear();
    for (auto &slot : m_save_slots) {
        if (slot)
            slot->icon.clear_image();
    }
    m_save_slots.clear();
    m_has_progress_bar = false;
    m_last_bar_percent = 0;
    m_selected_button = 0;
    m_selected_slot = 0;
    m_scroll_offset = 0;
    m_savedata_list_mode = false;
    m_savedata_info_mode = false;
    m_savedata_mode_display = 0;
    m_savedata_display_type = 0;
    m_savedata_btn_num = 0;
    m_ime_active = false;
    m_ime_text.clear();
    m_ime_preedit.clear();
    m_ime_cursor = 0;
    m_ime_last_text.clear();
    m_last_message.clear();
    m_info_icon.clear_image();
    m_info_icon_data.reset();
    m_list_title.clear();
    m_cancel_btn_focused = false;
    m_dismissing = false;
    m_focus_initialized = false;
}

void common_dialog_overlay::set_focus_target(float x, float y, float w, float h, float radius) {
    m_focus_target = { x, y, w, h, radius };
    if (!m_focus_initialized) {
        m_focus_current = m_focus_target;
        m_focus_initialized = true;
    }
}

void common_dialog_overlay::draw_focus_ring(compiled_resource &res) {
    if (!m_focus_initialized)
        return;

    const auto &f = m_focus_current;
    if (f.w <= 0.f || f.h <= 0.f)
        return;

    const float pulse = 0.5f + 0.5f * std::sin(m_focus_pulse_time * 3.0f);
    const float a = 0.85f + 0.15f * pulse;

    rounded_rect ring;
    ring.set_pos(static_cast<int16_t>(std::round(f.x)), static_cast<int16_t>(std::round(f.y)));
    ring.set_size(static_cast<uint16_t>(std::round(f.w)), static_cast<uint16_t>(std::round(f.h)));
    ring.back_color = { k_color_focus_ring.r, k_color_focus_ring.g, k_color_focus_ring.b, 0.15f * a };
    ring.border_size = k_focus_ring_thickness;
    ring.border_color = { k_color_focus_ring.r, k_color_focus_ring.g, k_color_focus_ring.b, a };
    ring.border_radius = static_cast<uint16_t>(f.radius);
    res.add(ring.get_compiled());
}

void common_dialog_overlay::apply_gloss_overlay(compiled_resource &res,
    int16_t x, int16_t y, uint16_t w, uint16_t h) {
    compiled_resource gloss_res;
    auto &cmd = gloss_res.append({});
    cmd.config.color = { 1.f, 1.f, 1.f, 1.f };
    cmd.config.active_effect = compiled_resource::effect_type::gloss;
    cmd.config.effect.gloss = { k_gloss_height, k_gloss_feather, k_gloss_opacity };
    cmd.config.disable_vertex_snap = true;

    cmd.verts.resize(4);
    cmd.verts[0].vec4(static_cast<float>(x), static_cast<float>(y), 0.f, 0.f);
    cmd.verts[1].vec4(static_cast<float>(x + w), static_cast<float>(y), 1.f, 0.f);
    cmd.verts[2].vec4(static_cast<float>(x), static_cast<float>(y + h), 0.f, 1.f);
    cmd.verts[3].vec4(static_cast<float>(x + w), static_cast<float>(y + h), 1.f, 1.f);

    res.add(gloss_res);
}

void common_dialog_overlay::set_btn_gloss_on_compiled(compiled_resource &compiled,
    float gloss_height, float opacity, float bottom_opacity) {
    if (compiled.draw_commands.empty())
        return;
    auto &cfg = compiled.draw_commands.front().config;
    if (cfg.active_effect == compiled_resource::effect_type::sdf) {
        // store btn_gloss params co-resident
        cfg.effect.sdf.btn_gloss_height = gloss_height;
        cfg.effect.sdf.btn_gloss_opacity = opacity;
        cfg.effect.sdf.btn_gloss_bottom_opacity = bottom_opacity;
    } else {
        cfg.active_effect = compiled_resource::effect_type::btn_gloss;
        cfg.effect.btn_gloss = { gloss_height, 0.13f, opacity, 0.f, 1.f, bottom_opacity };
    }
}

void common_dialog_overlay::update_progress(DialogState &dialog, bool is_savedata) {
    uint32_t percent = is_savedata ? dialog.savedata.bar_percent : dialog.msg.bar_percent;
    bool has_bar = is_savedata ? dialog.savedata.has_progress_bar : dialog.msg.has_progress_bar;

    if (!has_bar || !m_has_progress_bar)
        return;

    if (percent != m_last_bar_percent) {
        m_last_bar_percent = percent;

        const uint16_t max_w = static_cast<uint16_t>((is_savedata ? k_sd_card_w : k_card_w) - k_margin * 4);
        const uint16_t fill_w = static_cast<uint16_t>(max_w * std::min(percent, 100u) / 100);
        m_progress_fill.set_size(fill_w, k_progress_h);

        m_progress_label.set_text(fmt::format("{}%", percent));
    }
}

void common_dialog_overlay::build_message_dialog(DialogState &dialog) {
    auto &msg = dialog.msg;
    m_last_message = msg.message;

    const int16_t card_x = static_cast<int16_t>((k_vw - k_card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((k_vh - k_card_h) / 2);

    m_card_background.set_pos(card_x, card_y);
    m_card_background.set_size(k_card_w, k_card_h);
    m_card_background.back_color = k_color_card_bg;

    m_message_label.set_text(msg.message);
    m_message_label.set_font(k_common_dialog_font, k_font_size, true);
    m_message_label.align_text(overlay_element::center);
    m_message_label.set_wrap_text(true);
    m_message_label.fore_color = k_color_text;
    m_message_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_message_label.set_pos(static_cast<int16_t>(card_x + k_margin * 2),
        static_cast<int16_t>(card_y + k_card_h / 2 - 40 + k_text_voffset));
    m_message_label.set_size(static_cast<uint16_t>(k_card_w - k_margin * 4),
        static_cast<uint16_t>(k_card_h / 3));

    m_has_progress_bar = msg.has_progress_bar;
    if (m_has_progress_bar) {
        const uint16_t bar_w = k_card_w - k_margin * 4;
        const int16_t bar_x = static_cast<int16_t>(card_x + k_margin * 2);
        const int16_t bar_y = static_cast<int16_t>(card_y + k_card_h * 2 / 3 - k_progress_h);

        m_progress_bg.set_pos(bar_x, bar_y);
        m_progress_bg.set_size(bar_w, k_progress_h);
        m_progress_bg.back_color = k_color_progress_bg;
        m_progress_bg.border_radius = 3;

        const uint16_t fill_w = static_cast<uint16_t>(bar_w * std::min(msg.bar_percent, 100u) / 100);
        m_progress_fill.set_pos(bar_x, bar_y);
        m_progress_fill.set_size(fill_w, k_progress_h);
        m_progress_fill.back_color = k_color_progress_fill;
        m_progress_fill.border_radius = 3;

        m_progress_label.set_text(fmt::format("{}%", msg.bar_percent));
        m_progress_label.set_font(k_common_dialog_font, k_font_size_small, true);
        m_progress_label.align_text(overlay_element::center);
        m_progress_label.fore_color = k_color_text;
        m_progress_label.back_color = { 0.f, 0.f, 0.f, 0.f };
        m_progress_label.set_pos(bar_x, static_cast<int16_t>(bar_y + k_progress_h + 4));
        m_progress_label.set_size(bar_w, 20);

        m_last_bar_percent = msg.bar_percent;
    }

    m_buttons.clear();
    m_selected_button = 0;
    if (msg.btn_num > 0) {
        const uint16_t total_btn_w = static_cast<uint16_t>(msg.btn_num * k_btn_w + (msg.btn_num - 1) * k_margin);
        int16_t btn_x = static_cast<int16_t>(card_x + (k_card_w - total_btn_w) / 2);
        const int16_t btn_y = static_cast<int16_t>(card_y + k_card_h - 50);

        for (uint8_t i = 0; i < msg.btn_num; i++) {
            button_entry btn;
            btn.bg.set_pos(btn_x, btn_y);
            btn.bg.set_size(k_btn_w, k_btn_h);
            btn.bg.back_color = k_color_btn_normal;
            btn.bg.border_color = { 0.f, 0.f, 0.f, 0.f };
            btn.bg.border_radius = k_btn_radius;

            btn.text.set_text(msg.btn[i]);
            btn.text.set_font(k_common_dialog_font, k_font_size, true);
            btn.text.align_text(overlay_element::center);
            btn.text.fore_color = k_color_text;
            btn.text.back_color = { 0.f, 0.f, 0.f, 0.f };
            btn.text.set_pos(btn_x, static_cast<int16_t>(btn_y + (k_btn_h - k_font_size) / 2 + k_text_voffset));
            btn.text.set_size(k_btn_w, k_font_size + 4);

            btn.value = msg.btn_val[i];
            m_buttons.push_back(std::move(btn));

            btn_x = static_cast<int16_t>(btn_x + k_btn_w + k_margin);
        }
    }
}

void common_dialog_overlay::build_savedata_dialog(DialogState &dialog, int date_format) {
    auto &sd = dialog.savedata;
    m_savedata_mode_display = sd.mode_to_display;
    m_savedata_info_mode = sd.draw_info_window;
    m_savedata_display_type = sd.display_type;
    m_savedata_btn_num = sd.btn_num;

    if (sd.draw_info_window) {
        build_savedata_info(dialog, date_format);
    } else if (sd.mode_to_display == SCE_SAVEDATA_DIALOG_MODE_LIST) {
        build_savedata_list(dialog, date_format);
    } else {
        build_savedata_fixed(dialog, date_format);
    }
}

void common_dialog_overlay::build_savedata_list(DialogState &dialog, int date_format) {
    auto &sd = dialog.savedata;
    m_savedata_list_mode = true;
    m_list_title = sd.list_title;

    m_save_slots.clear();
    m_selected_slot = 0;
    m_scroll_offset = 0;

    for (uint32_t i = 0; i < sd.slot_list_size; i++) {
        const bool has_data = (sd.slot_info.size() > i && sd.slot_info[i].isExist == 1);

        if (sd.title.size() <= i || sd.title[i].empty())
            continue;

        auto entry = std::make_unique<save_slot_entry>();
        entry->slot_index = i;

        if (sd.title.size() > i && !sd.title[i].empty()) {
            entry->title.set_text(sd.title[i]);
        } else {
            entry->title.set_text("---");
        }
        entry->title.set_font(k_common_dialog_font, k_font_size, true);
        entry->title.fore_color = k_color_text;
        entry->title.back_color = { 0.f, 0.f, 0.f, 0.f };

        const bool has_date = (sd.has_date.size() > i && sd.has_date[i] && sd.date.size() > i);
        const bool has_sub = (sd.subtitle.size() > i && !sd.subtitle[i].empty());

        std::string date_text;
        if (has_date)
            date_text = format_date_time(date_format, &sd.date[i]);
        entry->date_label.set_text(date_text);
        entry->date_label.set_font(k_common_dialog_font, k_font_size_small, true);
        entry->date_label.fore_color = k_color_text_dim;
        entry->date_label.back_color = { 0.f, 0.f, 0.f, 0.f };

        std::string sub_text;
        if (has_sub)
            sub_text = sd.subtitle[i];
        entry->subtitle.set_text(sub_text);
        entry->subtitle.set_font(k_common_dialog_font, k_font_size_small, true);
        entry->subtitle.fore_color = k_color_text_dim;
        entry->subtitle.back_color = { 0.f, 0.f, 0.f, 0.f };

        if (sd.icon_buffer.size() > i && !sd.icon_buffer[i].empty()) {
            entry->icon_info = std::make_unique<image_info>(sd.icon_buffer[i]);
            entry->icon.set_raw_image(entry->icon_info.get());
        }
        entry->icon.set_size(k_slot_icon_w, k_slot_icon_h);

        entry->has_data = has_data;

        entry->bg.set_size(k_vw, k_slot_h);
        entry->bg.back_color = { 0.f, 0.f, 0.f, 0.f };
        entry->bg.border_radius = 0;

        m_save_slots.push_back(std::move(entry));
    }
}

void common_dialog_overlay::build_savedata_fixed(DialogState &dialog, int date_format) {
    auto &sd = dialog.savedata;
    m_savedata_list_mode = false;
    m_last_message = sd.msg;

    const auto sel = sd.selected_save;

    if (sel >= sd.title.size() && sel >= sd.icon_buffer.size())
        return;

    const int16_t card_x = static_cast<int16_t>((k_vw - k_sd_card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((k_vh - k_sd_card_h) / 2);

    m_card_background.set_pos(card_x, card_y);
    m_card_background.set_size(k_sd_card_w, k_sd_card_h);
    m_card_background.back_color = k_color_card_bg;
    m_card_background.border_radius = 12;

    const int16_t icon_x = static_cast<int16_t>(card_x + 48);
    const int16_t icon_y = static_cast<int16_t>(card_y + 34);
    if (sd.icon_buffer.size() > sel && !sd.icon_buffer[sel].empty()) {
        m_info_icon_data = std::make_unique<image_info>(sd.icon_buffer[sel]);
        m_info_icon.set_raw_image(m_info_icon_data.get());
    }
    m_info_icon.set_size(k_slot_icon_w, k_slot_icon_h);
    m_info_icon.set_pos(icon_x, icon_y);

    const int16_t text_left = static_cast<int16_t>(icon_x + k_slot_icon_w + 20);
    const uint16_t text_w = static_cast<uint16_t>(k_sd_card_w - (text_left - card_x) - k_margin * 2);

    if (sd.title.size() > sel && !sd.title[sel].empty()) {
        m_info_title.set_text(sd.title[sel]);
    }
    m_info_title.set_font(k_common_dialog_font, k_font_size, true);
    m_info_title.fore_color = k_color_text;
    m_info_title.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_title.set_pos(text_left,
        static_cast<int16_t>(icon_y + 10 + k_text_voffset));
    m_info_title.set_size(text_w, k_font_size + 4);

    if (sd.has_date.size() > sel && sd.has_date[sel] && sd.date.size() > sel) {
        m_info_date.set_text(format_date_time(date_format, &sd.date[sel]));
    }
    m_info_date.set_font(k_common_dialog_font, k_font_size_small, true);
    m_info_date.fore_color = k_color_text_dim;
    m_info_date.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_date.set_pos(text_left,
        static_cast<int16_t>(icon_y + 10 + k_font_size + 6 + k_text_voffset));
    m_info_date.set_size(text_w, k_font_size_small + 4);

    if (sd.subtitle.size() > sel && !sd.subtitle[sel].empty()) {
        m_info_subtitle.set_text(sd.subtitle[sel]);
    } else {
        m_info_subtitle.set_text("");
    }
    m_info_subtitle.set_font(k_common_dialog_font, k_font_size_small, true);
    m_info_subtitle.fore_color = k_color_text_dim;
    m_info_subtitle.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_subtitle.set_pos(text_left,
        static_cast<int16_t>(icon_y + 10 + k_font_size + 6 + k_font_size_small + 6 + k_text_voffset));
    m_info_subtitle.set_size(text_w, k_font_size_small + 4);

    {
        const int16_t sep_bottom = static_cast<int16_t>(icon_y + k_slot_icon_h + 10 + 1);
        const int16_t btn_top = static_cast<int16_t>(card_y + k_sd_card_h - k_sd_btn_h - 22);
        const uint16_t msg_h = static_cast<uint16_t>(k_sd_card_h / 3);
        const int16_t avail = static_cast<int16_t>(btn_top - sep_bottom);
        const int16_t msg_y = static_cast<int16_t>(sep_bottom + (avail - msg_h) / 2 + k_text_voffset);

        m_message_label.set_text(sd.msg);
        m_message_label.set_font(k_common_dialog_font, k_font_size, true);
        m_message_label.align_text(overlay_element::center);
        m_message_label.set_wrap_text(true);
        m_message_label.fore_color = k_color_text;
        m_message_label.back_color = { 0.f, 0.f, 0.f, 0.f };
        m_message_label.set_pos(static_cast<int16_t>(card_x + 50), msg_y);
        m_message_label.set_size(static_cast<uint16_t>(k_sd_card_w - 100), msg_h);
    }

    m_has_progress_bar = sd.has_progress_bar;
    if (m_has_progress_bar) {
        const uint16_t bar_w = static_cast<uint16_t>(k_sd_card_w - k_margin * 4);
        const int16_t bar_x = static_cast<int16_t>(card_x + k_margin * 2);
        const int16_t bar_y = static_cast<int16_t>(card_y + k_sd_card_h - k_sd_btn_h - 22 - k_progress_h - 24);

        m_progress_bg.set_pos(bar_x, bar_y);
        m_progress_bg.set_size(bar_w, k_progress_h);
        m_progress_bg.back_color = k_color_progress_bg;
        m_progress_bg.border_radius = 3;

        const uint16_t fill_w = static_cast<uint16_t>(bar_w * std::min(sd.bar_percent, 100u) / 100);
        m_progress_fill.set_pos(bar_x, bar_y);
        m_progress_fill.set_size(fill_w, k_progress_h);
        m_progress_fill.back_color = k_color_progress_fill;
        m_progress_fill.border_radius = 3;

        m_progress_label.set_text(fmt::format("{}%", sd.bar_percent));
        m_progress_label.set_font(k_common_dialog_font, k_font_size_small, true);
        m_progress_label.align_text(overlay_element::center);
        m_progress_label.fore_color = k_color_text;
        m_progress_label.back_color = { 0.f, 0.f, 0.f, 0.f };
        m_progress_label.set_pos(bar_x, static_cast<int16_t>(bar_y + k_progress_h + 4));
        m_progress_label.set_size(bar_w, 20);

        m_last_bar_percent = sd.bar_percent;
    }

    m_buttons.clear();
    m_selected_button = 0;
    if (sd.btn_num > 0) {
        const uint16_t total_btn_w = static_cast<uint16_t>(sd.btn_num * k_sd_btn_w + (sd.btn_num - 1) * k_sd_btn_gap);
        int16_t btn_x = static_cast<int16_t>(card_x + (k_sd_card_w - total_btn_w) / 2);
        const int16_t btn_y = static_cast<int16_t>(card_y + k_sd_card_h - k_sd_btn_h - 22);

        for (uint8_t i = 0; i < sd.btn_num; i++) {
            button_entry btn;
            btn.bg.set_pos(btn_x, btn_y);
            btn.bg.set_size(k_sd_btn_w, k_sd_btn_h);
            btn.bg.back_color = k_color_btn_normal;
            btn.bg.border_color = { 0.f, 0.f, 0.f, 0.f };
            btn.bg.border_radius = k_sd_btn_radius;

            btn.text.set_text(sd.btn[i]);
            btn.text.set_font(k_common_dialog_font, k_font_size, true);
            btn.text.align_text(overlay_element::center);
            btn.text.fore_color = k_color_text;
            btn.text.back_color = { 0.f, 0.f, 0.f, 0.f };
            btn.text.set_pos(btn_x, static_cast<int16_t>(btn_y + (k_sd_btn_h - k_font_size) / 2 + k_text_voffset));
            btn.text.set_size(k_sd_btn_w, k_font_size + 4);

            btn.value = sd.btn_val[i];
            m_buttons.push_back(std::move(btn));

            btn_x = static_cast<int16_t>(btn_x + k_sd_btn_w + k_sd_btn_gap);
        }
    }
}

void common_dialog_overlay::build_savedata_info(DialogState &dialog, int date_format) {
    auto &sd = dialog.savedata;
    m_savedata_info_mode = true;
    m_savedata_list_mode = false;
    m_list_title = sd.list_title;

    const auto sel = sd.selected_save;

    if (sel >= sd.title.size() && sel >= sd.icon_buffer.size())
        return;

    const int16_t icon_left = 100;
    const int16_t icon_y = 120;

    if (sd.icon_buffer.size() > sel && !sd.icon_buffer[sel].empty()) {
        m_info_icon_data = std::make_unique<image_info>(sd.icon_buffer[sel]);
        m_info_icon.set_raw_image(m_info_icon_data.get());
    }
    m_info_icon.set_size(k_slot_icon_w, k_slot_icon_h);
    m_info_icon.set_pos(icon_left, icon_y);

    if (sd.title.size() > sel && !sd.title[sel].empty())
        m_info_title.set_text(sd.title[sel]);
    m_info_title.set_font(k_common_dialog_font, k_font_size_title, true);
    m_info_title.fore_color = k_color_text;
    m_info_title.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_title.set_pos(static_cast<int16_t>(icon_left + k_slot_icon_w + 20),
        static_cast<int16_t>(icon_y + k_slot_icon_h / 2 - k_font_size_title / 2 + k_text_voffset));
    m_info_title.set_size(static_cast<uint16_t>(k_vw - icon_left - k_slot_icon_w - 20 - k_margin * 4), k_font_size_title + 4);

    const int16_t row_x_key = 100;
    const int16_t row_x_val = 320;
    const int16_t updated_y = static_cast<int16_t>(icon_y + k_slot_icon_h + 30);

    m_info_updated_label.set_text(lang::get(lang::str::save_updated));
    m_info_updated_label.set_font(k_common_dialog_font, k_font_size, true);
    m_info_updated_label.fore_color = k_color_text_dim;
    m_info_updated_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_updated_label.set_pos(row_x_key, static_cast<int16_t>(updated_y + k_text_voffset));
    m_info_updated_label.set_size(static_cast<uint16_t>(row_x_val - row_x_key - 10), k_font_size + 4);

    if (sd.has_date.size() > sel && sd.has_date[sel] && sd.date.size() > sel) {
        m_info_date.set_text(format_date_time(date_format, &sd.date[sel]));
    }
    m_info_date.set_font(k_common_dialog_font, k_font_size, true);
    m_info_date.fore_color = k_color_text;
    m_info_date.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_date.set_pos(row_x_val, static_cast<int16_t>(updated_y + k_text_voffset));
    m_info_date.set_size(static_cast<uint16_t>(k_vw - row_x_val - k_margin * 2), k_font_size + 4);

    const int16_t details_y = static_cast<int16_t>(updated_y + k_font_size + 30);

    std::string details;
    if (sd.details.size() > sel && !sd.details[sel].empty())
        details = sd.details[sel];
    else if (sd.subtitle.size() > sel && !sd.subtitle[sel].empty())
        details = sd.subtitle[sel];

    m_info_details_key_label.set_text(lang::get(lang::str::save_details));
    m_info_details_key_label.set_font(k_common_dialog_font, k_font_size, true);
    m_info_details_key_label.fore_color = k_color_text_dim;
    m_info_details_key_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_details_key_label.set_pos(row_x_key, static_cast<int16_t>(details_y + k_text_voffset));
    m_info_details_key_label.set_size(static_cast<uint16_t>(row_x_val - row_x_key - 10), k_font_size + 4);

    m_info_details.set_text(details);
    m_info_details.set_font(k_common_dialog_font, k_font_size, true);
    m_info_details.set_wrap_text(true);
    m_info_details.fore_color = k_color_text;
    m_info_details.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_info_details.set_pos(row_x_val, static_cast<int16_t>(details_y + k_text_voffset));
    m_info_details.set_size(static_cast<uint16_t>(k_vw - row_x_val - k_margin * 2),
        static_cast<uint16_t>(k_vh / 3));

    m_buttons.clear();
    m_selected_button = 0;
    button_entry back_btn;
    back_btn.bg.set_pos(20, static_cast<int16_t>(k_vh - 48));
    back_btn.bg.set_size(46, 46);
    back_btn.bg.back_color = { 1.f, 1.f, 1.f, 0.1f };
    back_btn.bg.border_color = { 0.f, 0.f, 0.f, 0.f };
    back_btn.bg.border_radius = 10;

    back_btn.text.set_text("");
    back_btn.text.set_font(k_common_dialog_font, k_font_size, true);
    back_btn.text.align_text(overlay_element::center);
    back_btn.text.fore_color = k_color_text;
    back_btn.text.back_color = { 0.f, 0.f, 0.f, 0.f };
    back_btn.text.set_pos(6,
        static_cast<int16_t>(k_vh - 48 + (34 - k_font_size) / 2 + k_text_voffset));
    back_btn.text.set_size(64, k_font_size + 4);
    back_btn.value = 0;

    m_buttons.push_back(std::move(back_btn));
}

void common_dialog_overlay::build_ime_dialog(DialogState &dialog) {
    auto &ime = dialog.ime;
    const std::string submit_text = lang::get(lang::str::submit);
    const std::string cancel_text = lang::get(lang::str::cancel);

    m_ime_active = true;
    m_ime_cancelable = ime.cancelable;
    m_ime_max_length = ime.max_length;
    m_ime_last_text = ime.text;
    m_ime_preedit.clear();

    m_ime_text = utf8_to_u32string(ime.text);
    m_ime_cursor = static_cast<uint32_t>(m_ime_text.size());

    const int16_t bar_x = static_cast<int16_t>((k_vw - k_ime_bar_w) / 2);

    m_ime_bar_bg.set_pos(bar_x, k_ime_bar_y);
    m_ime_bar_bg.set_size(k_ime_bar_w, k_ime_bar_h);
    m_ime_bar_bg.back_color = k_color_card_bg;
    m_ime_bar_bg.border_radius = k_ime_bar_radius;

    const uint16_t inner_pad = 6;
    const uint16_t submit_btn_w = std::max<uint16_t>(k_ime_btn_min_w,
        static_cast<uint16_t>(measure_label_width(submit_text, k_font_size_small, true) + k_ime_btn_hpad * 2));
    const uint16_t cancel_btn_w = m_ime_cancelable
        ? std::max<uint16_t>(k_ime_btn_min_w,
              static_cast<uint16_t>(measure_label_width(cancel_text, k_font_size_small, true) + k_ime_btn_hpad * 2))
        : 0;
    const uint16_t btns_total_w = static_cast<uint16_t>(submit_btn_w + cancel_btn_w
        + (m_ime_cancelable ? k_ime_btn_gap : 0));

    const int16_t field_x = static_cast<int16_t>(bar_x + inner_pad);
    const int16_t field_y = static_cast<int16_t>(k_ime_bar_y + (k_ime_bar_h - k_ime_field_h) / 2);
    const uint16_t field_w = static_cast<uint16_t>(k_ime_bar_w - btns_total_w - inner_pad * 3);

    m_ime_text_bg.set_pos(field_x, field_y);
    m_ime_text_bg.set_size(field_w, k_ime_field_h);
    m_ime_text_bg.back_color = { 1.f, 1.f, 1.f, 0.08f };
    m_ime_text_bg.border_radius = 6;

    m_ime_text_label.set_text(std::string(ime.text));
    m_ime_text_label.set_font(k_common_dialog_font, k_font_size, true);
    m_ime_text_label.fore_color = k_color_text;
    m_ime_text_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_ime_text_label.set_pos(static_cast<int16_t>(field_x + 8),
        static_cast<int16_t>(field_y + (k_ime_field_h - k_font_size) / 2 + k_text_voffset));
    m_ime_text_label.set_size(static_cast<uint16_t>(field_w - 16), k_font_size + 4);

    m_buttons.clear();
    int16_t btn_x = static_cast<int16_t>(bar_x + k_ime_bar_w - btns_total_w - inner_pad);
    const int16_t btn_y = static_cast<int16_t>(k_ime_bar_y + (k_ime_bar_h - k_ime_field_h) / 2);

    {
        button_entry submit;
        submit.bg.set_pos(btn_x, btn_y);
        submit.bg.set_size(submit_btn_w, k_ime_field_h);
        submit.bg.back_color = { 1.f, 1.f, 1.f, 0.10f };
        submit.bg.border_radius = 6;
        submit.text.set_text(submit_text);
        submit.text.set_font(k_common_dialog_font, k_font_size_small, true);
        submit.text.align_text(overlay_element::center);
        submit.text.fore_color = k_color_text;
        submit.text.back_color = { 0.f, 0.f, 0.f, 0.f };
        submit.text.set_pos(btn_x, static_cast<int16_t>(btn_y + (k_ime_field_h - k_font_size_small) / 2 + k_text_voffset));
        submit.text.set_size(submit_btn_w, k_font_size_small + 4);
        submit.value = 0;
        m_buttons.push_back(std::move(submit));
        btn_x = static_cast<int16_t>(btn_x + submit_btn_w + k_ime_btn_gap);
    }

    if (m_ime_cancelable) {
        button_entry cancel;
        cancel.bg.set_pos(btn_x, btn_y);
        cancel.bg.set_size(cancel_btn_w, k_ime_field_h);
        cancel.bg.back_color = { 1.f, 1.f, 1.f, 0.04f };
        cancel.bg.border_radius = 6;
        cancel.text.set_text(cancel_text);
        cancel.text.set_font(k_common_dialog_font, k_font_size_small, true);
        cancel.text.align_text(overlay_element::center);
        cancel.text.fore_color = k_color_text_dim;
        cancel.text.back_color = { 0.f, 0.f, 0.f, 0.f };
        cancel.text.set_pos(btn_x, static_cast<int16_t>(btn_y + (k_ime_field_h - k_font_size_small) / 2 + k_text_voffset));
        cancel.text.set_size(cancel_btn_w, k_font_size_small + 4);
        cancel.value = 1;
        m_buttons.push_back(std::move(cancel));
    }
}

void common_dialog_overlay::build_trophy_setup(DialogState &dialog) {
    m_trophy_tick = dialog.trophy.tick;

    const int16_t card_x = static_cast<int16_t>((k_vw - k_trophy_card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((k_vh - k_trophy_card_h) / 2);

    m_card_background.set_pos(card_x, card_y);
    m_card_background.set_size(k_trophy_card_w, k_trophy_card_h);
    m_card_background.back_color = k_color_card_bg;
    m_card_background.border_radius = 10;

    m_trophy_label.set_text(lang::get(lang::str::preparing_start_app));
    m_trophy_label.set_font(k_common_dialog_font, k_font_size, true);
    m_trophy_label.align_text(overlay_element::center);
    m_trophy_label.fore_color = k_color_text;
    m_trophy_label.back_color = { 0.f, 0.f, 0.f, 0.f };
    m_trophy_label.set_pos(static_cast<int16_t>(card_x + k_margin * 2),
        static_cast<int16_t>(card_y + k_trophy_card_h / 2 - k_font_size / 2 + k_text_voffset));
    m_trophy_label.set_size(static_cast<uint16_t>(k_trophy_card_w - k_margin * 4), k_font_size + 4);
}

compiled_resource common_dialog_overlay::get_compiled() {
    if (!visible)
        return {};

    compiled_resource result;

    switch (m_active_type) {
    case MESSAGE_DIALOG:
        result = compile_message_dialog();
        break;
    case SAVEDATA_DIALOG:
        if (m_savedata_info_mode)
            result = compile_savedata_info();
        else if (m_savedata_list_mode)
            result = compile_savedata_list();
        else
            result = compile_savedata_fixed();
        break;
    case IME_DIALOG:
        result = compile_ime_dialog();
        break;
    case TROPHY_SETUP_DIALOG:
        result = compile_trophy_setup();
        break;
    default:
        break;
    }

    compiled_resource final_result;
    final_result.add(m_dim_background.get_compiled());

    const bool is_card_dialog = (m_active_type == MESSAGE_DIALOG
        || m_active_type == TROPHY_SETUP_DIALOG
        || (m_active_type == SAVEDATA_DIALOG && !m_savedata_list_mode && !m_savedata_info_mode));

    if (m_dismissing && is_card_dialog) {
        if (!m_dismiss_fade_anim.active && !m_dismiss_scale_anim.active)
            return {};
        if (m_dismiss_scale_anim.active)
            m_dismiss_scale_anim.apply(result);
        final_result.add(result);
        if (m_dismiss_fade_anim.active)
            m_dismiss_fade_anim.apply(final_result);
        return final_result;
    }

    if (m_scale_anim.active && is_card_dialog)
        m_scale_anim.apply(result);

    final_result.add(result);
    m_fade_anim.apply(final_result);
    return final_result;
}

compiled_resource common_dialog_overlay::compile_message_dialog() {
    compiled_resource result;

    result.add(m_card_background.get_compiled());

    result.add(m_message_label.get_compiled());

    if (m_has_progress_bar) {
        result.add(m_progress_bg.get_compiled());
        result.add(m_progress_fill.get_compiled());
        result.add(m_progress_label.get_compiled());
    }

    for (size_t i = 0; i < m_buttons.size(); i++) {
        auto &btn = m_buttons[i];
        const bool selected = (static_cast<int>(i) == m_selected_button);

        if (selected)
            set_focus_target(btn.bg.x, btn.bg.y, btn.bg.w, btn.bg.h, k_btn_radius);

        {
            auto btn_compiled = btn.bg.get_compiled();
            set_btn_gloss_on_compiled(btn_compiled, k_btn_gloss_height, k_btn_gloss_opacity, k_btn_gloss_bottom_opacity);
            result.add(btn_compiled);
        }
        result.add(btn.text.get_compiled());
    }

    draw_focus_ring(result);
    return result;
}

compiled_resource common_dialog_overlay::compile_savedata_list() {
    compiled_resource result;

    overlay_element top_bar;
    top_bar.set_pos(0, 0);
    top_bar.set_size(k_vw, 96);
    top_bar.back_color = { 0.f, 0.f, 0.f, 0.f };
    result.add(top_bar.get_compiled());

    if (!m_list_title.empty()) {
        label title_lbl;
        title_lbl.set_text(m_list_title);
        title_lbl.set_font(k_common_dialog_font, k_font_size_title, true);
        title_lbl.align_text(overlay_element::center);
        title_lbl.fore_color = k_color_text;
        title_lbl.back_color = { 0.f, 0.f, 0.f, 0.f };
        title_lbl.set_pos(0, static_cast<int16_t>((96 - k_font_size_title) / 2 + k_text_voffset));
        title_lbl.set_size(k_vw, k_font_size_title + 4);
        result.add(title_lbl.get_compiled());
    }

    {
        ensure_icons_loaded();

        const int16_t cancel_x = 20;
        const int16_t cancel_y = 42;
        const uint16_t cancel_w = 46;
        const uint16_t cancel_h = 46;

        if (m_cancel_btn_focused) {
            set_focus_target(cancel_x, cancel_y, cancel_w, cancel_h, 10);
        }

        rounded_rect cancel_bg;
        cancel_bg.set_pos(cancel_x, cancel_y);
        cancel_bg.set_size(cancel_w, cancel_h);
        cancel_bg.back_color = { 1.f, 1.f, 1.f, 0.1f };
        cancel_bg.border_color = { 0.f, 0.f, 0.f, 0.f };
        cancel_bg.border_radius = 10;
        {
            auto cancel_compiled = cancel_bg.get_compiled();
            set_btn_gloss_on_compiled(cancel_compiled, 0.30f, 0.35f, 0.35f);
            result.add(cancel_compiled);
        }

        if (m_cross_icon_data && m_cross_icon_data->get_data()) {
            const uint16_t icon_pad = 10;
            image_view cross_img;
            cross_img.set_raw_image(m_cross_icon_data.get());
            cross_img.set_pos(static_cast<int16_t>(cancel_x + icon_pad),
                static_cast<int16_t>(cancel_y + icon_pad));
            cross_img.set_size(static_cast<uint16_t>(cancel_w - icon_pad * 2),
                static_cast<uint16_t>(cancel_h - icon_pad * 2));
            result.add(cross_img.get_compiled());
        }
    }

    if (m_save_slots.empty()) {
        label no_saves;
        no_saves.set_text(lang::get(lang::str::no_saved_data));
        no_saves.set_font(k_common_dialog_font, k_font_size_title, true);
        no_saves.align_text(overlay_element::center);
        no_saves.fore_color = k_color_text_dim;
        no_saves.back_color = { 0.f, 0.f, 0.f, 0.f };
        no_saves.set_pos(0, static_cast<int16_t>(k_vh / 2 - k_font_size_title / 2 + k_text_voffset));
        no_saves.set_size(k_vw, k_font_size_title + 4);
        result.add(no_saves.get_compiled());
    } else {
        const int16_t list_start_y = 96;
        const int16_t icon_left = 150;

        for (int i = 0; i < static_cast<int>(m_save_slots.size()); i++) {
            const int visual_idx = i - m_scroll_offset;
            if (visual_idx < 0 || visual_idx >= k_max_visible_slots)
                continue;

            auto &slot = *m_save_slots[i];
            const int16_t slot_y = static_cast<int16_t>(list_start_y + visual_idx * k_slot_h);
            const bool selected = (!m_cancel_btn_focused && i == m_selected_slot);

            if (selected) {
                set_focus_target(0, slot_y, k_vw, k_slot_h, 0);
            }

            slot.bg.set_pos(0, slot_y);
            slot.bg.set_size(k_vw, k_slot_h);
            slot.bg.back_color = selected ? k_color_slot_hover : color4f{ 0.f, 0.f, 0.f, 0.f };
            result.add(slot.bg.get_compiled());

            apply_gloss_overlay(result, 0, slot_y, k_vw, k_slot_h);

            slot.icon.set_pos(icon_left, static_cast<int16_t>(slot_y));
            slot.icon.set_size(k_slot_icon_w, k_slot_h);
            result.add(slot.icon.get_compiled());

            const uint16_t info_btn_size = 32;
            const int16_t info_right_margin = static_cast<int16_t>(k_vw * 0.20f);
            const uint16_t text_right_reserve = slot.has_data ? static_cast<uint16_t>(info_btn_size + info_right_margin + 10) : 0;

            const int16_t text_x = static_cast<int16_t>(icon_left + k_slot_icon_w + 20);
            const uint16_t text_w = static_cast<uint16_t>(k_vw - text_x - k_margin * 2 - text_right_reserve);

            if (slot.has_data) {
                const int16_t line_gap = 8;
                const int16_t total_text_h = k_font_size + line_gap + k_font_size_small + line_gap + k_font_size_small;
                const int16_t text_top = static_cast<int16_t>(slot_y + (k_slot_h - total_text_h) / 2 + k_text_voffset);

                slot.title.set_pos(text_x, text_top);
                slot.title.set_size(text_w, k_font_size + 4);
                result.add(slot.title.get_compiled());

                slot.date_label.set_pos(text_x, static_cast<int16_t>(text_top + k_font_size + line_gap));
                slot.date_label.set_size(text_w, k_font_size_small + 4);
                result.add(slot.date_label.get_compiled());

                slot.subtitle.set_pos(text_x, static_cast<int16_t>(text_top + k_font_size + line_gap + k_font_size_small + line_gap));
                slot.subtitle.set_size(text_w, k_font_size_small + 4);
                result.add(slot.subtitle.get_compiled());

                {
                    const int16_t info_x = static_cast<int16_t>(k_vw - info_btn_size - info_right_margin);
                    const int16_t info_y = static_cast<int16_t>(slot_y + (k_slot_h - info_btn_size) / 2);

                    ellipse info_bg;
                    info_bg.set_pos(info_x, info_y);
                    info_bg.set_size(info_btn_size, info_btn_size);
                    info_bg.back_color = { 1.f, 1.f, 1.f, 0.1f };
                    info_bg.border_color = { 0.f, 0.f, 0.f, 0.f };
                    info_bg.border_radius = info_btn_size / 2;
                    {
                        auto info_compiled = info_bg.get_compiled();
                        set_btn_gloss_on_compiled(info_compiled, 0.30f, 0.35f, 0.35f);
                        result.add(info_compiled);
                    }

                    if (m_info_icon_btn_data && m_info_icon_btn_data->get_data()) {
                        const uint16_t icon_pad = 6;
                        image_view info_img;
                        info_img.set_raw_image(m_info_icon_btn_data.get());
                        info_img.set_pos(static_cast<int16_t>(info_x + icon_pad),
                            static_cast<int16_t>(info_y + icon_pad));
                        info_img.set_size(static_cast<uint16_t>(info_btn_size - icon_pad * 2),
                            static_cast<uint16_t>(info_btn_size - icon_pad * 2));
                        result.add(info_img.get_compiled());
                    }
                }
            } else {
                slot.title.set_pos(text_x,
                    static_cast<int16_t>(slot_y + (k_slot_h - k_font_size) / 2 + k_text_voffset));
                slot.title.set_size(text_w, k_font_size + 4);
                result.add(slot.title.get_compiled());
            }
        }
    }

    draw_focus_ring(result);
    return result;
}

compiled_resource common_dialog_overlay::compile_savedata_fixed() {
    compiled_resource result;

    result.add(m_card_background.get_compiled());

    result.add(m_info_icon.get_compiled());
    result.add(m_info_title.get_compiled());
    result.add(m_info_date.get_compiled());

    result.add(m_info_subtitle.get_compiled());
    overlay_element sep;
    sep.set_pos(static_cast<int16_t>(m_card_background.x + k_margin * 2),
        static_cast<int16_t>(m_card_background.y + 34 + k_slot_icon_h + 10));
    sep.set_size(static_cast<uint16_t>(m_card_background.w - k_margin * 4), 1);
    sep.back_color = k_color_separator;
    result.add(sep.get_compiled());

    result.add(m_message_label.get_compiled());

    if (m_has_progress_bar) {
        result.add(m_progress_bg.get_compiled());
        result.add(m_progress_fill.get_compiled());
        result.add(m_progress_label.get_compiled());
    }

    for (size_t i = 0; i < m_buttons.size(); i++) {
        auto &btn = m_buttons[i];
        const bool selected = (static_cast<int>(i) == m_selected_button);
        if (selected)
            set_focus_target(btn.bg.x, btn.bg.y, btn.bg.w, btn.bg.h, k_sd_btn_radius);
        {
            auto btn_compiled = btn.bg.get_compiled();
            set_btn_gloss_on_compiled(btn_compiled, k_btn_gloss_height, k_btn_gloss_opacity, k_btn_gloss_bottom_opacity);
            result.add(btn_compiled);
        }
        result.add(btn.text.get_compiled());
    }

    draw_focus_ring(result);
    return result;
}

compiled_resource common_dialog_overlay::compile_savedata_info() {
    compiled_resource result;

    if (!m_list_title.empty()) {
        label title_lbl;
        title_lbl.set_text(m_list_title);
        title_lbl.set_font(k_common_dialog_font, k_font_size_title, true);
        title_lbl.align_text(overlay_element::center);
        title_lbl.fore_color = k_color_text;
        title_lbl.back_color = { 0.f, 0.f, 0.f, 0.f };
        title_lbl.set_pos(0, static_cast<int16_t>((96 - k_font_size_title) / 2 + k_text_voffset));
        title_lbl.set_size(k_vw, k_font_size_title + 4);
        result.add(title_lbl.get_compiled());
    }

    result.add(m_info_icon.get_compiled());
    result.add(m_info_title.get_compiled());

    result.add(m_info_updated_label.get_compiled());
    result.add(m_info_date.get_compiled());

    result.add(m_info_details_key_label.get_compiled());
    result.add(m_info_details.get_compiled());

    ensure_icons_loaded();
    for (size_t i = 0; i < m_buttons.size(); i++) {
        auto &btn = m_buttons[i];
        set_focus_target(btn.bg.x, btn.bg.y, btn.bg.w, btn.bg.h, 10);
        {
            auto btn_compiled = btn.bg.get_compiled();
            set_btn_gloss_on_compiled(btn_compiled, 0.30f, 0.35f, 0.35f);
            result.add(btn_compiled);
        }

        if (m_back_arrow_icon_data && m_back_arrow_icon_data->get_data()) {
            const uint16_t icon_pad = 10;
            const uint16_t avail_w = static_cast<uint16_t>(btn.bg.w - icon_pad * 2);
            const uint16_t avail_h = static_cast<uint16_t>(btn.bg.h - icon_pad * 2);
            const float src_w = static_cast<float>(m_back_arrow_icon_data->w);
            const float src_h = static_cast<float>(m_back_arrow_icon_data->h);
            const float scale = std::min(static_cast<float>(avail_w) / src_w, static_cast<float>(avail_h) / src_h);
            const uint16_t draw_w = static_cast<uint16_t>(src_w * scale);
            const uint16_t draw_h = static_cast<uint16_t>(src_h * scale);
            const int16_t draw_x = static_cast<int16_t>(btn.bg.x + icon_pad + (avail_w - draw_w) / 2);
            const int16_t draw_y = static_cast<int16_t>(btn.bg.y + icon_pad + (avail_h - draw_h) / 2);
            image_view arrow_img;
            arrow_img.set_raw_image(m_back_arrow_icon_data.get());
            arrow_img.set_pos(draw_x, draw_y);
            arrow_img.set_size(draw_w, draw_h);
            result.add(arrow_img.get_compiled());
        }
    }

    draw_focus_ring(result);
    return result;
}

compiled_resource common_dialog_overlay::compile_ime_dialog() {
    compiled_resource result;

    result.add(m_ime_bar_bg.get_compiled());

    result.add(m_ime_text_bg.get_compiled());
    result.add(m_ime_text_label.get_compiled());

    {
        const uint32_t caret = m_ime_cursor; // already clamped in poll_dialog
        const auto full_u32 = m_ime_text; // committed text as u32

        auto measure_substring = [&](const std::u32string &str) -> uint16_t {
            if (str.empty())
                return 0;
            overlay_element tmp;
            tmp.set_font(k_common_dialog_font, k_font_size, true);
            tmp.set_unicode_text(str);
            tmp.set_size(UINT16_MAX, k_font_size + 4);
            uint16_t tw = 0, th = 0;
            tmp.measure_text(tw, th, true);
            return tw;
        };

        const std::u32string text_before_caret = full_u32.substr(0, std::min(caret, static_cast<uint32_t>(full_u32.size())));
        const uint16_t caret_x_offset = measure_substring(text_before_caret);

        const std::u32string preedit_u32 = utf8_to_u32string(m_ime_preedit);
        const uint16_t preedit_w = measure_substring(preedit_u32);

        const int16_t field_top = m_ime_text_bg.y;
        const uint16_t field_h = m_ime_text_bg.h;
        const uint16_t cursor_h = static_cast<uint16_t>(field_h - k_ime_cursor_vpad * 2);

        const int16_t cursor_x = static_cast<int16_t>(m_ime_text_label.x + caret_x_offset + preedit_w);
        const int16_t cursor_y = static_cast<int16_t>(field_top + k_ime_cursor_vpad);

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        if ((ms % 1000) < 530) {
            overlay_element cursor_line;
            cursor_line.set_pos(cursor_x, cursor_y);
            cursor_line.set_size(k_ime_cursor_width, cursor_h);
            cursor_line.back_color = k_color_text;
            result.add(cursor_line.get_compiled());
        }

        if (preedit_w > 0) {
            const int16_t underline_x = static_cast<int16_t>(m_ime_text_label.x + caret_x_offset);
            const int16_t underline_y = static_cast<int16_t>(field_top + field_h - k_ime_preedit_underline_vpad - k_ime_preedit_underline_thickness);
            overlay_element underline;
            underline.set_pos(underline_x, underline_y);
            underline.set_size(preedit_w, k_ime_preedit_underline_thickness);
            underline.back_color = k_color_text;
            result.add(underline.get_compiled());
        }
    }

    for (size_t i = 0; i < m_buttons.size(); i++) {
        auto &btn = m_buttons[i];
        const bool selected = (static_cast<int>(i) == m_selected_button);
        if (selected)
            set_focus_target(btn.bg.x, btn.bg.y, btn.bg.w, btn.bg.h, 6);
        {
            auto btn_compiled = btn.bg.get_compiled();
            set_btn_gloss_on_compiled(btn_compiled, k_btn_gloss_height, k_btn_gloss_opacity, k_btn_gloss_bottom_opacity);
            result.add(btn_compiled);
        }
        result.add(btn.text.get_compiled());
    }

    draw_focus_ring(result);
    return result;
}

compiled_resource common_dialog_overlay::compile_trophy_setup() {
    compiled_resource result;

    result.add(m_card_background.get_compiled());
    apply_gloss_overlay(result, m_card_background.x, m_card_background.y,
        m_card_background.w, m_card_background.h);

    result.add(m_trophy_label.get_compiled());

    return result;
}

void common_dialog_overlay::on_button_pressed(pad_button button_press, bool is_auto_repeat) {
    if (!m_dialog)
        return;
    if (m_fade_anim.active || m_dismissing)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_dialog->mutex);

    switch (m_active_type) {
    case MESSAGE_DIALOG:
        handle_message_input(button_press, *m_dialog);
        break;
    case SAVEDATA_DIALOG:
        if (m_savedata_info_mode)
            handle_savedata_info_input(button_press, *m_dialog);
        else if (m_savedata_list_mode)
            handle_savedata_list_input(button_press, *m_dialog);
        else
            handle_savedata_fixed_input(button_press, *m_dialog);
        break;
    case IME_DIALOG:
        handle_ime_input(button_press, *m_dialog);
        break;
    default:
        break;
    }
}

void common_dialog_overlay::handle_message_input(pad_button btn, DialogState &dialog) {
    if (m_buttons.empty())
        return;

    if (is_confirm_button(btn)) {
        const auto &selected = m_buttons[m_selected_button];
        dialog.msg.status = selected.value;
        dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        m_stop_input_loop.store(true);
        m_stop_pad_interception.store(true);
        return;
    }

    if (is_cancel_button(btn)) {
        if (m_buttons.size() > 1) {
            dialog.msg.status = m_buttons.back().value;
            dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
        }
        return;
    }

    switch (btn) {
    case pad_button::dpad_left:
    case pad_button::ls_left:
        if (m_selected_button > 0)
            m_selected_button--;
        break;
    case pad_button::dpad_right:
    case pad_button::ls_right:
        if (m_selected_button < static_cast<int>(m_buttons.size()) - 1)
            m_selected_button++;
        break;
    default:
        break;
    }
}

void common_dialog_overlay::handle_savedata_list_input(pad_button btn, DialogState &dialog) {
    if (is_confirm_button(btn)) {
        if (m_cancel_btn_focused) {
            dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
            dialog.savedata.draw_info_window = false;
            dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
            dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
            return;
        }

        if (!m_save_slots.empty()) {
            const uint32_t slot_index = m_save_slots[m_selected_slot]->slot_index;
            dialog.savedata.selected_save = slot_index;
            dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
            dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
        }
        return;
    }

    if (is_cancel_button(btn)) {
        dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
        dialog.savedata.draw_info_window = false;
        dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        m_stop_input_loop.store(true);
        m_stop_pad_interception.store(true);
        return;
    }

    switch (btn) {
    case pad_button::dpad_up:
    case pad_button::ls_up:
        if (m_cancel_btn_focused) {
            break;
        }
        if (m_selected_slot > 0) {
            m_selected_slot--;
            if (m_selected_slot < m_scroll_offset)
                m_scroll_offset = m_selected_slot;
        } else {
            m_cancel_btn_focused = true;
        }
        break;
    case pad_button::dpad_down:
    case pad_button::ls_down:
        if (m_cancel_btn_focused) {
            m_cancel_btn_focused = false;
            break;
        }
        if (m_selected_slot < static_cast<int>(m_save_slots.size()) - 1) {
            m_selected_slot++;
            if (m_selected_slot >= m_scroll_offset + k_max_visible_slots)
                m_scroll_offset = m_selected_slot - k_max_visible_slots + 1;
        }
        break;
    case pad_button::triangle:
        if (!m_cancel_btn_focused && !m_save_slots.empty() && m_save_slots[m_selected_slot]->has_data) {
            dialog.savedata.draw_info_window = true;
            dialog.savedata.selected_save = m_save_slots[m_selected_slot]->slot_index;
        }
        break;
    default:
        break;
    }
}

void common_dialog_overlay::handle_savedata_fixed_input(pad_button btn, DialogState &dialog) {
    if (m_buttons.empty())
        return;

    if (is_confirm_button(btn)) {
        const auto &selected = m_buttons[m_selected_button];
        dialog.savedata.button_id = selected.value;
        dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        m_stop_input_loop.store(true);
        m_stop_pad_interception.store(true);
        return;
    }

    if (is_cancel_button(btn)) {
        dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
        dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        m_stop_input_loop.store(true);
        m_stop_pad_interception.store(true);
        return;
    }

    switch (btn) {
    case pad_button::dpad_left:
    case pad_button::ls_left:
        if (m_selected_button > 0)
            m_selected_button--;
        break;
    case pad_button::dpad_right:
    case pad_button::ls_right:
        if (m_selected_button < static_cast<int>(m_buttons.size()) - 1)
            m_selected_button++;
        break;
    default:
        break;
    }
}

void common_dialog_overlay::handle_savedata_info_input(pad_button btn, DialogState &dialog) {
    switch (btn) {
    case pad_button::circle:
    case pad_button::cross:
        dialog.savedata.draw_info_window = false;
        break;
    default:
        break;
    }
}

void common_dialog_overlay::handle_ime_input(pad_button btn, DialogState &dialog) {
    if (is_confirm_button(btn)) {
        if (m_selected_button == 0) {
            submit_ime_dialog(dialog);
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
        } else {
            cancel_ime_dialog(dialog);
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
        }
        return;
    }

    if (is_cancel_button(btn)) {
        if (m_ime_cancelable) {
            cancel_ime_dialog(dialog);
            m_stop_input_loop.store(true);
            m_stop_pad_interception.store(true);
        }
        return;
    }

    switch (btn) {
    case pad_button::dpad_left:
    case pad_button::ls_left:
        if (m_selected_button > 0)
            m_selected_button--;
        break;
    case pad_button::dpad_right:
    case pad_button::ls_right:
        if (m_selected_button < static_cast<int>(m_buttons.size()) - 1)
            m_selected_button++;
        break;
    default:
        break;
    }
}

void common_dialog_overlay::on_touch_pressed(float vx, float vy) {
    if (!m_dialog || m_fade_anim.active || m_dismissing)
        return;

    std::lock_guard<std::recursive_mutex> lock(m_dialog->mutex);
    const auto confirm_button = (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? pad_button::circle : pad_button::cross;
    const auto cancel_button = (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? pad_button::cross : pad_button::circle;

    switch (m_active_type) {
    case MESSAGE_DIALOG:
    case IME_DIALOG: {
        for (size_t i = 0; i < m_buttons.size(); i++) {
            auto &btn = m_buttons[i];
            if (vx >= btn.bg.x && vx <= btn.bg.x + btn.bg.w
                && vy >= btn.bg.y && vy <= btn.bg.y + btn.bg.h) {
                m_selected_button = static_cast<int>(i);
                on_button_pressed(confirm_button, false);
                return;
            }
        }
        break;
    }
    case SAVEDATA_DIALOG: {
        if (m_savedata_info_mode) {
            on_button_pressed(cancel_button, false);
            return;
        }
        if (m_savedata_list_mode) {
            const int16_t cancel_x = 20, cancel_y = 42;
            const uint16_t cancel_w = 46, cancel_h = 46;
            if (vx >= cancel_x && vx <= cancel_x + cancel_w
                && vy >= cancel_y && vy <= cancel_y + cancel_h) {
                on_button_pressed(cancel_button, false);
                return;
            }
            const int16_t list_start_y = 96;
            for (int i = 0; i < static_cast<int>(m_save_slots.size()); i++) {
                const int visual_idx = i - m_scroll_offset;
                if (visual_idx < 0 || visual_idx >= k_max_visible_slots)
                    continue;
                const int16_t slot_y = static_cast<int16_t>(list_start_y + visual_idx * k_slot_h);
                if (vy >= slot_y && vy < slot_y + k_slot_h) {
                    if (m_save_slots[i]->has_data) {
                        const uint16_t info_btn_size = 32;
                        const int16_t info_right_margin = static_cast<int16_t>(k_vw * 0.20f);
                        const int16_t info_x = static_cast<int16_t>(k_vw - info_btn_size - info_right_margin);
                        const int16_t info_y_pos = static_cast<int16_t>(slot_y + (k_slot_h - info_btn_size) / 2);
                        const int16_t hit_pad = 8;
                        if (vx >= info_x - hit_pad && vx <= info_x + info_btn_size + hit_pad
                            && vy >= info_y_pos - hit_pad && vy <= info_y_pos + info_btn_size + hit_pad) {
                            m_selected_slot = i;
                            m_cancel_btn_focused = false;
                            on_button_pressed(pad_button::triangle, false);
                            return;
                        }
                    }

                    m_selected_slot = i;
                    m_cancel_btn_focused = false;
                    on_button_pressed(confirm_button, false);
                    return;
                }
            }
        } else {
            for (size_t i = 0; i < m_buttons.size(); i++) {
                auto &btn = m_buttons[i];
                if (vx >= btn.bg.x && vx <= btn.bg.x + btn.bg.w
                    && vy >= btn.bg.y && vy <= btn.bg.y + btn.bg.h) {
                    m_selected_button = static_cast<int>(i);
                    on_button_pressed(confirm_button, false);
                    return;
                }
            }
        }
        break;
    }
    default:
        break;
    }
}
} // namespace overlay
