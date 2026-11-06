// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/app_selector.h>

#include <app/functions.h>
#include <app/state.h>
#include <bgm_player/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <overlay/controls.h>
#include <overlay/element.h>
#include <overlay/font.h>
#include <renderer/state.h>
#include <sdl-frontend/functions.h>
#include <sdl/frame_host.h>
#include <sdl/sdl_title.h>
#include <sdl/sdl_window.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <vita_shell/functions.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <map>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sdl_frontend {

constexpr float BACKGROUND_TOP = 32.f;
constexpr float ICON_SIZE = 64.f;
constexpr float ICON_GUTTER = 8.f;
constexpr float TEXT_ROW_HEIGHT = 28.f;
constexpr float ROW_HEIGHT = ICON_SIZE + 8.f;
constexpr float MAX_ROW_HEIGHT = 120.f;
constexpr float SEARCH_HEIGHT = 28.f;
constexpr float SEARCH_WIDTH = 200.f;
constexpr float SEARCH_MIN_WIDTH = 100.f;
constexpr float SORT_MAX_WIDTH = 220.f;
constexpr float HEADER_CONTROL_GAP = 12.f;
constexpr float USER_LEFT = 145.f;
constexpr float USER_WIDTH = 110.f;
constexpr float REFRESH_LEFT = 267.f;
constexpr float REFRESH_WIDTH = 92.f;
constexpr float SORT_LEFT = REFRESH_LEFT + REFRESH_WIDTH + HEADER_CONTROL_GAP;
constexpr float SORT_OPTION_HEIGHT = 28.f;

constexpr std::array<app::UserSortType, 5> SORT_TYPES = {
    app::TITLE, app::TITLE_ID, app::APP_VER, app::CATEGORY, app::LAST_TIME
};

struct HeaderLayout {
    float refresh_x;
    float refresh_width;
    float sort_x;
    float sort_width;
    float search_x;
    float search_width;
};

static HeaderLayout get_header_layout(float logical_width, float clock_width, float scale_x) {
    const float search_right = logical_width - 20.f * scale_x - HEADER_CONTROL_GAP * scale_x - clock_width;
    const float search_width = std::min(SEARCH_WIDTH * scale_x,
        std::max(SEARCH_MIN_WIDTH * scale_x, search_right - (SORT_LEFT + SORT_MAX_WIDTH + HEADER_CONTROL_GAP) * scale_x));
    const float search_x = search_right - search_width;
    float sort_width = std::min(SORT_MAX_WIDTH * scale_x, std::max(0.f, search_x - (SORT_LEFT + HEADER_CONTROL_GAP) * scale_x));
    float sort_x = search_x - HEADER_CONTROL_GAP * scale_x - sort_width;
    float refresh_x = sort_x - HEADER_CONTROL_GAP * scale_x - REFRESH_WIDTH * scale_x;
    if (refresh_x < REFRESH_LEFT * scale_x) {
        sort_width = std::max(0.f, sort_width - (REFRESH_LEFT * scale_x - refresh_x));
        sort_x = search_x - HEADER_CONTROL_GAP * scale_x - sort_width;
        refresh_x = sort_x - HEADER_CONTROL_GAP * scale_x - REFRESH_WIDTH * scale_x;
    }
    return {
        refresh_x,
        REFRESH_WIDTH * scale_x,
        sort_x,
        sort_width,
        search_x,
        search_width
    };
}

struct HeaderVerticalLayout {
    float control_scale;
    float control_top;
    float control_height;
    float sort_menu_top;
};

static HeaderVerticalLayout get_header_vertical_layout(float header_height) {
    const float control_scale = header_height / BACKGROUND_TOP;
    const float control_height = SEARCH_HEIGHT * control_scale;
    const float control_top = (header_height - control_height) / 2.f;
    return { control_scale, control_top, control_height, control_top + control_height + 4.f * control_scale };
}

static std::string to_ascii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) {
            result.push_back(static_cast<char>(lead));
            ++i;
            continue;
        }

        size_t sequence_length = 1;
        if ((lead & 0xE0) == 0xC0)
            sequence_length = 2;
        else if ((lead & 0xF0) == 0xE0)
            sequence_length = 3;
        else if ((lead & 0xF8) == 0xF0)
            sequence_length = 4;
        result.push_back('?');
        i += std::min(sequence_length, text.size() - i);
    }
    return result;
}

static std::string app_label(const app::AppEntry &app) {
    std::string label = app.title.empty() ? app.title_id : app.title + " [" + app.title_id + "]";
    return label + " (v" + app.app_ver + ")";
}

static const char *sort_type_name(app::UserSortType type) {
    switch (type) {
    case app::APP_VER: return "Version";
    case app::CATEGORY: return "Category";
    case app::LAST_TIME: return "Last played";
    case app::TITLE: return "Title";
    case app::TITLE_ID: return "Title ID";
    case app::COMPAT: return "Compatibility";
    }
    return "Title";
}

static std::string lowercase_ascii(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return character < 0x80 ? static_cast<char>(std::tolower(character)) : static_cast<char>(character);
    });
    return result;
}

static std::vector<size_t> find_matching_apps(const std::vector<app::AppEntry> &apps, std::string_view query) {
    std::vector<size_t> matches;
    const auto normalized_query = lowercase_ascii(query);
    for (size_t index = 0; index < apps.size(); ++index) {
        if (normalized_query.empty() || lowercase_ascii(app_label(apps[index])).find(normalized_query) != std::string::npos)
            matches.push_back(index);
    }
    return matches;
}

static void remove_last_utf8_character(std::string &text) {
    if (text.empty())
        return;

    size_t character_start = text.size() - 1;
    while (character_start > 0 && (static_cast<unsigned char>(text[character_start]) & 0xC0) == 0x80)
        --character_start;
    text.resize(character_start);
}

static float get_row_height(float list_height, size_t app_count, float scale_y) {
    const float row_height = ROW_HEIGHT * scale_y;
    const auto full_rows = static_cast<size_t>(list_height / row_height);
    if (app_count > 0 && app_count <= full_rows)
        return std::min(MAX_ROW_HEIGHT * scale_y, std::max(row_height, list_height / static_cast<float>(app_count)));
    return row_height;
}

static size_t row_at(float x, float y, size_t first_row, size_t visible_rows,
    size_t app_count, float list_left, float list_right, float list_top, float list_bottom, float scale_y) {
    if (x < list_left || x >= list_right || y < list_top || y >= list_bottom)
        return app_count;
    const auto row_height = get_row_height(list_bottom - list_top, app_count, scale_y);
    const auto row = static_cast<size_t>((y - list_top) / row_height);
    if (row >= visible_rows || first_row + row >= app_count)
        return app_count;
    return first_row + row;
}

static size_t get_visible_rows(float list_top, float list_bottom, size_t app_count, float scale_y) {
    const float list_height = std::max(0.f, list_bottom - list_top);
    const float row_height = get_row_height(list_height, app_count, scale_y);
    return static_cast<size_t>(std::max(1.f, std::floor(list_height / row_height)));
}

overlay::font *selector_font{};
std::vector<SDL_Texture *> font_textures;
bool font_unavailable{};

static void destroy_font() {
    for (auto *texture : font_textures)
        SDL_DestroyTexture(texture);
    font_textures.clear();
    selector_font = nullptr;
    font_unavailable = false;
}

static void initialize_font(EmuEnvState &emuenv, SDL_Window *window) {
    destroy_font();
    sdl::FrameHost frame_host(window, nullptr);
    overlay::fontmgr::set_system_lang(emuenv.cfg.sys_lang);
    overlay::fontmgr::set_system_font_dirs(frame_host.font_dirs());

    auto firmware_font_dir = fs_utils::path_to_utf8(emuenv.vita_fs_path / "sa0" / "data" / "font" / "pvf");
    if (!firmware_font_dir.empty() && firmware_font_dir.back() != '/' && firmware_font_dir.back() != '\\')
        firmware_font_dir += '/';
    overlay::fontmgr::set_firmware_font_dir(firmware_font_dir);
    selector_font = overlay::fontmgr::get("ltn4.pvf", 14, true);
}

static bool ensure_font_textures(SDL_Renderer *renderer) {
    const auto dimensions = selector_font->get_glyph_data_dimensions();
    if (dimensions.depth <= font_textures.size())
        return true;

    const auto &glyph_data = selector_font->get_glyph_data();
    const size_t page_size = static_cast<size_t>(dimensions.width) * dimensions.height;
    for (size_t page = font_textures.size(); page < dimensions.depth; ++page) {
        auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            static_cast<int>(dimensions.width), static_cast<int>(dimensions.height));
        if (!texture) {
            LOG_WARN_ONCE("Could not create SDL font texture: {}", SDL_GetError());
            font_unavailable = true;
            return false;
        }

        std::vector<uint8_t> rgba(page_size * 4);
        const auto *alpha = glyph_data.data() + page * page_size;
        for (size_t pixel = 0; pixel < page_size; ++pixel) {
            rgba[pixel * 4] = 255;
            rgba[pixel * 4 + 1] = 255;
            rgba[pixel * 4 + 2] = 255;
            rgba[pixel * 4 + 3] = alpha[pixel];
        }

        if (!SDL_UpdateTexture(texture, nullptr, rgba.data(), static_cast<int>(dimensions.width * 4))
            || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)) {
            LOG_WARN_ONCE("Could not upload SDL font texture: {}", SDL_GetError());
            SDL_DestroyTexture(texture);
            font_unavailable = true;
            return false;
        }
        font_textures.push_back(texture);
    }

    return true;
}

