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

#include "private.h"

#include <bgm_player/functions.h>
#include <config/state.h>
#include <gui-imgui/functions.h>
#include <io/state.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <vita_shell/functions.h>

#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

std::string get_theme_title_from_buffer(const vfs::FileBuffer &buffer) {
    pugi::xml_document doc;
    if (doc.load_buffer(buffer.data(), buffer.size()))
        return doc.child("theme").child("InfomationProperty").child("m_title").child("m_default").text().as_string();
    return "Internal error";
}

static constexpr ImVec2 background_size(960.f, 512.f), background_preview_size(226.f, 128.f);

bool init_user_background(GuiState &gui, EmuEnvState &emuenv, const std::string &background_path) {
    auto background_path_path = fs_utils::utf8_to_path(background_path);
    if (!fs::exists(background_path_path)) {
        LOG_WARN("Background doesn't exist: {}.", background_path_path);
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;

    FILE *f = FOPEN(background_path_path.c_str(), "rb");
    if (!f) {
        LOG_ERROR("Unable to open background: {}.", background_path_path);
        return false;
    }
    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);
    fclose(f);

    if (!data) {
        LOG_ERROR("Invalid or corrupted background: {}.", background_path_path);
        return false;
    }

    gui.user_backgrounds[background_path] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    // Resize background to fit screen size if needed (keep aspect ratio)
    auto &user_background = gui.user_backgrounds_infos[background_path];

    // Resize for preview
    const auto prew_ratio = std::min(background_preview_size.x / static_cast<float>(width), background_preview_size.y / static_cast<float>(height));
    user_background.prev_size = ImVec2(width * prew_ratio, height * prew_ratio);

    // Resize for home screen
    const auto ratio = std::min(background_size.x / static_cast<float>(width), background_size.y / static_cast<float>(height));
    user_background.size = ImVec2(width * ratio, height * ratio);

    // Center background on screen (keep aspect ratio)
    user_background.prev_pos = ImVec2((background_preview_size.x / 2.f) - (user_background.prev_size.x / 2.f), (background_preview_size.y / 2.f) - (user_background.prev_size.y / 2.f));
    user_background.pos = ImVec2((background_size.x / 2.f) - (user_background.size.x / 2.f), (background_size.y / 2.f) - (user_background.size.y / 2.f));

    return gui.user_backgrounds.contains(background_path);
}

bool init_user_backgrounds(GuiState &gui, EmuEnvState &emuenv) {
    gui.user_backgrounds.clear();
    gui.user_backgrounds_infos.clear();
    gui.current_user_bg = 0;
    for (const auto &bg : emuenv.app.user_list.users[emuenv.io.user_id].backgrounds)
        init_user_background(gui, emuenv, bg);

    return !gui.user_backgrounds.empty();
}

enum DateLayout {
    LEFT_DOWN,
    LEFT_UP,
    RIGHT_DOWN,
};

struct StartParam {
    ImU32 date_color = 0xFFFFFFFF;
    DateLayout date_layout = DateLayout::LEFT_DOWN;
    ImVec2 date_pos = ImVec2(900.f, 186.f);
    ImVec2 clock_pos = ImVec2(880.f, 146.f);
};

static ImU32 to_imgui_color(const vita_shell::Color &color) {
    return IM_COL32(color.r, color.g, color.b, color.a);
}

static ImVec4 to_imgui_color_float(const vita_shell::Color &color) {
    return ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
}

static const vita_shell::ThemeState &get_theme_state(EmuEnvState &emuenv,
    const std::string &content_id, bool reload = false) {
    const auto &theme = vita_shell::get_theme_state();
    if (reload || !theme.loaded || theme.content_id != content_id)
        vita_shell::load_theme(emuenv.vita_fs_path, emuenv.static_assets_path, content_id);
    return vita_shell::get_theme_state();
}

static StartParam start_param;
void init_theme_start_background(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    const auto &theme = get_theme_state(emuenv, content_id);
    start_param = {};
    gui.start_background = {};
    start_param.date_color = to_imgui_color(theme.start_date_color.value_or(vita_shell::Color{}));
    start_param.date_layout = static_cast<DateLayout>(theme.start_date_layout);

    switch (start_param.date_layout) {
    case DateLayout::LEFT_DOWN:
        break;
    case DateLayout::LEFT_UP:
        start_param.date_pos.y = 468.f;
        start_param.clock_pos.y = 426.f;
        break;
    case DateLayout::RIGHT_DOWN:
        start_param.date_pos.x = 50.f;
        start_param.clock_pos.x = 50.f;
        break;
    default:
        LOG_WARN("Date layout for this theme is unknown : {}", static_cast<uint32_t>(start_param.date_layout));
        break;
    }

    if (theme.start_screen_background.empty()) {
        LOG_WARN("Start background not found for theme '{}'.", content_id);
        return;
    }

    std::vector<uint8_t> buffer;
    if (!fs_utils::read_data(theme.start_screen_background, buffer)) {
        LOG_WARN("Could not read start background '{}'.", theme.start_screen_background);
        return;
    }
    int32_t width = 0;
    int32_t height = 0;
    stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid start background '{}' for content id: {}.", theme.start_screen_background, content_id);
        return;
    }

    gui.start_background = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
}

