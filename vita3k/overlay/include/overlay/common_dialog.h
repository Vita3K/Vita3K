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
#include <overlay/layouts.h>
#include <overlay/user_interface.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DialogState;

namespace overlay {

class common_dialog_overlay : public user_interface {
public:
    common_dialog_overlay();

    void update(uint64_t timestamp_us) override;
    compiled_resource get_compiled() override;
    void on_button_pressed(pad_button button_press, bool is_auto_repeat) override;
    void on_touch_pressed(float vx, float vy) override;

    bool poll_dialog(DialogState &dialog, int date_format, int sys_button);

private:
    int m_active_type = 0;

    overlay_element m_dim_background;
    rounded_rect m_card_background;
    label m_message_label;

    rounded_rect m_progress_bg;
    rounded_rect m_progress_fill;
    label m_progress_label;
    bool m_has_progress_bar = false;
    uint32_t m_last_bar_percent = 0;

    struct button_entry {
        rounded_rect bg;
        label text;
        uint32_t value = 0;
    };
    std::vector<button_entry> m_buttons;
    int m_selected_button = 0;

    struct save_slot_entry {
        rounded_rect bg;
        image_view icon;
        std::unique_ptr<image_info> icon_info;
        label title;
        label subtitle;
        label date_label;
        uint32_t slot_index = 0;
        bool has_data = false;
    };
    std::vector<std::unique_ptr<save_slot_entry>> m_save_slots;
    int m_selected_slot = 0;
    int m_scroll_offset = 0;
    bool m_savedata_list_mode = false;
    bool m_savedata_info_mode = false;
    uint32_t m_savedata_mode_display = 0;
    uint32_t m_savedata_display_type = 0;
    uint8_t m_savedata_btn_num = 0;
    std::string m_list_title;

    std::unique_ptr<image_info> m_cross_icon_data;
    std::unique_ptr<image_info> m_info_icon_btn_data;
    std::unique_ptr<image_info> m_back_arrow_icon_data;
    bool m_icons_loaded = false;
    void ensure_icons_loaded();
    static std::unique_ptr<image_info> load_icon_white(const std::string &path);

    image_view m_info_icon;
    std::unique_ptr<image_info> m_info_icon_data;
    label m_info_title;
    label m_info_date;
    label m_info_details;
    label m_info_subtitle;
    label m_info_updated_label;
    label m_info_details_key_label;

    std::u32string m_ime_text;
    std::string m_ime_preedit;
    uint32_t m_ime_cursor = 0;
    uint32_t m_ime_max_length = 0;
    bool m_ime_cancelable = false;
    rounded_rect m_ime_bar_bg;
    rounded_rect m_ime_text_bg;
    label m_ime_text_label;
    bool m_ime_active = false;
    std::string m_ime_last_text;

    label m_trophy_label;
    uint64_t m_trophy_tick = 0;

    animation_color_interpolate m_fade_anim;
    animation_scale m_scale_anim;

    bool m_dismissing = false;
    bool m_dismiss_is_substate = false;
    animation_color_interpolate m_dismiss_fade_anim;
    animation_scale m_dismiss_scale_anim;

    struct focus_state {
        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        float radius = 0.f;
    };
    focus_state m_focus_current;
    focus_state m_focus_target;
    bool m_focus_initialized = false;
    float m_focus_pulse_time = 0.f;
    bool m_cancel_btn_focused = false;
    static constexpr float k_focus_lerp_speed = 12.f;

    std::string m_last_message;

    void build_message_dialog(DialogState &dialog);
    void build_savedata_dialog(DialogState &dialog, int date_format);
    void build_savedata_list(DialogState &dialog, int date_format);
    void build_savedata_fixed(DialogState &dialog, int date_format);
    void build_savedata_info(DialogState &dialog, int date_format);
    void build_ime_dialog(DialogState &dialog);
    void build_trophy_setup(DialogState &dialog);
    void clear_all();

    compiled_resource compile_message_dialog();
    compiled_resource compile_savedata_list();
    compiled_resource compile_savedata_fixed();
    compiled_resource compile_savedata_info();
    compiled_resource compile_ime_dialog();
    compiled_resource compile_trophy_setup();

    void handle_message_input(pad_button btn, DialogState &dialog);
    void handle_savedata_list_input(pad_button btn, DialogState &dialog);
    void handle_savedata_fixed_input(pad_button btn, DialogState &dialog);
    void handle_savedata_info_input(pad_button btn, DialogState &dialog);
    void handle_ime_input(pad_button btn, DialogState &dialog);