static bool draw_font(SDL_Renderer *renderer, CoordinateScale coordinate_scale, float x, float y,
    std::string_view text, SDL_Color color) {
    if (!selector_font || font_unavailable)
        return false;

    const auto unicode_text = overlay::utf8_to_u32string(text);
    const auto glyphs = selector_font->render_text(unicode_text.c_str());
    if (glyphs.empty() || !ensure_font_textures(renderer))
        return false;

    struct Batch {
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
    };
    std::vector<Batch> batches(font_textures.size());

    for (size_t i = 0; i + 3 < glyphs.size(); i += 4) {
        const auto page = static_cast<size_t>(std::floor(glyphs[i].w()));
        if (page >= batches.size())
            continue;

        auto &batch = batches[page];
        const int base_vertex = static_cast<int>(batch.vertices.size());
        for (size_t vertex_index = 0; vertex_index < 4; ++vertex_index) {
            const auto &glyph = glyphs[i + vertex_index];
            batch.vertices.push_back({ .position = {
                                           x + glyph.x() * coordinate_scale.x,
                                           y + (selector_font->get_size_px() + glyph.y()) * coordinate_scale.y },
                .color = { static_cast<float>(color.r) / 255.f, static_cast<float>(color.g) / 255.f, static_cast<float>(color.b) / 255.f, static_cast<float>(color.a) / 255.f },
                .tex_coord = { glyph.z(), glyph.w() - static_cast<float>(page) } });
        }
        batch.indices.insert(batch.indices.end(), { base_vertex, base_vertex + 1, base_vertex + 2, base_vertex + 1, base_vertex + 3, base_vertex + 2 });
    }

    for (size_t page = 0; page < batches.size(); ++page) {
        auto &batch = batches[page];
        if (batch.indices.empty())
            continue;
        if (!SDL_RenderGeometry(renderer, font_textures[page], batch.vertices.data(),
                static_cast<int>(batch.vertices.size()), batch.indices.data(), static_cast<int>(batch.indices.size()))) {
            LOG_WARN_ONCE("SDL font rendering failed: {}", SDL_GetError());
            font_unavailable = true;
            return false;
        }
    }

    return true;
}

static float font_centered_y(std::string_view text, float area_height) {
    if (!selector_font)
        return (area_height - 8.f) / 2.f;

    const auto unicode_text = overlay::utf8_to_u32string(text);
    const auto glyphs = selector_font->render_text(unicode_text.c_str());
    if (glyphs.empty())
        return (area_height - static_cast<float>(selector_font->get_size_px())) / 2.f;

    float min_y = glyphs.front().y();
    float max_y = min_y;
    for (const auto &glyph : glyphs) {
        min_y = std::min(min_y, glyph.y());
        max_y = std::max(max_y, glyph.y());
    }
    return area_height / 2.f - static_cast<float>(selector_font->get_size_px()) - (min_y + max_y) / 2.f;
}

struct ClockLayout {
    std::string text;
    float width;
};

static ClockLayout get_clock_layout(const EmuEnvState &emuenv) {
    const auto now = std::chrono::system_clock::now();
    const auto time_value = std::chrono::system_clock::to_time_t(now);
    tm local_time{};
    SAFE_LOCALTIME(&time_value, &local_time);
    const bool is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const int hour = is_12_hour_format && local_time.tm_hour == 0
        ? 12
        : is_12_hour_format && local_time.tm_hour > 12 ? local_time.tm_hour - 12
                                                       : local_time.tm_hour;
    std::string text = std::to_string(hour) + ":";
    if (local_time.tm_min < 10)
        text += '0';
    text += std::to_string(local_time.tm_min);
    if (is_12_hour_format)
        text += local_time.tm_hour >= 12 ? " PM" : " AM";

    const auto unicode_text = overlay::utf8_to_u32string(text);
    const float width = selector_font
        ? selector_font->get_char_offset(unicode_text.c_str(), unicode_text.size()).first
        : static_cast<float>(text.size()) * 8.f;
    return { text, width };
}

std::unordered_map<std::string, SDL_Texture *> app_icon_textures;

static void destroy_app_icons() {
    std::unordered_set<SDL_Texture *> destroyed_textures;
    for (const auto &entry : app_icon_textures) {
        if (entry.second && destroyed_textures.insert(entry.second).second)
            SDL_DestroyTexture(entry.second);
    }
    app_icon_textures.clear();
}

static SDL_Texture *get_app_icon(EmuEnvState &emuenv, SDL_Renderer *renderer, const std::string &path) {
    if (const auto found = app_icon_textures.find(path); found != app_icon_textures.end())
        return found->second;

    std::vector<uint8_t> bytes;
    fs::path icon_path = fs_utils::utf8_to_path(path);
    if (path.empty() || !fs_utils::read_data(icon_path, bytes)) {
        if (!path.empty())
            LOG_WARN("Could not read app icon at '{}'; trying a default icon.", path);

        const auto default_fw_icon = emuenv.vita_fs_path / "vs0/data/internal/livearea/default/sce_sys/icon0.png";
        icon_path = fs::exists(default_fw_icon)
            ? default_fw_icon
            : emuenv.static_assets_path / "data/image/icon.png";
        const auto fallback_key = fs_utils::path_to_utf8(icon_path);
        if (const auto found = app_icon_textures.find(fallback_key); found != app_icon_textures.end()) {
            app_icon_textures.emplace(path, found->second);
            return found->second;
        }
        bytes.clear();
        if (!fs_utils::read_data(icon_path, bytes)) {
            LOG_WARN("Could not read default app icon at '{}'.", icon_path);
            app_icon_textures.emplace(fallback_key, nullptr);
            app_icon_textures.emplace(path, nullptr);
            return nullptr;
        }
    }

    const auto source_key = fs_utils::path_to_utf8(icon_path);
    overlay::image_info image(bytes);
    SDL_Texture *texture = nullptr;
    if (!image.get_data() || image.w <= 0 || image.h <= 0) {
        LOG_WARN("Could not decode app icon at '{}'.", icon_path);
    } else {
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
        if (!texture) {
            LOG_WARN("Could not create texture for app icon '{}': {}", icon_path, SDL_GetError());
        } else if (!SDL_UpdateTexture(texture, nullptr, image.get_data(), image.w * 4)
            || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)
            || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
            LOG_WARN("Could not upload app icon '{}': {}", icon_path, SDL_GetError());
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    app_icon_textures.emplace(source_key, texture);
    if (source_key != path)
        app_icon_textures.emplace(path, texture);
    return texture;
}

static const vita_shell::ThemeState &get_theme_state(EmuEnvState &emuenv,
    const std::string &content_id, bool reload = false) {
    const auto &theme = vita_shell::get_theme_state();
    if (reload || !theme.loaded || theme.content_id != content_id)
        vita_shell::load_theme(emuenv.vita_fs_path, emuenv.static_assets_path, content_id);
    return vita_shell::get_theme_state();
}

static std::vector<fs::path> get_background_paths(EmuEnvState &emuenv) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return {};

    std::vector<fs::path> paths;
    if (!user->second.use_theme_bg) {
        paths.reserve(user->second.backgrounds.size());
        for (const auto &background : user->second.backgrounds)
            paths.emplace_back(fs_utils::utf8_to_path(background));
        return paths;
    }

    const auto &theme = get_theme_state(emuenv, user->second.theme_id);
    paths.reserve(theme.backgrounds.size());
    for (const auto &background : theme.backgrounds)
        paths.push_back(background.path);
    return paths;
}

static std::pair<std::string, std::string> get_theme_bgm_path(EmuEnvState &emuenv) {
    const std::pair<std::string, std::string> default_bgm{ "pd0", "data/systembgm/home.at9" };
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return default_bgm;

    const auto &theme = get_theme_state(emuenv, user->second.theme_id);
    return { theme.bgm.device, theme.bgm.device_path.generic_string() };
}

struct BackgroundTexture {
    SDL_Texture *texture;
    int width;
    int height;
};

std::vector<BackgroundTexture> background_textures;
static bool background_overlay_available = false;

struct ViewportArea {
    float x;
    float y;
    float width;
    float height;
    float header_height;
    CoordinateScale scale;
};

static ScrollbarLayout get_scrollbar_layout(const ViewportArea &area, float list_top, float list_bottom,
    size_t app_count, size_t visible_rows, size_t first_row) {
    const SDL_FRect track{
        area.x + area.width - 14.f * area.scale.x,
        list_top,
        8.f * area.scale.x,
        std::max(0.f, list_bottom - list_top)
    };
    const size_t max_first_index = app_count > visible_rows ? app_count - visible_rows : 0;
    if (max_first_index == 0 || track.h <= 0.f)
        return { track, {}, max_first_index };

    const float thumb_height = std::min(track.h,
        std::max(24.f * area.scale.y,
            track.h * static_cast<float>(visible_rows) / static_cast<float>(app_count)));
    const float scroll_fraction = static_cast<float>(std::min(first_row, max_first_index))
        / static_cast<float>(max_first_index);
    const SDL_FRect thumb{
        track.x,
        track.y + (track.h - thumb_height) * scroll_fraction,
        track.w,
        thumb_height
    };
    return { track, thumb, max_first_index };
}