bool init_user_start_background(GuiState &gui, const std::string &image_path) {
    const fs::path image_path_path = fs_utils::utf8_to_path(image_path);
    if (!fs::exists(image_path_path)) {
        LOG_WARN("Image doesn't exist: {}.", image_path);
        return false;
    }

    start_param = {};
    gui.start_background = {};

    int32_t width = 0;
    int32_t height = 0;

    FILE *f = FOPEN(image_path_path.c_str(), "rb");
    if (!f) {
        LOG_ERROR("Unable to open image: {}.", image_path);
        return false;
    }
    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);
    fclose(f);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", image_path);
        return false;
    }

    gui.start_background = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.start_background;
}

bool init_theme(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    const auto &theme = get_theme_state(emuenv, content_id, true);

    gui.app_selector.sys_apps_icon.clear();
    gui.current_theme_bg = 0;
    gui.information_bar_color.bar = to_imgui_color(theme.information_bar.bar_color.value_or(vita_shell::Color{ 0, 0, 0, 255 }));
    gui.information_bar_color.indicator = to_imgui_color(theme.information_bar.indicator_color.value_or(vita_shell::Color{}));
    gui.information_bar_color.notice_font = to_imgui_color(theme.information_bar.notice_font_color.value_or(vita_shell::Color{}));
    gui.theme_backgrounds.clear();
    gui.theme_backgrounds_font_color.clear();
    gui.theme_information_bar_notice.clear();

    bgm_player::set_current_bgm_path({ theme.bgm.device, theme.bgm.device_path.generic_string() });
    bgm_player::init_bgm(emuenv.vita_fs_path, emuenv.app.user_list.users[emuenv.io.user_id].system_music);

    for (const auto &[title_id, icon_path] : theme.system_app_icons) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        if (!fs_utils::read_data(icon_path, buffer)) {
            buffer = init_default_icon(gui, emuenv);
            if (buffer.empty()) {
                LOG_WARN("No icon found for system app '{}' in theme '{}'.", title_id, content_id);
                continue;
            }
        }

        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid system icon '{}' for theme '{}'.", icon_path, content_id);
            continue;
        }

        gui.app_selector.sys_apps_icon[title_id] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    for (const auto &background : theme.backgrounds) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        if (!fs_utils::read_data(background.path, buffer)) {
            LOG_WARN("Background '{}' could not be read for theme '{}'.", background.path, content_id);
            continue;
        }

        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid background '{}' for theme '{}'.", background.path, content_id);
            continue;
        }

        gui.theme_backgrounds.emplace_back(gui.imgui_state.get(), data, width, height);
        gui.theme_backgrounds_font_color.push_back(to_imgui_color_float(background.text_color.value_or(theme.text_color)));
        stbi_image_free(data);
    }

    constexpr int cx = 90;
    constexpr int cy = 20;
    constexpr float radius = 64.f;
    constexpr float feather = 1.5f;
    for (const auto &[notice_type, icon_path] : theme.information_bar.notice_icons) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;
        if (!fs_utils::read_data(icon_path, buffer)) {
            LOG_WARN("Notice icon '{}' could not be read for theme '{}'.", icon_path, content_id);
            continue;
        }

        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid notice icon '{}' for theme '{}'.", icon_path, content_id);
            continue;
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int dx = x - cx;
                const int dy = y - cy;
                const float dist = sqrtf(dx * dx + dy * dy);

                const int idx = (y * width + x) * 4;
                if (dist >= radius)
                    data[idx + 3] = 0;
                else if (dist >= (radius - feather)) {
                    float t = (radius - dist) / feather;
                    t = std::clamp(t, 0.0f, 1.0f);
                    data[idx + 3] = static_cast<unsigned char>(t * 255.0f);
                }
            }
        }

        const auto gui_notice_type = notice_type == vita_shell::NoticeIcon::NoNotice ? NoticeIcon::NO : NoticeIcon::NEW;
        gui.theme_information_bar_notice[gui_notice_type] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    return !gui.theme_backgrounds.empty();
}