    void set_focus_target(float x, float y, float w, float h, float radius);
    void draw_focus_ring(compiled_resource &res);
    void apply_gloss_overlay(compiled_resource &res, int16_t x, int16_t y,
        uint16_t w, uint16_t h);
    static void set_btn_gloss_on_compiled(compiled_resource &compiled,
        float gloss_height, float opacity, float bottom_opacity);

    static std::string format_date_time(int date_format, const void *sce_date_time);
    void update_progress(DialogState &dialog, bool is_savedata);

    DialogState *m_dialog = nullptr;
    int m_date_format = 0;
    int m_sys_button = 0;

    bool is_confirm_button(pad_button button) const;
    bool is_cancel_button(pad_button button) const;

    static constexpr uint16_t k_vw = 960;
    static constexpr uint16_t k_vh = 544;
    static constexpr uint16_t k_margin = 16;

    // Message dialog card
    static constexpr uint16_t k_card_w = 564;
    static constexpr uint16_t k_card_h = 362;

    // IME compact input bar
    static constexpr uint16_t k_ime_bar_w = 768;
    static constexpr uint16_t k_ime_bar_h = 44;
    static constexpr int16_t k_ime_bar_y = 55;
    static constexpr uint16_t k_ime_bar_radius = 10;
    static constexpr uint16_t k_ime_btn_min_w = 50;
    static constexpr uint16_t k_ime_btn_hpad = 12;
    static constexpr uint16_t k_ime_btn_gap = 6;
    static constexpr uint16_t k_ime_field_h = 32;

    // SaveData fixed dialog card
    static constexpr uint16_t k_sd_card_w = 760;
    static constexpr uint16_t k_sd_card_h = 440;

    // Trophy setup dialog card
    static constexpr uint16_t k_trophy_card_w = 764;
    static constexpr uint16_t k_trophy_card_h = 440;

    // Message dialog buttons
    static constexpr uint16_t k_btn_w = 200;
    static constexpr uint16_t k_btn_h = 35;
    static constexpr uint16_t k_btn_radius = 8;

    // SaveData dialog buttons
    static constexpr uint16_t k_sd_btn_w = 320;
    static constexpr uint16_t k_sd_btn_h = 46;
    static constexpr uint16_t k_sd_btn_radius = 10;
    static constexpr uint16_t k_sd_btn_gap = 40;

    // Save slot / thumbnail layout
    static constexpr uint16_t k_slot_h = 90;
    static constexpr uint16_t k_slot_icon_w = 160;
    static constexpr uint16_t k_slot_icon_h = 90;
    static constexpr uint16_t k_progress_h = 6;
    static constexpr uint16_t k_font_size = 16;
    static constexpr uint16_t k_font_size_small = 13;
    static constexpr uint16_t k_font_size_title = 18;
    static constexpr int k_max_visible_slots = 5;

    static constexpr color4f k_color_bg_dim{ 0.f, 0.f, 0.f, 0.47f };
    static constexpr color4f k_color_card_bg{ 0.145f, 0.157f, 0.176f, 0.95f };
    static constexpr color4f k_color_btn_normal{ 1.f, 1.f, 1.f, 0.078f };
    static constexpr color4f k_color_text{ 1.f, 1.f, 1.f, 1.f };
    static constexpr color4f k_color_text_dim{ 1.f, 1.f, 1.f, 0.7f };
    static constexpr color4f k_color_progress_bg{ 0.32f, 0.32f, 0.36f, 0.3f };
    static constexpr color4f k_color_progress_fill{ 0.553f, 0.706f, 0.325f, 1.f };
    static constexpr color4f k_color_separator{ 1.f, 1.f, 1.f, 0.24f };
    static constexpr color4f k_color_slot_hover{ 1.f, 1.f, 1.f, 0.12f };

    static constexpr color4f k_color_focus_ring{ 0.180f, 0.765f, 0.878f, 1.f };
    static constexpr uint8_t k_focus_ring_thickness = 2;

    static constexpr float k_gloss_height = 0.80f;
    static constexpr float k_gloss_feather = 0.20f;
    static constexpr float k_gloss_opacity = 0.50f;

    static constexpr float k_btn_gloss_height = 0.54f;
    static constexpr float k_btn_gloss_curve_lift = 0.13f;
    static constexpr float k_btn_gloss_opacity = 0.87f;
    static constexpr float k_btn_gloss_bottom_opacity = 0.18f;

    static constexpr int16_t k_text_voffset = -3;

    static constexpr uint16_t k_ime_cursor_width = 2;
    static constexpr uint16_t k_ime_cursor_vpad = 5;
    static constexpr uint16_t k_ime_preedit_underline_thickness = 2;
    static constexpr uint16_t k_ime_preedit_underline_vpad = 4;
};

} // namespace overlay