static ViewportArea get_viewport_area() {
    const auto &viewport = sdl::get_viewport();
    const CoordinateScale scale{
        viewport.scale_x * viewport.manual_dpi_scale,
        viewport.scale_y * viewport.manual_dpi_scale
    };
    return {
        viewport.logical_x,
        viewport.logical_y,
        viewport.logical_width,
        viewport.logical_height,
        std::min(BACKGROUND_TOP * scale.y, viewport.logical_height),
        scale
    };
}

static void load_backgrounds(SDL_Renderer *renderer, const std::vector<fs::path> &paths) {
    for (const auto &path : paths) {
        std::vector<uint8_t> bytes;
        if (!fs_utils::read_data(path, bytes)) {
            LOG_WARN("Could not read app selector background '{}'.", path);
            continue;
        }

        overlay::image_info image(bytes);
        if (!image.get_data() || image.w <= 0 || image.h <= 0) {
            LOG_WARN("Could not decode app selector background '{}'.", path);
            continue;
        }

        auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
        if (!texture) {
            LOG_WARN("Could not create app selector background texture '{}': {}", path, SDL_GetError());
            continue;
        }
        if (!SDL_UpdateTexture(texture, nullptr, image.get_data(), image.w * 4)
            || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
            LOG_WARN("Could not upload app selector background '{}': {}", path, SDL_GetError());
            SDL_DestroyTexture(texture);
            continue;
        }
        background_textures.push_back({ texture, image.w, image.h });
    }
}

static void destroy_backgrounds() {
    for (const auto &background : background_textures) {
        if (background.texture)
            SDL_DestroyTexture(background.texture);
    }
    background_textures.clear();
}

static void draw_background(size_t index, SDL_Renderer *renderer, const ViewportArea &area) {
    constexpr SDL_Color background_color{ 11, 90, 252, 160 };
    SDL_FRect viewport_background{ area.x, area.y, area.width, area.height };
    SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g,
        background_color.b, background_overlay_available ? background_color.a : 255);
    if (!SDL_RenderFillRect(renderer, &viewport_background))
        LOG_WARN_ONCE("SDL app selector viewport background rendering failed: {}", SDL_GetError());

    if (background_textures.empty())
        return;

    const auto &background = background_textures[index % background_textures.size()];
    SDL_FRect destination{
        area.x,
        area.y,
        area.width,
        area.height
    };
    if (!SDL_RenderTexture(renderer, background.texture, nullptr, &destination))
        LOG_WARN_ONCE("SDL app selector background rendering failed: {}", SDL_GetError());
}

static void draw_app_icon(EmuEnvState &emuenv, SDL_Renderer *renderer, const app::AppEntry &app,
    CoordinateScale coordinate_scale, float x_offset, float y, float row_height) {
    if (auto *texture = get_app_icon(emuenv, renderer, app.icon_path)) {
        SDL_FRect destination{
            x_offset + 26.f * coordinate_scale.x,
            y + (row_height - ICON_SIZE * coordinate_scale.y) / 2.f,
            ICON_SIZE * coordinate_scale.x,
            ICON_SIZE * coordinate_scale.y
        };
        constexpr int circle_segments = 48;
        constexpr float pi = 3.14159265358979323846f;
        std::array<SDL_Vertex, circle_segments + 1> vertices{};
        std::array<int, circle_segments * 3> indices{};
        const float center_x = destination.x + destination.w / 2.f;
        const float center_y = destination.y + destination.h / 2.f;
        const float radius_x = destination.w / 2.f;
        const float radius_y = destination.h / 2.f;
        vertices[0] = {
            .position = { center_x, center_y },
            .color = { 1.f, 1.f, 1.f, 1.f },
            .tex_coord = { 0.5f, 0.5f }
        };
        for (int segment = 0; segment < circle_segments; ++segment) {
            const float angle = 2.f * pi * static_cast<float>(segment) / static_cast<float>(circle_segments);
            const float x = std::cos(angle);
            const float y_offset = std::sin(angle);
            vertices[segment + 1] = {
                .position = { center_x + radius_x * x, center_y + radius_y * y_offset },
                .color = { 1.f, 1.f, 1.f, 1.f },
                .tex_coord = { 0.5f + 0.5f * x, 0.5f + 0.5f * y_offset }
            };
            const int index = segment * 3;
            indices[index] = 0;
            indices[index + 1] = segment + 1;
            indices[index + 2] = (segment + 1) % circle_segments + 1;
        }
        if (!SDL_RenderGeometry(renderer, texture, vertices.data(), static_cast<int>(vertices.size()),
                indices.data(), static_cast<int>(indices.size())))
            LOG_WARN_ONCE("SDL app icon rendering failed: {}", SDL_GetError());
    }
}

static void draw_text(SDL_Renderer *renderer, CoordinateScale coordinate_scale, float x, float y,
    const char *text, SDL_Color color) {
    if (draw_font(renderer, coordinate_scale, x, y, text, color))
        return;

    const auto ascii_text = to_ascii(text);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(renderer, x, y, ascii_text.c_str());
}

static void draw_header_control(SDL_Renderer *renderer, const SDL_FRect &box, CoordinateScale scale, bool focused) {
    SDL_SetRenderDrawColor(renderer, focused ? 26 : 38, focused ? 94 : 41, focused ? 126 : 56, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, focused ? 105 : 82, focused ? 225 : 120, focused ? 255 : 190, 255);
    SDL_RenderRect(renderer, &box);
    if (focused) {
        SDL_FRect inner_box{
            box.x + scale.x,
            box.y + scale.y,
            box.w - 2.f * scale.x,
            box.h - 2.f * scale.y
        };
        SDL_RenderRect(renderer, &inner_box);
    }
}

static void draw_sort_direction_arrow(SDL_Renderer *renderer, CoordinateScale coordinate_scale, float x, float y, bool descending) {
    const float tip_y = y + (descending ? 7.f : 1.f) * coordinate_scale.y;
    const float base_y = y + (descending ? 1.f : 7.f) * coordinate_scale.y;
    const float center_x = x + 4.f * coordinate_scale.x;
    const float left_x = x;
    const float right_x = x + 8.f * coordinate_scale.x;
    if (!SDL_RenderLine(renderer, left_x, base_y, center_x, tip_y)
        || !SDL_RenderLine(renderer, center_x, tip_y, right_x, base_y)
        || !SDL_RenderLine(renderer, center_x, tip_y, center_x, base_y))
        LOG_WARN_ONCE("SDL app selector sort direction arrow rendering failed: {}", SDL_GetError());
}

enum class HeaderControl {
    None,
    User,
    Refresh,
    Sort,
    Search
};

struct AppSelectorState {
    std::string search_query;
    std::vector<app::AppEntry> apps;
    std::vector<size_t> matching_apps;
    size_t selected_index = 0;
    size_t first_row = 0;
    size_t sort_option_index = 0;
    HeaderControl hovered_header_control = HeaderControl::None;
    size_t current_background = 0;
    Uint64 last_background_change = 0;
    bool search_focused = false;
    bool search_editing = false;
    bool refresh_focused = false;
    bool sort_focused = false;
    bool user_focused = false;
    bool sort_menu_open = false;
    bool text_input_started = false;
    bool suppress_next_text_input = false;
    bool mouse_input_active = false;
    bool mouse_hovering_app = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset_y = 0.f;
    bool suppress_next_finger_down = false;
};

static AppSelectorState app_selector_state;
static SDL_Renderer *renderer = nullptr;
static float renderer_system_scale = 0.f;

static bool sync_renderer_scale() {
    const auto &viewport = sdl::get_viewport();
    if (viewport.window_width <= 0.f || viewport.window_height <= 0.f) {
        LOG_ERROR("SDL app selector has no valid shared viewport.");
        return false;
    }

    if (!renderer)
        return true;
    if (viewport.system_dpi_scale <= 0.f) {
        LOG_ERROR("SDL app selector has an invalid shared system DPI scale.");
        return false;
    }
    if (renderer_system_scale != viewport.system_dpi_scale) {
        if (!SDL_SetRenderScale(renderer, viewport.system_dpi_scale, viewport.system_dpi_scale)) {
            LOG_ERROR("SDL app selector renderer scaling setup failed: {}", SDL_GetError());
            return false;
        }
        renderer_system_scale = viewport.system_dpi_scale;
    }
    return true;
}

static bool event_to_render_coordinates(float window_x, float window_y, float &render_x, float &render_y) {
    if (!renderer || !SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &render_x, &render_y)) {
        LOG_WARN_ONCE("SDL app selector could not map window coordinates to its logical viewport: {}", SDL_GetError());
        return false;
    }
    return true;
}