void draw_background(GuiState &gui, EmuEnvState &emuenv) {
    const auto &user = emuenv.app.user_list.users[emuenv.io.user_id];
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_POS_MAX(viewport.logical_x + viewport.logical_width, viewport.logical_y + viewport.logical_height);
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto INFO_BAR_HEIGHT = 32.f * SCALE.y;
    const auto HALF_INFO_BAR_HEIGHT = INFO_BAR_HEIGHT / 2.f;

    const auto is_user_background = !gui.user_backgrounds.empty() && !user.use_theme_bg;
    const auto is_theme_background = !gui.theme_backgrounds.empty() && user.use_theme_bg;

    auto draw_list = ImGui::GetBackgroundDrawList();

    // Draw black background for full screens
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize, IM_COL32(0.f, 0.f, 0.f, 255.f));

    // Draw blue background for home screen and live area screen only
    if (gui.vita_area.home_screen || gui.vita_area.live_area_screen)
        draw_list->AddRectFilled(VIEWPORT_POS, VIEWPORT_POS_MAX, IM_COL32(11.f, 90.f, 252.f, 160.f), 0.f, ImDrawFlags_RoundCornersAll);

    // Draw background image for home screen and app loading screen only
    if (!gui.vita_area.live_area_screen && (is_theme_background || is_user_background)) {
        const auto MARGIN_HEIGHT = gui.vita_area.home_screen ? INFO_BAR_HEIGHT : HALF_INFO_BAR_HEIGHT;
        ImVec2 background_pos_min(VIEWPORT_POS.x, VIEWPORT_POS.y + MARGIN_HEIGHT);
        ImVec2 background_pos_max(background_pos_min.x + VIEWPORT_SIZE.x, background_pos_min.y + VIEWPORT_SIZE.y - MARGIN_HEIGHT);

        // Draw background image
        std::string user_bg_path;
        if (is_user_background) {
            user_bg_path = user.backgrounds[gui.current_user_bg];
            const auto &user_background_infos = gui.user_backgrounds_infos[user_bg_path];
            background_pos_min = ImVec2(background_pos_min.x + (user_background_infos.pos.x * SCALE.x), background_pos_min.y + (user_background_infos.pos.y * SCALE.y));
            background_pos_max = ImVec2(background_pos_min.x + (user_background_infos.size.x * SCALE.x), background_pos_min.y + (user_background_infos.size.y * SCALE.y));
        }

        const auto &background = is_user_background ? gui.user_backgrounds[user_bg_path] : gui.theme_backgrounds[gui.current_theme_bg];

        draw_list->AddImage(background, background_pos_min, background_pos_max);
    }
}

void close_start_screen(GuiState &gui, EmuEnvState &emuenv) {
    gui.vita_area.start_screen = false;
    bgm_player::switch_bgm_state(false);
    gui.vita_area.home_screen = true;
    if (!emuenv.cfg.show_info_bar)
        gui.vita_area.information_bar = false;
}

