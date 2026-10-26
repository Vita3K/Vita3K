// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/functions.h>

#include <app/functions.h>
#include <app/state.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <overlay/controls.h>
#include <overlay/element.h>
#include <renderer/state.h>
#include <sdl-frontend/font_renderer.h>
#include <sdl/functions.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <util/types.h>

#include <SDL3/SDL.h>

#include <array>
#include <unordered_set>

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
constexpr float REFRESH_LEFT = 145.f;
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

static std::string app_label(const app::AppEntry &app) {
    std::string label = app.title.empty() ? app.title_id : app.title + " [" + app.title_id + "]";
    return label + " (v" + app.app_ver + ")";
}

static const char *sort_type_name(app::UserSortType type) {
    switch (type) {
    case app::APP_VER: return "Version";
    case app::CATEGORY: return "Category";
    case app::LAST_TIME: return "Last played";
    case app::TITLE:
    case app::COMPAT: return "Title";
    case app::TITLE_ID: return "Title ID";
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

static ui::FontRenderer selector_font;

static void destroy_font() {
    selector_font.release();
}

static void initialize_font(SDL_Renderer *renderer, EmuEnvState &emuenv, SDL_Window *window) {
    selector_font.initialize(renderer, emuenv, window, "ltn4.pvf", 14, true);
}

static float font_centered_y(std::string_view text, float area_height) {
    return selector_font.centered_y(text, area_height);
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

    const float width = selector_font.text_width(text);
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

struct ViewportArea {
    float x;
    float y;
    float width;
    float height;
    float header_height;
    SceFVector2 scale;
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
    const SceFVector2 scale{
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

static void draw_app_icon(EmuEnvState &emuenv, SDL_Renderer *renderer, const app::AppEntry &app,
    SceFVector2 coordinate_scale, float x_offset, float y, float row_height) {
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

static void draw_text(SDL_Renderer *renderer, SceFVector2 coordinate_scale, float x, float y,
    const char *text, SDL_Color color) {
    selector_font.draw_text(renderer, coordinate_scale, x, y, text, color);
}

static void draw_header_control(SDL_Renderer *renderer, const SDL_FRect &box, SceFVector2 scale, bool focused) {
    ui::draw_rect(renderer, box,
        { static_cast<Uint8>(focused ? 26 : 38), static_cast<Uint8>(focused ? 94 : 41),
            static_cast<Uint8>(focused ? 126 : 56), 255 });
    const SDL_Color outline_color{
        static_cast<Uint8>(focused ? 105 : 82), static_cast<Uint8>(focused ? 225 : 120),
        static_cast<Uint8>(focused ? 255 : 190), 255
    };
    SDL_SetRenderDrawColor(renderer, outline_color.r, outline_color.g, outline_color.b, outline_color.a);
    SDL_RenderRect(renderer, &box);
    if (focused) {
        ui::draw_inset_border(renderer, box, scale.x, scale.y, outline_color);
    }
}

static void draw_sort_direction_arrow(SDL_Renderer *renderer, SceFVector2 coordinate_scale, float x, float y, bool descending) {
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

static std::string search_query;
static std::vector<app::AppEntry> apps;
static std::vector<size_t> matching_apps;
static SDL_Renderer *renderer = nullptr;
static float renderer_system_scale = 0.f;
static size_t selected_index = 0;
static size_t first_row = 0;
static size_t sort_option_index = 0;
enum class HeaderControl {
    None,
    Refresh,
    Sort,
    Search
};
static HeaderControl hovered_header_control = HeaderControl::None;
static bool search_focused = false;
static bool search_editing = false;
static bool refresh_focused = false;
static bool sort_focused = false;
static bool mouse_input_active = false;
static bool mouse_hovering_app = false;
static bool scrollbar_dragging = false;
static float scrollbar_drag_offset_y = 0.f;
static bool sort_menu_open = false;
static bool text_input_started = false;
static bool suppress_next_text_input = false;
static bool suppress_next_finger_down = false;

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
    if (!ui::window_to_render_coordinates(renderer, window_x, window_y, render_x, render_y)) {
        LOG_WARN_ONCE("SDL app selector could not map window coordinates to its logical viewport: {}", SDL_GetError());
        return false;
    }
    return true;
}

static bool initialize_render_resources(EmuEnvState &emuenv, SDL_Window *window) {
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
    initialize_font(renderer, emuenv, window);
    destroy_app_icons();
    return true;
}

static void release_render_resources() {
    destroy_app_icons();
    destroy_font();
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    renderer_system_scale = 0.f;
}

static void stop_text_input(SDL_Window *window) {
    if (!text_input_started)
        return;
    if (window && !SDL_StopTextInput(window))
        LOG_WARN("SDL app selector could not stop text input: {}", SDL_GetError());
    text_input_started = false;
}

static bool start_text_input(SDL_Window *window) {
    if (text_input_started)
        return true;
    if (!window || !SDL_StartTextInput(window)) {
        LOG_WARN("SDL app selector could not start text input: {}", SDL_GetError());
        return false;
    }
    text_input_started = true;
    return true;
}

static void set_search_editing(SDL_Window *window, bool editing) {
    if (editing == search_editing || !window)
        return;
    if (editing) {
        if (!start_text_input(window))
            return;
    } else
        stop_text_input(window);
    search_editing = editing;
}

static void move_selection(SDL_Window *window, int offset, bool focus_search_at_top = false) {
    if (offset < 0) {
        if (search_focused || refresh_focused || sort_focused)
            return;
        if (focus_search_at_top && offset == -1 && selected_index == 0) {
            refresh_focused = true;
            return;
        }
    } else if (offset > 0) {
        if (refresh_focused) {
            refresh_focused = false;
            return;
        }
        if (sort_focused) {
            sort_focused = false;
            return;
        }
        if (search_focused) {
            search_focused = false;
            set_search_editing(window, false);
            return;
        }
    }
    if (matching_apps.empty())
        return;
    const auto last_index = static_cast<int64_t>(matching_apps.size() - 1);
    const auto next_index = std::clamp(static_cast<int64_t>(selected_index) + offset, int64_t{ 0 }, last_index);
    selected_index = static_cast<size_t>(next_index);
}

static void cancel_selection(SDL_Window *window, bool &running) {
    if (sort_menu_open) {
        sort_menu_open = false;
        return;
    }
    if (search_editing || search_focused || refresh_focused || sort_focused || !search_query.empty()) {
        set_search_editing(window, false);
        search_focused = false;
        refresh_focused = false;
        sort_focused = false;
        search_query.clear();
        matching_apps = find_matching_apps(apps, search_query);
        selected_index = 0;
        first_row = 0;
    } else {
        running = false;
    }
}

static void open_sort_menu(EmuEnvState &emuenv, SDL_Window *window) {
    set_search_editing(window, false);
    search_focused = false;
    refresh_focused = false;
    sort_focused = true;
    sort_menu_open = true;
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end()) {
        LOG_ERROR("Cannot open SDL sort menu for unknown user '{}'.", emuenv.io.user_id);
        sort_option_index = 0;
        return;
    }
    const auto current = std::find(SORT_TYPES.begin(), SORT_TYPES.end(), user->second.sort_apps_type);
    sort_option_index = current == SORT_TYPES.end() ? 0 : static_cast<size_t>(std::distance(SORT_TYPES.begin(), current));
}

static void close_sort_menu() {
    sort_menu_open = false;
}

static void move_header_focus(SDL_Window *window, bool move_right) {
    if (search_editing)
        set_search_editing(window, false);

    if (move_right) {
        if (refresh_focused) {
            refresh_focused = false;
            sort_focused = true;
        } else if (sort_focused) {
            sort_focused = false;
            search_focused = true;
        } else if (!search_focused) {
            refresh_focused = true;
        }
    } else if (search_focused) {
        search_focused = false;
        sort_focused = true;
    } else if (sort_focused) {
        sort_focused = false;
        refresh_focused = true;
    } else if (refresh_focused) {
        refresh_focused = false;
    }
}

static void apply_sort(EmuEnvState &emuenv) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end()) {
        LOG_ERROR("Cannot sort SDL app selector list for unknown user '{}'.", emuenv.io.user_id);
        return;
    }

    const auto sort_type = user->second.sort_apps_type;
    const auto sort_state = user->second.sort_apps_state;
    const auto app_times = sort_type == app::LAST_TIME
        ? app::get_user_app_times(emuenv)
        : std::map<std::string, app::AppTime>{};
    std::sort(apps.begin(), apps.end(), [&](const app::AppEntry &lhs, const app::AppEntry &rhs) {
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

    matching_apps = find_matching_apps(apps, search_query);
    selected_index = 0;
    first_row = 0;
}

static void refresh_app_list(EmuEnvState &emuenv) {
    destroy_app_icons();
    apps = app::get_apps(emuenv);
    matching_apps = find_matching_apps(apps, search_query);
    selected_index = 0;
    first_row = 0;
    apply_sort(emuenv);
}

static void scan_and_refresh_apps(EmuEnvState &emuenv) {
    if (!app::scan_apps(emuenv)) {
        LOG_ERROR("Could not refresh the SDL app selector list.");
        return;
    }

    refresh_app_list(emuenv);
}

static void select_sort_type(EmuEnvState &emuenv, app::UserSortType sort_type) {
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user == emuenv.app.user_list.users.end()) {
        LOG_ERROR("Cannot change SDL app selector sort for unknown user '{}'.", emuenv.io.user_id);
        return;
    }
    if (user->second.sort_apps_type == sort_type)
        user->second.sort_apps_state = user->second.sort_apps_state == app::DESCENDANT ? app::ASCENDANT : app::DESCENDANT;
    else
        user->second.sort_apps_type = sort_type;
    app::save_user(emuenv, emuenv.io.user_id);
    apply_sort(emuenv);
    close_sort_menu();
    sort_focused = true;
}

static void queue_app_launch(EmuEnvState &emuenv, const std::string &path) {
    app::request_in_process_launch(emuenv, AppLaunchRequest{ .app_path = path });
}

static void confirm_selection(EmuEnvState &emuenv, SDL_Window *window) {
    if (refresh_focused) {
        scan_and_refresh_apps(emuenv);
    } else if (sort_focused) {
        open_sort_menu(emuenv, window);
    } else if (search_focused) {
        if (search_editing) {
            set_search_editing(window, false);
            search_focused = false;
        } else {
            set_search_editing(window, true);
        }
    } else if (!matching_apps.empty()) {
        queue_app_launch(emuenv, apps[matching_apps[selected_index]].path);
    }
}

bool initialize_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    stop_text_input(window);
    release_render_resources();
    search_query.clear();
    apps = app::get_apps(emuenv);
    matching_apps = find_matching_apps(apps, search_query);
    apply_sort(emuenv);
    if (!sync_renderer_scale())
        return false;
    selected_index = 0;
    first_row = 0;
    search_focused = false;
    search_editing = false;
    refresh_focused = false;
    sort_focused = false;
    mouse_input_active = false;
    mouse_hovering_app = false;
    scrollbar_dragging = false;
    hovered_header_control = HeaderControl::None;
    sort_menu_open = false;
    suppress_next_text_input = false;
    suppress_next_finger_down = false;
    if (!initialize_render_resources(emuenv, window))
        return false;
    refresh_controllers(emuenv.ctrl, emuenv);
    return true;
}

void suspend_app_selector(EmuEnvState &, SDL_Window *window) {
    suppress_next_text_input = false;
    suppress_next_finger_down = false;
    stop_text_input(window);
    release_render_resources();
}

bool resume_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    if (!renderer && !initialize_render_resources(emuenv, window))
        return false;
    if (search_editing && !start_text_input(window))
        search_editing = false;
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (user != emuenv.app.user_list.users.end()) {
        if (user->second.sort_apps_type == app::LAST_TIME)
            apply_sort(emuenv);
    } else {
        LOG_ERROR("Cannot resume SDL app selector for unknown user '{}'.", emuenv.io.user_id);
        return false;
    }
    return true;
}

void handle_app_selector_event(EmuEnvState &emuenv, SDL_Window *window, const SDL_Event &event,
    uint32_t vita_button, bool &running) {
    if (event.type == SDL_EVENT_DROP_COMPLETE) {
        refresh_app_list(emuenv);
        return;
    }
    if (!running)
        return;
    if (!sync_renderer_scale())
        return;
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
        if (suppress_next_text_input)
            suppress_next_text_input = false;
        if (event.key.key == SDLK_ESCAPE) {
            cancel_selection(window, running);
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
        const bool navigation_repeat = event.key.key == SDLK_UP || event.key.key == SDLK_DOWN
            || event.key.key == SDLK_PAGEUP || event.key.key == SDLK_PAGEDOWN
            || vita_button == SCE_CTRL_UP || vita_button == SCE_CTRL_DOWN;
        if (event.key.repeat && !navigation_repeat)
            break;
        if (sort_menu_open) {
            if (vita_button == SCE_CTRL_UP || event.key.key == SDLK_UP) {
                if (sort_option_index > 0)
                    --sort_option_index;
            } else if (vita_button == SCE_CTRL_DOWN || event.key.key == SDLK_DOWN) {
                sort_option_index = std::min(sort_option_index + 1, SORT_TYPES.size() - 1);
            } else if (vita_button == SCE_CTRL_CROSS || event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                select_sort_type(emuenv, SORT_TYPES[sort_option_index]);
            } else if (vita_button == SCE_CTRL_CIRCLE) {
                close_sort_menu();
            }
            break;
        }
        if (search_editing) {
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
                confirm_selection(emuenv, window);
            break;
        }
        if (vita_button == SCE_CTRL_RIGHT || event.key.key == SDLK_RIGHT) {
            move_header_focus(window, true);
            break;
        }
        if (vita_button == SCE_CTRL_LEFT || event.key.key == SDLK_LEFT) {
            move_header_focus(window, false);
            break;
        }
        if (event.key.key == SDLK_PAGEUP)
            move_selection(window, -static_cast<int>(get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y)));
        else if (event.key.key == SDLK_PAGEDOWN)
            move_selection(window, static_cast<int>(get_visible_rows(list_top, list_bottom, matching_apps.size(), viewport_area.scale.y)));
        else if (vita_button == SCE_CTRL_UP || event.key.key == SDLK_UP)
            move_selection(window, -1, true);
        else if (vita_button == SCE_CTRL_DOWN || event.key.key == SDLK_DOWN)
            move_selection(window, 1, true);
        else if (vita_button == SCE_CTRL_CROSS || event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
            if (search_focused && event.key.key >= SDLK_SPACE && event.key.key <= SDLK_Z)
                suppress_next_text_input = true;
            confirm_selection(emuenv, window);
        } else if (vita_button == SCE_CTRL_CIRCLE) {
            cancel_selection(window, running);
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
        if (sort_menu_open) {
            if (vita_button == SCE_CTRL_UP) {
                if (sort_option_index > 0)
                    --sort_option_index;
            } else if (vita_button == SCE_CTRL_DOWN) {
                sort_option_index = std::min(sort_option_index + 1, SORT_TYPES.size() - 1);
            } else if (vita_button == SCE_CTRL_CROSS) {
                select_sort_type(emuenv, SORT_TYPES[sort_option_index]);
            } else if (vita_button == SCE_CTRL_CIRCLE) {
                close_sort_menu();
            }
        } else if (vita_button == SCE_CTRL_RIGHT)
            move_header_focus(window, true);
        else if (vita_button == SCE_CTRL_LEFT)
            move_header_focus(window, false);
        else if (vita_button == SCE_CTRL_UP)
            move_selection(window, -1, true);
        else if (vita_button == SCE_CTRL_DOWN)
            move_selection(window, 1, true);
        else if (vita_button == SCE_CTRL_CROSS)
            confirm_selection(emuenv, window);
        else if (vita_button == SCE_CTRL_CIRCLE)
            cancel_selection(window, running);
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
            if (logical_x >= header.refresh_x && logical_x < header.refresh_x + header.refresh_width)
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
            if (!event_to_render_coordinates(event.button.x, event.button.y, render_x, render_y)) {
                hovered_header_control = HeaderControl::None;
                break;
            }
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
                select_sort_type(emuenv, SORT_TYPES[option]);
                break;
            }
            if (sort_menu_open) {
                close_sort_menu();
                sort_focused = false;
                if (sort_control_clicked || refresh_control_clicked)
                    break;
            }
            if (refresh_control_clicked) {
                set_search_editing(window, false);
                search_focused = false;
                sort_focused = false;
                refresh_focused = true;
                scan_and_refresh_apps(emuenv);
                break;
            }
            if (sort_control_clicked) {
                open_sort_menu(emuenv, window);
                break;
            }
            if (search_control_clicked) {
                refresh_focused = false;
                sort_focused = false;
                search_focused = true;
                set_search_editing(window, true);
                break;
            }
            if (matching_apps.empty())
                break;
            const auto row = row_at(render_x, render_y, first_row, visible_rows,
                matching_apps.size(), viewport_area.x + 20.f * viewport_area.scale.x,
                viewport_area.x + viewport_area.width - 20.f * viewport_area.scale.x,
                list_top, list_bottom, viewport_area.scale.y);
            if (row < matching_apps.size()) {
                set_search_editing(window, false);
                search_focused = false;
                refresh_focused = false;
                sort_focused = false;
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
                queue_app_launch(emuenv, apps[matching_apps[row]].path);
            }
        }
        break;
    default:
        break;
    }
}

void render_app_selector(EmuEnvState &emuenv, SDL_Window *window) {
    if (!renderer || !sync_renderer_scale())
        return;

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
    const auto viewport_area = get_viewport_area();
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
    ui::draw_background(renderer,
        { viewport_area.x, viewport_area.y, viewport_area.width, viewport_area.height },
        { 11, 90, 252, 160 });

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
            viewport_area.x + header.sort_x + header.sort_width - 16.f * scale.x,
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
                : index == selected_index && !search_focused && !refresh_focused && !sort_focused;
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
    stop_text_input(window);
    release_render_resources();
    search_query.clear();
    apps.clear();
    matching_apps.clear();
}

} // namespace sdl_frontend