static bool initialize_render_resources(EmuEnvState &emuenv, AppSelectorState &state, SDL_Window *window) {
    if (!window) {
        LOG_ERROR("SDL app selector received no window.");
        return false;
    }

    sdl::set_default_window_title(window);
    const char *renderer_name = emuenv.backend_renderer == renderer::Backend::Vulkan ? SDL_GPU_RENDERER : "opengl";
    renderer = SDL_CreateRenderer(window, renderer_name);
    if (!renderer) {
        LOG_ERROR("SDL app selector renderer creation failed: {}", SDL_GetError());
        return false;
    }
    if (!sync_renderer_scale()) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
        renderer_system_scale = 0.f;
        return false;
    }
    background_overlay_available = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (!background_overlay_available)
        LOG_WARN("SDL app selector background overlay blending is unavailable: {}", SDL_GetError());
    initialize_font(emuenv, window);
    destroy_app_icons();
    destroy_backgrounds();
    load_backgrounds(renderer, get_background_paths(emuenv));
    if (!background_textures.empty())
        state.current_background %= background_textures.size();
    state.last_background_change = SDL_GetTicks();
    return true;
}

static void release_render_resources() {
    destroy_backgrounds();
    destroy_app_icons();
    destroy_user_selector_icons();
    destroy_font();
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    renderer_system_scale = 0.f;
}

static void stop_text_input(AppSelectorState &state, SDL_Window *window) {
    if (!state.text_input_started)
        return;
    if (window && !SDL_StopTextInput(window))
        LOG_WARN("SDL app selector could not stop text input: {}", SDL_GetError());
    state.text_input_started = false;
}

static bool start_text_input(AppSelectorState &state, SDL_Window *window) {
    if (state.text_input_started)
        return true;
    if (!window || !SDL_StartTextInput(window)) {
        LOG_WARN("SDL app selector could not start text input: {}", SDL_GetError());
        return false;
    }
    state.text_input_started = true;
    return true;
}

static void set_search_editing(AppSelectorState &state, SDL_Window *window, bool editing) {
    if (editing == state.search_editing || !window)
        return;
    if (editing) {
        if (!start_text_input(state, window))
            return;
    } else {
        stop_text_input(state, window);
    }
    state.search_editing = editing;
}

static void move_selection(AppSelectorState &state, SDL_Window *window, int offset, bool focus_search_at_top = false) {
    if (offset < 0) {
        if (state.search_focused || state.refresh_focused || state.sort_focused || state.user_focused)
            return;
        if (focus_search_at_top && offset == -1 && state.selected_index == 0) {
            state.refresh_focused = true;
            return;
        }
    } else if (offset > 0) {
        if (state.refresh_focused) {
            state.refresh_focused = false;
            return;
        }
        if (state.sort_focused) {
            state.sort_focused = false;
            return;
        }
        if (state.search_focused) {
            state.search_focused = false;
            set_search_editing(state, window, false);
            return;
        }
        if (state.user_focused) {
            state.user_focused = false;
            return;
        }
    }
    if (state.matching_apps.empty())
        return;
    const auto last_index = static_cast<int64_t>(state.matching_apps.size() - 1);
    const auto next_index = std::clamp(static_cast<int64_t>(state.selected_index) + offset, int64_t{ 0 }, last_index);
    state.selected_index = static_cast<size_t>(next_index);
}

static void cancel_selection(AppSelectorState &state, SDL_Window *window, bool &running) {
    if (state.sort_menu_open) {
        state.sort_menu_open = false;
        return;
    }
    if (state.search_editing || state.search_focused || state.refresh_focused || state.sort_focused || state.user_focused || !state.search_query.empty()) {
        set_search_editing(state, window, false);
        state.search_focused = false;
        state.refresh_focused = false;
        state.sort_focused = false;
        state.user_focused = false;
        state.search_query.clear();
        state.matching_apps = find_matching_apps(state.apps, state.search_query);
        state.selected_index = 0;
        state.first_row = 0;
    } else {
        running = false;
    }
}

static void open_sort_menu(EmuEnvState &emuenv, AppSelectorState &state, SDL_Window *window) {
    set_search_editing(state, window, false);
    state.search_focused = false;
    state.refresh_focused = false;
    state.sort_focused = true;
    state.user_focused = false;
    state.sort_menu_open = true;
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user != emuenv.app.user_list.users.end()) {
        const auto current = std::find(SORT_TYPES.begin(), SORT_TYPES.end(), user->second.sort_apps_type);
        state.sort_option_index = current == SORT_TYPES.end() ? 0 : static_cast<size_t>(std::distance(SORT_TYPES.begin(), current));
    }
}

static void close_sort_menu(AppSelectorState &state) {
    state.sort_menu_open = false;
}

static void move_header_focus(AppSelectorState &state, SDL_Window *window, bool move_right) {
    if (state.search_editing)
        set_search_editing(state, window, false);

    if (move_right) {
        if (state.user_focused) {
            state.user_focused = false;
            state.refresh_focused = true;
        } else if (state.refresh_focused) {
            state.refresh_focused = false;
            state.sort_focused = true;
        } else if (state.sort_focused) {
            state.sort_focused = false;
            state.search_focused = true;
        } else if (!state.search_focused) {
            state.refresh_focused = true;
        }
    } else if (state.search_focused) {
        state.search_focused = false;
        state.sort_focused = true;
    } else if (state.sort_focused) {
        state.sort_focused = false;
        state.refresh_focused = true;
    } else if (state.refresh_focused) {
        state.refresh_focused = false;
        state.user_focused = true;
    }
}

static void apply_sort(EmuEnvState &emuenv, AppSelectorState &state) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return;

    const auto sort_type = user->second.sort_apps_type;
    const auto sort_state = user->second.sort_apps_state;
    const auto app_times = sort_type == app::LAST_TIME ? app::get_user_app_times(emuenv) : std::map<std::string, app::AppTime>{};
    std::sort(state.apps.begin(), state.apps.end(), [&](const app::AppEntry &lhs, const app::AppEntry &rhs) {
        if (sort_type == app::LAST_TIME) {
            const auto lhs_time = app_times.find(lhs.path);
            const auto rhs_time = app_times.find(rhs.path);
            const auto lhs_used = lhs_time == app_times.end() ? 0 : lhs_time->second.last_time_used;
            const auto rhs_used = rhs_time == app_times.end() ? 0 : rhs_time->second.last_time_used;
            if (lhs_used != rhs_used)
                return sort_state == app::ASCENDANT ? lhs_used > rhs_used : lhs_used < rhs_used;
        } else {
            std::string lhs_value;
            std::string rhs_value;
            switch (sort_type) {
            case app::APP_VER:
                lhs_value = lhs.app_ver;
                rhs_value = rhs.app_ver;
                break;
            case app::CATEGORY:
                lhs_value = lhs.category;
                rhs_value = rhs.category;
                break;
            case app::TITLE_ID:
                lhs_value = lhs.title_id;
                rhs_value = rhs.title_id;
                break;
            case app::COMPAT:
            case app::TITLE:
                lhs_value = string_utils::toupper(lhs.title);
                rhs_value = string_utils::toupper(rhs.title);
                break;
            case app::LAST_TIME: break;
            }
            if (lhs_value != rhs_value)
                return sort_state == app::DESCENDANT ? lhs_value > rhs_value : lhs_value < rhs_value;
        }
        if (lhs.title_id != rhs.title_id)
            return sort_state == app::DESCENDANT ? lhs.title_id > rhs.title_id : lhs.title_id < rhs.title_id;
        return sort_state == app::DESCENDANT ? lhs.path > rhs.path : lhs.path < rhs.path;
    });

    state.matching_apps = find_matching_apps(state.apps, state.search_query);
    state.selected_index = 0;
    state.first_row = 0;
}

static void refresh_apps(EmuEnvState &emuenv, AppSelectorState &state) {
    if (!app::scan_apps(emuenv)) {
        LOG_ERROR("Could not refresh the SDL app selector list.");
        return;
    }

    destroy_app_icons();
    state.apps = app::get_apps(emuenv);
    state.matching_apps = find_matching_apps(state.apps, state.search_query);
    state.selected_index = 0;
    state.first_row = 0;
    apply_sort(emuenv, state);
}

static void select_sort_type(EmuEnvState &emuenv, AppSelectorState &state, app::UserSortType sort_type) {
    auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return;
    if (user->second.sort_apps_type == sort_type)
        user->second.sort_apps_state = user->second.sort_apps_state == app::DESCENDANT ? app::ASCENDANT : app::DESCENDANT;
    else
        user->second.sort_apps_type = sort_type;
    app::save_user(emuenv, emuenv.io.user_id);
    apply_sort(emuenv, state);
    close_sort_menu(state);
    state.sort_focused = true;
}

static void cycle_sort_type(EmuEnvState &emuenv, AppSelectorState &state) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return;

    const auto current = std::find(SORT_TYPES.begin(), SORT_TYPES.end(), user->second.sort_apps_type);
    const auto next = current == SORT_TYPES.end() || std::next(current) == SORT_TYPES.end()
        ? SORT_TYPES.begin()
        : std::next(current);
    select_sort_type(emuenv, state, *next);
}