void draw_start_screen(GuiState &gui, EmuEnvState &emuenv) {
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto INFO_BAR_HEIGHT = 32.f * SCALE.y;

    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFO_BAR_HEIGHT);
    const ImVec2 WINDOW_POS_MAX(WINDOW_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFO_BAR_HEIGHT);

    const auto draw_list = ImGui::GetBackgroundDrawList();

    // Draw black background for full screens
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize, IM_COL32(0.f, 0.f, 0.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##start_screen", &gui.vita_area.start_screen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (gui.start_background)
        draw_list->AddImage(gui.start_background, WINDOW_POS, WINDOW_POS_MAX);
    else
        draw_list->AddRectFilled(WINDOW_POS, WINDOW_POS_MAX, IM_COL32(43.f, 44.f, 47.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    draw_list->AddRect(ImVec2(WINDOW_POS.x + (32.f * SCALE.x), WINDOW_POS.y + (32.f * SCALE.y)), ImVec2(WINDOW_POS_MAX.x - (32.f * SCALE.x), WINDOW_POS_MAX.y - (32.f * SCALE.y)), IM_COL32(255.f, 255.f, 255.f, 255.f), 20.0f * SCALE.x, ImDrawFlags_RoundCornersAll);

    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    tm local = {};

    SAFE_LOCALTIME(&tt, &local);

    ImGui::PushFont(gui.vita_font[gui.current_font_level]);
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * SCALE.x);
    const auto SCAL_PIX_DATE_FONT = 34.f / 28.f;
    const auto DATE_FONT_SIZE = (34.f * SCALE.x) * DEFAULT_FONT_SCALE;
    const auto SCAL_DATE_FONT_SIZE = DATE_FONT_SIZE / ImGui::GetFontSize();

    auto DATE_TIME = get_date_time(gui, emuenv, local);
    const auto &DATE_STR = DATE_TIME[DateTime::DATE_DETAIL];
    const auto CALC_DATE_SIZE = ImGui::CalcTextSize(DATE_STR.c_str());
    const auto DATE_INIT_SCALE = ImVec2(start_param.date_pos.x * SCALE.x, start_param.date_pos.y * SCALE.y);
    const auto DATE_SIZE = ImVec2(CALC_DATE_SIZE.x * SCAL_DATE_FONT_SIZE, CALC_DATE_SIZE.y * SCAL_DATE_FONT_SIZE * SCAL_PIX_DATE_FONT);
    const auto DATE_POS = ImVec2(WINDOW_POS_MAX.x - (start_param.date_layout == DateLayout::RIGHT_DOWN ? DATE_INIT_SCALE.x + (DATE_SIZE.x * RES_SCALE.x) : DATE_INIT_SCALE.x), WINDOW_POS_MAX.y - DATE_INIT_SCALE.y);
    draw_list->AddText(gui.vita_font[gui.current_font_level], DATE_FONT_SIZE * RES_SCALE.x, DATE_POS, start_param.date_color, DATE_STR.c_str());
    ImGui::PopFont();

    ImGui::PushFont(gui.large_font[gui.current_font_level]);
    const auto DEFAULT_LARGE_FONT_SCALE = ImGui::GetFontSize() / (116.f * SCALE.y);
    const auto LARGE_FONT_SIZE = (116.f * SCALE.y) * DEFAULT_FONT_SCALE;
    const auto PIX_LARGE_FONT_SCALE = (96.f * SCALE.y) / ImGui::GetFontSize();

    const auto &CLOCK_STR = DATE_TIME[DateTime::CLOCK];
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(CLOCK_STR.c_str());
    const auto CLOCK_SIZE = ImVec2(CALC_CLOCK_SIZE.x * RES_SCALE.x, CALC_CLOCK_SIZE.y * PIX_LARGE_FONT_SCALE);

    const auto &DAY_MOMENT_STR = DATE_TIME[DateTime::DAY_MOMENT];
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DAY_MOMENT_STR.c_str());
    const auto DAY_MOMENT_LARGE_FONT_SIZE = (56.f * SCALE.x) * DEFAULT_LARGE_FONT_SCALE;
    const auto LARGE_FONT_DAY_MOMENT_SCALE = DAY_MOMENT_LARGE_FONT_SIZE / ImGui::GetFontSize();
    const auto DAY_MOMENT_SIZE = is_12_hour_format ? ImVec2((CALC_DAY_MOMENT_SIZE.x * LARGE_FONT_DAY_MOMENT_SCALE) * RES_SCALE.x, (CALC_DAY_MOMENT_SIZE.y * LARGE_FONT_DAY_MOMENT_SCALE) * PIX_LARGE_FONT_SCALE) : ImVec2(0.f, 0.f);

    auto CLOCK_POS = ImVec2(WINDOW_POS_MAX.x - (start_param.clock_pos.x * SCALE.x), WINDOW_POS_MAX.y - (start_param.clock_pos.y * SCALE.y));
    if (start_param.date_layout == DateLayout::RIGHT_DOWN)
        CLOCK_POS.x -= CLOCK_SIZE.x + DAY_MOMENT_SIZE.x;
    else if (string_utils::stoi_def(DATE_TIME[DateTime::HOUR], 0, "hour") < 10)
        CLOCK_POS.x += ImGui::CalcTextSize("0").x * RES_SCALE.x;

    draw_list->AddText(gui.large_font[gui.current_font_level], LARGE_FONT_SIZE * RES_SCALE.y, CLOCK_POS, start_param.date_color, CLOCK_STR.c_str());
    if (is_12_hour_format) {
        const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + CLOCK_SIZE.x + (6.f * SCALE.x), CLOCK_POS.y + (CLOCK_SIZE.y - DAY_MOMENT_SIZE.y));
        draw_list->AddText(gui.large_font[gui.current_font_level], DAY_MOMENT_LARGE_FONT_SIZE * RES_SCALE.y, DAY_MOMENT_POS, start_param.date_color, DAY_MOMENT_STR.c_str());
    }
    ImGui::PopFont();

    if (ImGui::IsWindowHovered(ImGuiFocusedFlags_RootWindow) && ImGui::IsMouseClicked(0))
        close_start_screen(gui, emuenv);

    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace gui