static void toggle_sort_direction(EmuEnvState &emuenv, AppSelectorState &state) {
    auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end())
        return;
    user->second.sort_apps_state = user->second.sort_apps_state == app::DESCENDANT ? app::ASCENDANT : app::DESCENDANT;
    app::save_user(emuenv, emuenv.io.user_id);
    apply_sort(emuenv, state);
}

static void open_user_selector(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window);

static void queue_app_launch(EmuEnvState &emuenv, const std::string &path) {
    app::request_in_process_launch(emuenv, AppLaunchRequest{ .app_path = path });
}

static void confirm_selection(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window) {
    auto &app_state = app_selector_state;
    if (app_state.user_focused) {
        open_user_selector(emuenv, state, window);
    } else if (app_state.refresh_focused) {
        refresh_apps(emuenv, app_state);
    } else if (app_state.sort_focused) {
        open_sort_menu(emuenv, app_state, window);
    } else if (app_state.search_focused) {
        if (app_state.search_editing) {
            set_search_editing(app_state, window, false);
            app_state.search_focused = false;
        } else {
            set_search_editing(app_state, window, true);
        }
    } else if (!app_state.matching_apps.empty()) {
        queue_app_launch(emuenv, app_state.apps[app_state.matching_apps[app_state.selected_index]].path);
    }
}

static void initialize_selector_bgm(EmuEnvState &emuenv) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end()) {
        LOG_ERROR("Cannot initialize SDL selector BGM for unknown user '{}'.", emuenv.io.user_id);
        return;
    }
    if (!user->second.system_music) {
        LOG_INFO("SDL selector BGM is disabled for user '{}'.", user->first);
        return;
    }

    bgm_player::init_bgm_player(emuenv.cfg.bgm_volume);
    const auto theme_bgm = get_theme_bgm_path(emuenv);
    LOG_INFO("Starting SDL selector BGM for user '{}' from {}:{} at volume {}%.",
        user->first, theme_bgm.first, theme_bgm.second, emuenv.cfg.bgm_volume);
    bgm_player::set_current_bgm_path(theme_bgm);
    if (bgm_player::init_bgm(emuenv.vita_fs_path, true))
        bgm_player::switch_bgm_state(false);
}

static void open_user_selector(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window) {
    auto &app_state = app_selector_state;
    set_search_editing(app_state, window, false);
    app_state.search_focused = false;
    app_state.refresh_focused = false;
    app_state.sort_focused = false;
    app_state.user_focused = false;
    app_state.sort_menu_open = false;
    begin_user_selector(emuenv);
    state.screen = FrontendScreen::UserSelector;
}

static void select_user(EmuEnvState &emuenv, FrontendState &state, const std::string &user_id) {
    auto &app_state = app_selector_state;
    const auto user = emuenv.app.user_list.users.find(user_id);
    if (user == emuenv.app.user_list.users.end())
        return;

    if (user->first != emuenv.io.user_id) {
        bgm_player::destroy_bgm_player();
        if (!app::activate_user(emuenv, user->first)) {
            LOG_ERROR("Could not activate SDL frontend user '{}'.", user->first);
            initialize_selector_bgm(emuenv);
            return;
        }
        emuenv.cfg.user_id = user->first;
        if (config::serialize_config(emuenv.cfg, emuenv.cfg.config_path) != Success)
            LOG_ERROR("Could not save selected SDL frontend user '{}'.", user->first);

        get_theme_state(emuenv, user->second.theme_id, true);
        apply_sort(emuenv, app_state);
        destroy_backgrounds();
        load_backgrounds(renderer, get_background_paths(emuenv));
        app_state.current_background = 0;
        app_state.last_background_change = SDL_GetTicks();
        initialize_selector_bgm(emuenv);
    }
    reset_user_selector();
    state.screen = FrontendScreen::AppSelector;
}

void handle_user_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window,
    const SDL_Event &event, uint32_t vita_button) {
    if (!sync_renderer_scale())
        return;
    const auto viewport_area = get_viewport_area();
    constexpr float logical_window_width = static_cast<float>(sdl::VITA_VIEWPORT_WIDTH);
    constexpr float logical_window_height = static_cast<float>(sdl::VITA_VIEWPORT_HEIGHT);
    if (const auto selected_user = handle_user_selector_event(emuenv, state, window, renderer,
            event, vita_button, viewport_area.scale, viewport_area.x, viewport_area.y,
            logical_window_width, logical_window_height))
        select_user(emuenv, state, *selected_user);
}

void render_user_selector(EmuEnvState &emuenv) {
    if (!renderer || !sync_renderer_scale())
        return;
    const auto viewport_area = get_viewport_area();
    render_user_selector(emuenv, renderer, viewport_area.scale, viewport_area.x, viewport_area.y,
        static_cast<float>(sdl::VITA_VIEWPORT_WIDTH), static_cast<float>(sdl::VITA_VIEWPORT_HEIGHT));
    if (!SDL_RenderPresent(renderer))
        LOG_WARN_ONCE("SDL user selector rendering failed: {}", SDL_GetError());
}

void draw_selector_text(SDL_Renderer *renderer, CoordinateScale coordinate_scale, float x, float y,
    const char *text, SDL_Color color) {
    draw_text(renderer, coordinate_scale, x, y, text, color);
}

bool initialize_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    auto &app_state = app_selector_state;
    bgm_player::destroy_bgm_player();
    stop_text_input(app_state, window);
    release_render_resources();
    app_state = {};
    app_state.apps = app::get_apps(emuenv);
    app_state.matching_apps = find_matching_apps(app_state.apps, app_state.search_query);
    apply_sort(emuenv, app_state);
    if (!sync_renderer_scale())
        return false;
    reset_user_selector();
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user != emuenv.app.user_list.users.end())
        get_theme_state(emuenv, user->second.theme_id, true);
    if (!initialize_render_resources(emuenv, app_state, window))
        return false;
    refresh_controllers(emuenv.ctrl, emuenv);
    initialize_selector_bgm(emuenv);
    return true;
}

void suspend_app_selector(SDL_Window *window) {
    app_selector_state.suppress_next_text_input = false;
    app_selector_state.suppress_next_finger_down = false;
    app_selector_state.scrollbar_dragging = false;
    stop_text_input(app_selector_state, window);
    release_render_resources();
}

bool resume_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    auto &app_state = app_selector_state;
    if (!renderer && !initialize_render_resources(emuenv, app_state, window))
        return false;
    if (app_state.search_editing && !start_text_input(app_state, window))
        app_state.search_editing = false;
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user != emuenv.app.user_list.users.end()) {
        if (user->second.sort_apps_type == app::LAST_TIME)
            apply_sort(emuenv, app_state);
        if (user->second.system_music)
            bgm_player::switch_bgm_state(false);
    } else {
        LOG_ERROR("Cannot resume SDL app selector for unknown user '{}'.", emuenv.io.user_id);
        return false;
    }
    return true;
}

void handle_app_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window, const SDL_Event &event,
    uint32_t vita_button, bool &running) {
    auto &app_state = app_selector_state;
    auto &search_query = app_state.search_query;
    auto &apps = app_state.apps;
    auto &matching_apps = app_state.matching_apps;
    auto &selected_index = app_state.selected_index;
    auto &first_row = app_state.first_row;
    auto &sort_option_index = app_state.sort_option_index;
    auto &hovered_header_control = app_state.hovered_header_control;
    auto &search_focused = app_state.search_focused;
    auto &search_editing = app_state.search_editing;
    auto &refresh_focused = app_state.refresh_focused;
    auto &sort_focused = app_state.sort_focused;
    auto &user_focused = app_state.user_focused;
    auto &sort_menu_open = app_state.sort_menu_open;
    auto &suppress_next_text_input = app_state.suppress_next_text_input;
    auto &mouse_input_active = app_state.mouse_input_active;
    auto &mouse_hovering_app = app_state.mouse_hovering_app;
    auto &scrollbar_dragging = app_state.scrollbar_dragging;
    auto &scrollbar_drag_offset_y = app_state.scrollbar_drag_offset_y;
    auto &suppress_next_finger_down = app_state.suppress_next_finger_down;
    if (!running)
        return;
    if (!sync_renderer_scale())
        return;
    if (event.type == SDL_EVENT_DROP_COMPLETE) {
        refresh_apps(emuenv, app_state);
        return;
    }
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    const auto viewport_area = get_viewport_area();
    const float list_top = viewport_area.y + viewport_area.header_height;
    const float list_bottom = viewport_area.y + viewport_area.height;
    const auto header_vertical = get_header_vertical_layout(viewport_area.header_height);
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        mouse_input_active = false;
        mouse_hovering_app = false;
        scrollbar_dragging = false;
        hovered_header_control = HeaderControl::None;
        const bool navigation_repeat = event.key.key == SDLK_UP || event.key.key == SDLK_DOWN
            || event.key.key == SDLK_PAGEUP || event.key.key == SDLK_PAGEDOWN
            || vita_button == SCE_CTRL_UP || vita_button == SCE_CTRL_DOWN;
        if (event.key.repeat && !navigation_repeat)
            break;
        if (event.key.key == SDLK_ESCAPE) {
            cancel_selection(app_state, window, running);
            break;
        }
        if (event.key.key == SDLK_BACKSPACE && search_editing) {
            if (!search_query.empty()) {
                remove_last_utf8_character(search_query);
                matching_apps = find_matching_apps(apps, search_query);
                selected_index = 0;
                first_row = 0;
            }
            break;
        }
        if (sort_menu_open) {
            if (vita_button == SCE_CTRL_UP || event.key.key == SDLK_UP) {
                if (sort_option_index > 0)
                    --sort_option_index;
            } else if (vita_button == SCE_CTRL_DOWN || event.key.key == SDLK_DOWN) {
                sort_option_index = std::min(sort_option_index + 1, SORT_TYPES.size() - 1);
            } else if (vita_button == SCE_CTRL_CROSS || event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                select_sort_type(emuenv, app_state, SORT_TYPES[sort_option_index]);
            } else if (vita_button == SCE_CTRL_CIRCLE) {
                close_sort_menu(app_state);
            }
            break;
        }
        if (vita_button == SCE_CTRL_TRIANGLE) {
            cycle_sort_type(emuenv, app_state);
            break;
        }
        if (vita_button == SCE_CTRL_SQUARE) {
            toggle_sort_direction(emuenv, app_state);
        } else if (vita_button == SCE_CTRL_RIGHT || event.key.key == SDLK_RIGHT) {
            move_header_focus(app_state, window, true);
        } else if (vita_button == SCE_CTRL_LEFT || event.key.key == SDLK_LEFT) {
            move_header_focus(app_state, window, false);
        } else if (search_editing) {
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
                confirm_selection(emuenv, state, window);
            break;
        } else if (event.key.key == SDLK_PAGEUP)
            move_selection(app_state, window, -static_cast<int>(get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y)));
        else if (event.key.key == SDLK_PAGEDOWN)
            move_selection(app_state, window, static_cast<int>(get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y)));
        else if (vita_button == SCE_CTRL_UP || event.key.key == SDLK_UP)
            move_selection(app_state, window, -1, true);
        else if (vita_button == SCE_CTRL_DOWN || event.key.key == SDLK_DOWN)
            move_selection(app_state, window, 1, true);
        else if (vita_button == SCE_CTRL_CROSS || event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
            if (search_focused && event.key.key >= SDLK_SPACE && event.key.key <= SDLK_Z)
                suppress_next_text_input = true;
            confirm_selection(emuenv, state, window);
        } else if (vita_button == SCE_CTRL_CIRCLE) {
            cancel_selection(app_state, window, running);
        }
        break;
    }
    case SDL_EVENT_TEXT_INPUT:
        if (suppress_next_text_input) {
            suppress_next_text_input = false;
        } else if (search_editing) {
            search_query += event.text.text;
            matching_apps = find_matching_apps(apps, search_query);
            selected_index = 0;
            first_row = 0;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        mouse_input_active = false;
        mouse_hovering_app = false;
        scrollbar_dragging = false;
        hovered_header_control = HeaderControl::None;
        if (sort_menu_open) {
            if (vita_button == SCE_CTRL_UP) {
                if (sort_option_index > 0)
                    --sort_option_index;
            } else if (vita_button == SCE_CTRL_DOWN) {
                sort_option_index = std::min(sort_option_index + 1, SORT_TYPES.size() - 1);
            } else if (vita_button == SCE_CTRL_CROSS) {
                select_sort_type(emuenv, app_state, SORT_TYPES[sort_option_index]);
            } else if (vita_button == SCE_CTRL_CIRCLE) {
                close_sort_menu(app_state);
            }
        } else if (vita_button == SCE_CTRL_TRIANGLE)
            cycle_sort_type(emuenv, app_state);
        else if (vita_button == SCE_CTRL_SQUARE)
            toggle_sort_direction(emuenv, app_state);
        else if (vita_button == SCE_CTRL_RIGHT)
            move_header_focus(app_state, window, true);
        else if (vita_button == SCE_CTRL_LEFT)
            move_header_focus(app_state, window, false);
        else if (vita_button == SCE_CTRL_UP)
            move_selection(app_state, window, -1, true);
        else if (vita_button == SCE_CTRL_DOWN)
            move_selection(app_state, window, 1, true);
        else if (vita_button == SCE_CTRL_CROSS)
            confirm_selection(emuenv, state, window);
        else if (vita_button == SCE_CTRL_CIRCLE)
            cancel_selection(app_state, window, running);
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        mouse_input_active = true;
        mouse_hovering_app = false;
        const float wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        if (wheel_y != 0.f && !matching_apps.empty()) {
            const int lines = std::max(1, static_cast<int>(std::ceil(std::abs(wheel_y) * 3.f)));
            const auto visible_rows = get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y);
            const size_t max_first_row = matching_apps.size() > visible_rows
                ? matching_apps.size() - visible_rows
                : 0;
            if (wheel_y > 0.f)
                first_row = first_row > static_cast<size_t>(lines) ? first_row - lines : 0;
            else
                first_row = std::min(first_row + static_cast<size_t>(lines), max_first_row);
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        mouse_input_active = true;
        mouse_hovering_app = false;
        float render_x = 0.f;
        float render_y = 0.f;
        if (!event_to_render_coordinates(event.motion.x, event.motion.y, render_x, render_y)) {
            hovered_header_control = HeaderControl::None;
            break;
        }
        const auto clock = get_clock_layout(emuenv);
        const auto header = get_header_layout(viewport_area.width, clock.width * viewport_area.scale.x,
            viewport_area.scale.x);
        const float logical_x = render_x - viewport_area.x;
        const float logical_y = render_y - viewport_area.y;
        if (logical_y >= header_vertical.control_top
            && logical_y < header_vertical.control_top + header_vertical.control_height) {
            if (logical_x >= USER_LEFT * viewport_area.scale.x
                && logical_x < (USER_LEFT + USER_WIDTH) * viewport_area.scale.x)
                hovered_header_control = HeaderControl::User;
            else if (logical_x >= header.refresh_x && logical_x < header.refresh_x + header.refresh_width)
                hovered_header_control = HeaderControl::Refresh;
            else if (logical_x >= header.sort_x && logical_x < header.sort_x + header.sort_width)
                hovered_header_control = HeaderControl::Sort;
            else if (logical_x >= header.search_x && logical_x < header.search_x + header.search_width)
                hovered_header_control = HeaderControl::Search;
            else
                hovered_header_control = HeaderControl::None;
        } else {
            hovered_header_control = HeaderControl::None;
        }
        const float sort_option_height = SORT_OPTION_HEIGHT * header_vertical.control_scale;
        const float sort_menu_height = sort_option_height * static_cast<float>(SORT_TYPES.size());
        if (sort_menu_open && logical_x >= header.sort_x && logical_x < header.sort_x + header.sort_width
            && logical_y >= header_vertical.sort_menu_top && logical_y < header_vertical.sort_menu_top + sort_menu_height) {
            sort_option_index = static_cast<size_t>((logical_y - header_vertical.sort_menu_top) / sort_option_height);
            break;
        }
        const auto visible_rows = get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y);
        if (scrollbar_dragging) {
            const auto scrollbar = get_scrollbar_layout(viewport_area, list_top, list_bottom,
                matching_apps.size(), visible_rows, first_row);
            const float thumb_travel = scrollbar.track.h - scrollbar.thumb.h;
            if (thumb_travel > 0.f && scrollbar.max_first_index > 0) {
                const float thumb_top = std::clamp(render_y - scrollbar_drag_offset_y,
                    scrollbar.track.y, scrollbar.track.y + thumb_travel);
                const float scroll_fraction = (thumb_top - scrollbar.track.y) / thumb_travel;
                first_row = static_cast<size_t>(std::lround(
                    scroll_fraction * static_cast<float>(scrollbar.max_first_index)));
                selected_index = std::clamp(selected_index, first_row,
                    std::min(first_row + visible_rows - 1, matching_apps.size() - 1));
            }
            break;
        }
        const auto row = row_at(render_x, render_y, first_row, visible_rows,
            matching_apps.size(), viewport_area.x + 20.f * viewport_area.scale.x,
            viewport_area.x + viewport_area.width - 20.f * viewport_area.scale.x,
            list_top, list_bottom, viewport_area.scale.y);
        if (row < matching_apps.size()) {
            selected_index = row;
            mouse_hovering_app = true;
        }
        break;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        mouse_input_active = true;
        mouse_hovering_app = false;
        hovered_header_control = HeaderControl::None;
        scrollbar_dragging = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        mouse_input_active = true;
        mouse_hovering_app = false;
        if (event.button.which == SDL_TOUCH_MOUSEID)
            suppress_next_finger_down = true;
        if (event.button.button == SDL_BUTTON_LEFT) {
            float render_x = 0.f;
            float render_y = 0.f;
            if (!event_to_render_coordinates(event.button.x, event.button.y, render_x, render_y))
                break;
            const auto clock = get_clock_layout(emuenv);
            const auto header = get_header_layout(viewport_area.width, clock.width * viewport_area.scale.x,
                viewport_area.scale.x);
            const float logical_x = render_x - viewport_area.x;
            const float logical_y = render_y - viewport_area.y;
            const auto visible_rows = get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y);
            const auto scrollbar = get_scrollbar_layout(viewport_area, list_top, list_bottom,
                matching_apps.size(), visible_rows, first_row);
            if (scrollbar.max_first_index > 0 && ui::point_in_rect(scrollbar.track, render_x, render_y)) {
                scrollbar_dragging = true;
                scrollbar_drag_offset_y = ui::point_in_rect(scrollbar.thumb, render_x, render_y)
                    ? render_y - scrollbar.thumb.y
                    : scrollbar.thumb.h / 2.f;
                const float thumb_travel = scrollbar.track.h - scrollbar.thumb.h;
                if (thumb_travel > 0.f) {
                    const float thumb_top = std::clamp(render_y - scrollbar_drag_offset_y,
                        scrollbar.track.y, scrollbar.track.y + thumb_travel);
                    const float scroll_fraction = (thumb_top - scrollbar.track.y) / thumb_travel;
                    first_row = static_cast<size_t>(std::lround(
                        scroll_fraction * static_cast<float>(scrollbar.max_first_index)));
                    selected_index = std::clamp(selected_index, first_row,
                        std::min(first_row + visible_rows - 1, matching_apps.size() - 1));
                }
                break;
            }
            const bool user_control_clicked = logical_y >= header_vertical.control_top
                && logical_y < header_vertical.control_top + header_vertical.control_height
                && logical_x >= USER_LEFT * viewport_area.scale.x
                && logical_x < (USER_LEFT + USER_WIDTH) * viewport_area.scale.x;
            const bool refresh_control_clicked = logical_y >= header_vertical.control_top
                && logical_y < header_vertical.control_top + header_vertical.control_height
                && logical_x >= header.refresh_x && logical_x < header.refresh_x + header.refresh_width;
            const bool sort_control_clicked = logical_y >= header_vertical.control_top
                && logical_y < header_vertical.control_top + header_vertical.control_height
                && logical_x >= header.sort_x && logical_x < header.sort_x + header.sort_width;
            const bool search_control_clicked = logical_y >= header_vertical.control_top
                && logical_y < header_vertical.control_top + header_vertical.control_height
                && logical_x >= header.search_x && logical_x < header.search_x + header.search_width;
            if (refresh_control_clicked)
                hovered_header_control = HeaderControl::Refresh;
            else if (sort_control_clicked)
                hovered_header_control = HeaderControl::Sort;
            else if (search_control_clicked)
                hovered_header_control = HeaderControl::Search;
            else if (!sort_menu_open)
                hovered_header_control = HeaderControl::None;
            const float sort_option_height = SORT_OPTION_HEIGHT * header_vertical.control_scale;
            const float sort_menu_height = sort_option_height * static_cast<float>(SORT_TYPES.size());
            if (sort_menu_open && logical_x >= header.sort_x && logical_x < header.sort_x + header.sort_width
                && logical_y >= header_vertical.sort_menu_top
                && logical_y < header_vertical.sort_menu_top + sort_menu_height) {
                const auto option = static_cast<size_t>((logical_y - header_vertical.sort_menu_top) / sort_option_height);
                select_sort_type(emuenv, app_state, SORT_TYPES[option]);
                break;
            }
            if (sort_menu_open) {
                close_sort_menu(app_state);
                sort_focused = false;
                if (sort_control_clicked || refresh_control_clicked)
                    break;
            }
            if (user_control_clicked) {
                hovered_header_control = HeaderControl::User;
                open_user_selector(emuenv, state, window);
                break;
            }
            if (refresh_control_clicked) {
                set_search_editing(app_state, window, false);
                search_focused = false;
                sort_focused = false;
                user_focused = false;
                refresh_focused = true;
                refresh_apps(emuenv, app_state);
                break;
            }
            if (sort_control_clicked) {
                open_sort_menu(emuenv, app_state, window);
                break;
            }
            if (search_control_clicked) {
                refresh_focused = false;
                sort_focused = false;
                user_focused = false;
                search_focused = true;
                set_search_editing(app_state, window, true);
                break;
            }
            if (matching_apps.empty())
                break;
            const auto row = row_at(render_x, render_y, first_row, visible_rows,
                matching_apps.size(), viewport_area.x + 20.f * viewport_area.scale.x,
                viewport_area.x + viewport_area.width - 20.f * viewport_area.scale.x,
                list_top, list_bottom, viewport_area.scale.y);
            if (row < matching_apps.size()) {
                set_search_editing(app_state, window, false);
                search_focused = false;
                refresh_focused = false;
                sort_focused = false;
                user_focused = false;
                queue_app_launch(emuenv, apps[matching_apps[row]].path);
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT)
            scrollbar_dragging = false;
        break;
    case SDL_EVENT_FINGER_DOWN:
        if (suppress_next_finger_down) {
            suppress_next_finger_down = false;
            break;
        }
        if (!matching_apps.empty()) {
            float render_x = 0.f;
            float render_y = 0.f;
            if (!event_to_render_coordinates(event.tfinger.x * static_cast<float>(window_width),
                    event.tfinger.y * static_cast<float>(window_height), render_x, render_y))
                break;
            const auto visible_rows = get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y);
            const auto row = row_at(render_x, render_y,
                first_row, visible_rows, matching_apps.size(), viewport_area.x + 20.f * viewport_area.scale.x,
                viewport_area.x + viewport_area.width - 20.f * viewport_area.scale.x,
                list_top, list_bottom, viewport_area.scale.y);
            if (row < matching_apps.size()) {
                search_focused = false;
                refresh_focused = false;
                sort_focused = false;
                user_focused = false;
                queue_app_launch(emuenv, apps[matching_apps[row]].path);
            }
        }
        break;
    default:
        break;
    }
}

void render_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    auto &app_state = app_selector_state;
    auto &search_query = app_state.search_query;
    auto &apps = app_state.apps;
    auto &matching_apps = app_state.matching_apps;
    auto &selected_index = app_state.selected_index;
    auto &first_row = app_state.first_row;
    auto &sort_option_index = app_state.sort_option_index;
    auto &hovered_header_control = app_state.hovered_header_control;
    auto &current_background = app_state.current_background;
    auto &last_background_change = app_state.last_background_change;
    auto &search_focused = app_state.search_focused;
    auto &search_editing = app_state.search_editing;
    auto &refresh_focused = app_state.refresh_focused;
    auto &sort_focused = app_state.sort_focused;
    auto &user_focused = app_state.user_focused;
    auto &sort_menu_open = app_state.sort_menu_open;
    auto &mouse_input_active = app_state.mouse_input_active;
    auto &mouse_hovering_app = app_state.mouse_hovering_app;
    auto &scrollbar_dragging = app_state.scrollbar_dragging;
    auto &scrollbar_drag_offset_y = app_state.scrollbar_drag_offset_y;
    if (!renderer || !sync_renderer_scale())
        return;

    const auto viewport_area = get_viewport_area();
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end()) {
        LOG_ERROR("Cannot render SDL app selector for unknown user '{}'.", emuenv.io.user_id);
        return;
    }
    const auto sort_type = user->second.sort_apps_type;
    const bool sort_descending = user->second.sort_apps_state == app::DESCENDANT;

    constexpr SDL_Color default_bar_color{ 0, 0, 0, 255 };
    constexpr SDL_Color default_indicator_color{ 255, 255, 255, 255 };
    const auto clock_color = default_indicator_color;
    const auto clock = get_clock_layout(emuenv);
    if (background_textures.size() > 1 && emuenv.cfg.delay_background > 0) {
        const Uint64 interval = static_cast<Uint64>(emuenv.cfg.delay_background) * 1000;
        const Uint64 now = SDL_GetTicks();
        if (now - last_background_change >= interval) {
            const Uint64 advances = (now - last_background_change) / interval;
            current_background = (current_background + advances % background_textures.size()) % background_textures.size();
            last_background_change += advances * interval;
        }
    }

    const auto scale = viewport_area.scale;
    const float list_top = viewport_area.y + viewport_area.header_height;
    const float list_bottom = viewport_area.y + viewport_area.height;
    const auto header_vertical = get_header_vertical_layout(viewport_area.header_height);
    const float list_clip_width = std::max(0.f, viewport_area.width - 40.f * scale.x);
    const float list_clip_height = std::max(0.f, list_bottom - list_top);
    const auto header = get_header_layout(viewport_area.width, clock.width * scale.x, scale.x);
    const float row_height = get_row_height(list_clip_height, matching_apps.size(), scale.y);
    const auto visible_rows = get_visible_rows(list_top, list_bottom, matching_apps.size(), scale.y);
    if (!apps.empty() && !mouse_input_active) {
        if (selected_index < first_row)
            first_row = selected_index;
        else if (visible_rows > 0 && selected_index >= first_row + visible_rows)
            first_row = selected_index - visible_rows + 1;
    }

    SDL_SetRenderDrawColor(renderer, 24, 26, 38, 255);
    SDL_RenderClear(renderer);
    draw_background(current_background, renderer, viewport_area);
    if (!background_textures.empty()) {
        if (background_overlay_available) {
            SDL_FRect overlay{ viewport_area.x, viewport_area.y, viewport_area.width, viewport_area.height };
            SDL_SetRenderDrawColor(renderer, 12, 14, 22, 80);
            if (!SDL_RenderFillRect(renderer, &overlay))
                LOG_WARN_ONCE("SDL app selector background overlay rendering failed: {}", SDL_GetError());
        }
    }
    SDL_FRect header_bar{
        viewport_area.x, viewport_area.y, viewport_area.width, viewport_area.header_height
    };
    SDL_SetRenderDrawColor(renderer, default_bar_color.r, default_bar_color.g, default_bar_color.b, default_bar_color.a);
    if (!SDL_RenderFillRect(renderer, &header_bar))
        LOG_WARN_ONCE("SDL app selector header bar rendering failed: {}", SDL_GetError());

    const float clock_y = viewport_area.y + font_centered_y(clock.text, viewport_area.header_height / scale.y) * scale.y;
    draw_text(renderer, scale, viewport_area.x + viewport_area.width - 20.f * scale.x - clock.width * scale.x,
        clock_y, clock.text.c_str(), clock_color);

    constexpr const char *title = "Select App";
    const float title_y = viewport_area.y + font_centered_y(title, viewport_area.header_height / scale.y) * scale.y;
    draw_text(renderer, scale, viewport_area.x + 24.f * scale.x, title_y, title, clock_color);

    SDL_FRect refresh_box{
        viewport_area.x + header.refresh_x,
        viewport_area.y + header_vertical.control_top,
        header.refresh_width,
        header_vertical.control_height
    };
    SDL_FRect user_box{
        viewport_area.x + USER_LEFT * scale.x,
        viewport_area.y + header_vertical.control_top,
        USER_WIDTH * scale.x,
        header_vertical.control_height
    };
    draw_header_control(renderer, user_box, scale,
        mouse_input_active ? hovered_header_control == HeaderControl::User : user_focused);
    draw_text(renderer, scale, viewport_area.x + (USER_LEFT + 7.f) * scale.x,
        viewport_area.y + header_vertical.control_top + 5.f * scale.y,
        "Users", { 235, 238, 248, 255 });
    draw_header_control(renderer, refresh_box, scale,
        mouse_input_active ? hovered_header_control == HeaderControl::Refresh : refresh_focused);
    draw_text(renderer, scale, viewport_area.x + header.refresh_x + 7.f * scale.x,
        viewport_area.y + header_vertical.control_top + 5.f * scale.y, "Refresh", { 235, 238, 248, 255 });
    const auto sort_text = "Sort: " + std::string(sort_type_name(sort_type));
    SDL_FRect sort_box{
        viewport_area.x + header.sort_x,
        viewport_area.y + header_vertical.control_top,
        header.sort_width,
        header_vertical.control_height
    };
    if (header.sort_width > 0.f) {
        draw_header_control(renderer, sort_box, scale,
            mouse_input_active ? hovered_header_control == HeaderControl::Sort : sort_focused);
        draw_text(renderer, scale, viewport_area.x + header.sort_x + 6.f * scale.x,
            viewport_area.y + header_vertical.control_top + 5.f * scale.y,
            sort_text.c_str(), { 235, 238, 248, 255 });
        draw_sort_direction_arrow(renderer, scale,
            viewport_area.x + header.sort_x + header.sort_width - 14.f * scale.x,
            viewport_area.y + header_vertical.control_top + 10.f * scale.y, sort_descending);
    }
    SDL_FRect search_box{
        viewport_area.x + header.search_x,
        viewport_area.y + header_vertical.control_top,
        header.search_width,
        header_vertical.control_height
    };
    draw_header_control(renderer, search_box, scale,
        mouse_input_active ? hovered_header_control == HeaderControl::Search : search_focused);
    SDL_Rect search_clip{
        static_cast<int>(search_box.x), static_cast<int>(search_box.y),
        static_cast<int>(search_box.w), static_cast<int>(search_box.h)
    };
    SDL_SetRenderClipRect(renderer, &search_clip);
    const auto search_prompt = search_editing
        ? "Type to search..."
        : (search_focused && !mouse_input_active ? "Press Cross/Enter to type" : "Search...");
    draw_text(renderer, scale, viewport_area.x + header.search_x + 6.f * scale.x,
        viewport_area.y + header_vertical.control_top + 5.f * scale.y,
        search_query.empty() ? search_prompt : search_query.c_str(),
        search_query.empty() ? SDL_Color{ 150, 156, 176, 255 } : SDL_Color{ 235, 238, 248, 255 });
    SDL_SetRenderClipRect(renderer, nullptr);

    SDL_Rect app_list_clip{
        static_cast<int>(viewport_area.x + 20.f * scale.x), static_cast<int>(list_top),
        static_cast<int>(list_clip_width), static_cast<int>(list_clip_height)
    };
    SDL_SetRenderClipRect(renderer, &app_list_clip);
    if (apps.empty()) {
        draw_text(renderer, scale, viewport_area.x + 24.f * scale.x, list_top,
            "No installed apps found.", { 235, 238, 248, 255 });
        draw_text(renderer, scale, viewport_area.x + 24.f * scale.x, list_top + TEXT_ROW_HEIGHT * scale.y,
            "Drag and drop .zip/.vpk files, or a .pkg with its .rif/.bin/work.bin license here.", { 180, 186, 204, 255 });
    } else if (matching_apps.empty()) {
        draw_text(renderer, scale, viewport_area.x + 24.f * scale.x, list_top,
            "No matching apps found.", { 235, 238, 248, 255 });
    } else {
        for (size_t row = 0; row < visible_rows && first_row + row < matching_apps.size(); ++row) {
            const size_t index = first_row + row;
            const auto &app = apps[matching_apps[index]];
            const float y = list_top + static_cast<float>(row) * row_height;
            const bool app_highlighted = mouse_input_active
                ? mouse_hovering_app && index == selected_index
                : index == selected_index && !search_focused && !refresh_focused && !sort_focused && !user_focused;
            if (app_highlighted) {
                SDL_FRect highlight{
                    viewport_area.x + 20.f * scale.x, y, list_clip_width, row_height
                };
                const SDL_Color outline_color{ 105, 225, 255, 255 };
                ui::draw_selection_highlight(renderer, highlight, { 20, 104, 194, 142 }, &outline_color);
            }
            draw_app_icon(emuenv, renderer, app, scale, viewport_area.x, y, row_height);
            const auto label = app_label(app);
            draw_text(renderer, scale, viewport_area.x + (26.f + ICON_SIZE + ICON_GUTTER) * scale.x,
                y + (row_height - TEXT_ROW_HEIGHT * scale.y) / 2.f, label.c_str(), { 235, 238, 248, 255 });
        }
    }
    SDL_SetRenderClipRect(renderer, nullptr);

    const auto scrollbar = get_scrollbar_layout(viewport_area, list_top, list_bottom,
        matching_apps.size(), visible_rows, first_row);
    if (scrollbar.max_first_index > 0)
        ui::draw_scrollbar(renderer, scrollbar.track, scrollbar.thumb,
            { 45, 50, 66, 220 }, { 105, 177, 244, 255 });

    if (sort_menu_open && header.sort_width > 0.f) {
        const float sort_option_height = SORT_OPTION_HEIGHT * header_vertical.control_scale;
        SDL_FRect menu_box{
            viewport_area.x + header.sort_x,
            viewport_area.y + header_vertical.sort_menu_top,
            header.sort_width,
            sort_option_height * static_cast<float>(SORT_TYPES.size())
        };
        SDL_SetRenderDrawColor(renderer, 38, 41, 56, 255);
        SDL_RenderFillRect(renderer, &menu_box);
        SDL_SetRenderDrawColor(renderer, 82, 120, 190, 255);
        SDL_RenderRect(renderer, &menu_box);
        for (size_t option = 0; option < SORT_TYPES.size(); ++option) {
            SDL_FRect option_box{
                menu_box.x,
                menu_box.y + static_cast<float>(option) * sort_option_height,
                menu_box.w,
                sort_option_height
            };
            if (option == sort_option_index) {
                const SDL_Color outline_color{ 105, 225, 255, 255 };
                ui::draw_selection_highlight(renderer, option_box, { 20, 104, 194, 142 }, &outline_color);
            }
            draw_text(renderer, scale, viewport_area.x + header.sort_x + 8.f * scale.x,
                viewport_area.y + header_vertical.sort_menu_top + 6.f * scale.y
                    + static_cast<float>(option) * sort_option_height,
                sort_type_name(SORT_TYPES[option]), { 235, 238, 248, 255 });
            if (SORT_TYPES[option] == sort_type)
                draw_sort_direction_arrow(renderer, scale,
                    viewport_area.x + header.sort_x + header.sort_width - 14.f * scale.x,
                    viewport_area.y + header_vertical.sort_menu_top + 10.f * scale.y
                        + static_cast<float>(option) * sort_option_height,
                    sort_descending);
        }
    }

    SDL_RenderPresent(renderer);
}

void destroy_app_selector(SDL_Window *window) {
    stop_text_input(app_selector_state, window);
    release_render_resources();
    app_selector_state = {};
    reset_user_selector();
}

} // namespace sdl_frontend