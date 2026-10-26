// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/functions.h>

#include <app/functions.h>
#include <config/functions.h>
#include <ctrl/functions.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <overlay/font.h>
#include <renderer/state.h>
#include <sdl-frontend/font_renderer.h>
#include <sdl/functions.h>
#include <util/exit_code.h>
#include <util/log.h>
#include <util/system.h>

#include <SDL3/SDL.h>

#include <cmath>

namespace sdl_frontend {

struct LanguageOption {
    SceSystemParamLang id;
    const char *name;
};

const std::array<LanguageOption, 20> SYSTEM_LANGUAGES = { {
    { SCE_SYSTEM_PARAM_LANG_DANISH, "Dansk" },
    { SCE_SYSTEM_PARAM_LANG_GERMAN, "Deutsch" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_GB, "English (United Kingdom)" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_US, "English (United States)" },
    { SCE_SYSTEM_PARAM_LANG_SPANISH, reinterpret_cast<const char *>(u8"Español") },
    { SCE_SYSTEM_PARAM_LANG_FRENCH, reinterpret_cast<const char *>(u8"Français") },
    { SCE_SYSTEM_PARAM_LANG_ITALIAN, "Italiano" },
    { SCE_SYSTEM_PARAM_LANG_DUTCH, "Nederlands" },
    { SCE_SYSTEM_PARAM_LANG_NORWEGIAN, "Norsk" },
    { SCE_SYSTEM_PARAM_LANG_POLISH, "Polski" },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR, reinterpret_cast<const char *>(u8"Português (Brasil)") },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT, reinterpret_cast<const char *>(u8"Português (Portugal)") },
    { SCE_SYSTEM_PARAM_LANG_RUSSIAN, reinterpret_cast<const char *>(u8"Русский") },
    { SCE_SYSTEM_PARAM_LANG_FINNISH, "Suomi" },
    { SCE_SYSTEM_PARAM_LANG_SWEDISH, "Svenska" },
    { SCE_SYSTEM_PARAM_LANG_TURKISH, reinterpret_cast<const char *>(u8"Türkçe") },
    { SCE_SYSTEM_PARAM_LANG_JAPANESE, reinterpret_cast<const char *>(u8"日本語") },
    { SCE_SYSTEM_PARAM_LANG_KOREAN, "Korean" },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_S, reinterpret_cast<const char *>(u8"简体中文") },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_T, reinterpret_cast<const char *>(u8"繁體中文") },
} };

static void set_setup_system_language(int language) {
    overlay::fontmgr::set_system_lang(language);
}

struct TextRenderer {
    SDL_Renderer *renderer{};
    ui::FontRenderer font;

    ~TextRenderer() {
        font.release();
        if (renderer)
            SDL_DestroyRenderer(renderer);
    }
};

struct SetupLayout {
    SDL_FRect panel;
    SDL_FRect list;
    SDL_FRect finish_button;
    float row_height;
    size_t visible_rows;
    SceFVector2 scale;
};

static SetupLayout get_setup_layout() {
    const auto &viewport = sdl::get_viewport();
    const SceFVector2 scale{
        viewport.scale_x * viewport.manual_dpi_scale,
        viewport.scale_y * viewport.manual_dpi_scale
    };
    const float panel_width = std::min(756.f * scale.x, std::max(0.f, viewport.logical_width - 196.f * scale.x));
    const float panel_height = std::min(418.f * scale.y, std::max(0.f, viewport.logical_height - 126.f * scale.y));
    SDL_FRect panel{
        viewport.logical_x + (viewport.logical_width - panel_width) / 2.f,
        viewport.logical_y + 30.f * scale.y,
        panel_width,
        panel_height
    };
    const float list_top = panel.y + 116.f * scale.y;
    const float list_bottom = panel.y + panel.h - 12.f * scale.y;
    const float row_height = std::max(1.f, 72.f * scale.y);
    const float list_height = std::max(0.f, list_bottom - list_top);
    const float list_width = std::max(0.f, panel.w - 212.f * scale.x);
    const float button_width = std::min(186.f * scale.x, std::max(0.f, viewport.logical_width - 16.f * scale.x));
    const float button_height = 52.f * scale.y;
    const auto visible_rows = std::max<size_t>(1, static_cast<size_t>(list_height / row_height));
    return {
        panel,
        { panel.x + 100.f * scale.x, list_top, list_width, list_height },
        { viewport.logical_x + (viewport.logical_width - button_width) / 2.f,
            viewport.logical_y + viewport.logical_height - button_height - 6.f * scale.y,
            button_width,
            button_height },
        row_height,
        visible_rows,
        scale
    };
}

static ScrollbarLayout get_scrollbar_layout(const SetupLayout &layout, size_t first_option) {
    const SDL_FRect track{
        layout.list.x + layout.list.w,
        layout.list.y,
        12.f * layout.scale.x,
        layout.list.h
    };
    const float thumb_height = std::min(track.h,
        std::max(20.f * layout.scale.y,
            track.h * static_cast<float>(layout.visible_rows) / static_cast<float>(SYSTEM_LANGUAGES.size())));
    const size_t max_first_option = SYSTEM_LANGUAGES.size() > layout.visible_rows
        ? SYSTEM_LANGUAGES.size() - layout.visible_rows
        : 0;
    const float scroll_fraction = max_first_option == 0
        ? 0.f
        : static_cast<float>(first_option) / static_cast<float>(max_first_option);
    const SDL_FRect thumb{
        track.x + 1.f * layout.scale.x,
        track.y + (track.h - thumb_height) * scroll_fraction,
        std::max(0.f, track.w - 2.f * layout.scale.x),
        thumb_height
    };
    return { track, thumb, max_first_option };
}

static void keep_selection_visible(size_t selection, size_t visible_rows, size_t &first_option) {
    if (selection < first_option)
        first_option = selection;
    else if (selection >= first_option + visible_rows)
        first_option = selection - visible_rows + 1;
    first_option = std::min(first_option, SYSTEM_LANGUAGES.size() > visible_rows ? SYSTEM_LANGUAGES.size() - visible_rows : 0);
}

static void draw_rounded_rect(SDL_Renderer *renderer, const SDL_FRect &rect, float radius, SDL_Color color) {
    constexpr size_t corner_segments = 6;
    constexpr float pi = 3.14159265358979323846f;
    const float corner_radius = std::min(radius, std::min(rect.w, rect.h) / 2.f);
    const std::array<std::array<float, 4>, 4> corners = { { { rect.x + corner_radius, rect.y + corner_radius, pi, pi * 1.5f },
        { rect.x + rect.w - corner_radius, rect.y + corner_radius, pi * 1.5f, pi * 2.f },
        { rect.x + rect.w - corner_radius, rect.y + rect.h - corner_radius, 0.f, pi * 0.5f },
        { rect.x + corner_radius, rect.y + rect.h - corner_radius, pi * 0.5f, pi } } };
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(1 + corners.size() * (corner_segments + 1));
    indices.reserve(corners.size() * (corner_segments + 1) * 3);
    vertices.push_back({ .position = { rect.x + rect.w / 2.f, rect.y + rect.h / 2.f },
        .color = { static_cast<float>(color.r) / 255.f, static_cast<float>(color.g) / 255.f,
            static_cast<float>(color.b) / 255.f, static_cast<float>(color.a) / 255.f },
        .tex_coord = { 0.f, 0.f } });
    for (const auto &corner : corners) {
        for (size_t segment = 0; segment <= corner_segments; ++segment) {
            const float progress = static_cast<float>(segment) / static_cast<float>(corner_segments);
            const float angle = corner[2] + (corner[3] - corner[2]) * progress;
            vertices.push_back({ .position = {
                                     corner[0] + corner_radius * std::cos(angle),
                                     corner[1] + corner_radius * std::sin(angle) },
                .color = { static_cast<float>(color.r) / 255.f, static_cast<float>(color.g) / 255.f, static_cast<float>(color.b) / 255.f, static_cast<float>(color.a) / 255.f },
                .tex_coord = { 0.f, 0.f } });
        }
    }
    const size_t perimeter_vertices = vertices.size() - 1;
    for (size_t vertex = 0; vertex < perimeter_vertices; ++vertex) {
        indices.insert(indices.end(), { 0, static_cast<int>(vertex + 1), static_cast<int>((vertex + 1) % perimeter_vertices + 1) });
    }
    if (!SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
            indices.data(), static_cast<int>(indices.size())))
        LOG_WARN_ONCE("SDL initial setup finish button rendering failed: {}", SDL_GetError());
}

static void draw_setup(TextRenderer &text_renderer, size_t selected_option, size_t focused_option,
    bool finish_focused, size_t first_option) {
    const auto layout = get_setup_layout();
    const auto &viewport = sdl::get_viewport();
    ui::draw_background(text_renderer.renderer,
        { viewport.logical_x, viewport.logical_y, viewport.logical_width, viewport.logical_height },
        { 194, 207, 241, 255 });

    SDL_SetRenderDrawColor(text_renderer.renderer, 255, 255, 255, 255);
    SDL_RenderRect(text_renderer.renderer, &layout.panel);
    ui::draw_inset_border(text_renderer.renderer, layout.panel, layout.scale.x, layout.scale.y,
        { 255, 255, 255, 255 });

    constexpr std::string_view title = "Select a language.";
    const SceFVector2 title_scale{ layout.scale.x * 1.2f, layout.scale.y * 1.2f };
    const float title_width = text_renderer.font.text_width(title) * title_scale.x;
    text_renderer.font.draw_text(text_renderer.renderer, title_scale, layout.panel.x + (layout.panel.w - title_width) / 2.f,
        layout.panel.y + 12.f * layout.scale.y
            + text_renderer.font.centered_y(title, 56.f / 1.2f) * title_scale.y,
        title, { 235, 238, 248, 255 });

    SDL_SetRenderDrawColor(text_renderer.renderer, 100, 112, 139, 255);
    SDL_RenderLine(text_renderer.renderer,
        layout.panel.x + 10.f * layout.scale.x, layout.panel.y + 94.f * layout.scale.y,
        layout.panel.x + layout.panel.w - 10.f * layout.scale.x, layout.panel.y + 94.f * layout.scale.y);

    const auto scrollbar = get_scrollbar_layout(layout, first_option);
    const float scrollbar_width = scrollbar.track.w;
    ui::draw_scrollbar(text_renderer.renderer, scrollbar.track, scrollbar.thumb,
        { 39, 40, 52, 255 }, { 105, 106, 119, 255 });

    const SDL_Rect list_clip{
        static_cast<int>(layout.list.x),
        static_cast<int>(layout.list.y),
        static_cast<int>(layout.list.w + scrollbar_width),
        static_cast<int>(layout.list.h)
    };
    SDL_SetRenderClipRect(text_renderer.renderer, &list_clip);
    for (size_t row = 0; row < layout.visible_rows; ++row) {
        const size_t option_index = first_option + row;
        if (option_index >= SYSTEM_LANGUAGES.size())
            break;

        SDL_FRect row_box{
            layout.list.x,
            layout.list.y + static_cast<float>(row) * layout.row_height,
            layout.list.w,
            layout.row_height
        };
        if (!finish_focused && option_index == focused_option) {
            ui::draw_selection_highlight(text_renderer.renderer, row_box, { 83, 177, 244, 255 });
        }
        if (option_index == selected_option)
            text_renderer.font.draw_text(text_renderer.renderer, layout.scale, row_box.x + 100.f * layout.scale.x,
                row_box.y + text_renderer.font.centered_y("V", layout.row_height / layout.scale.y) * layout.scale.y,
                "V", { 245, 245, 250, 255 });
        text_renderer.font.draw_text(text_renderer.renderer, layout.scale, row_box.x + 134.f * layout.scale.x,
            row_box.y + text_renderer.font.centered_y(SYSTEM_LANGUAGES[option_index].name, layout.row_height / layout.scale.y) * layout.scale.y,
            SYSTEM_LANGUAGES[option_index].name, { 245, 245, 250, 255 });
        SDL_SetRenderDrawColor(text_renderer.renderer, 100, 112, 139, 255);
        SDL_RenderLine(text_renderer.renderer, row_box.x, row_box.y + row_box.h,
            row_box.x + row_box.w, row_box.y + row_box.h);
    }
    SDL_SetRenderClipRect(text_renderer.renderer, nullptr);

    const SDL_Color finish_background = finish_focused
        ? SDL_Color{ 83, 177, 244, 255 }
        : SDL_Color{ 50, 53, 57, 255 };
    draw_rounded_rect(text_renderer.renderer, layout.finish_button, 12.f * layout.scale.x, finish_background);
    const SDL_Color finish_color{ 245, 245, 250, 255 };
    constexpr std::string_view finish_label = "Finish";
    const float finish_label_width = text_renderer.font.text_width(finish_label) * layout.scale.x;
    text_renderer.font.draw_text(text_renderer.renderer, layout.scale, layout.finish_button.x + (layout.finish_button.w - finish_label_width) / 2.f,
        layout.finish_button.y + text_renderer.font.centered_y(finish_label, layout.finish_button.h / layout.scale.y) * layout.scale.y,
        finish_label, finish_color);
}

bool run_initial_setup(EmuEnvState &emuenv, SDL_Window *window) {
    if (!window) {
        LOG_ERROR("SDL initial setup received no window.");
        return false;
    }

    const char *renderer_name = emuenv.backend_renderer == renderer::Backend::Vulkan ? SDL_GPU_RENDERER : "opengl";
    TextRenderer text_renderer{};
    text_renderer.renderer = SDL_CreateRenderer(window, renderer_name);
    if (!text_renderer.renderer) {
        LOG_ERROR("SDL initial setup renderer creation failed: {}", SDL_GetError());
        return false;
    }
    const auto &viewport = sdl::get_viewport();
    if (viewport.system_dpi_scale <= 0.f || viewport.logical_width <= 0.f || viewport.logical_height <= 0.f
        || viewport.scale_x <= 0.f || viewport.scale_y <= 0.f || viewport.manual_dpi_scale <= 0.f
        || !SDL_SetRenderScale(text_renderer.renderer, viewport.system_dpi_scale, viewport.system_dpi_scale)) {
        LOG_ERROR("SDL initial setup renderer scaling setup failed: {}", SDL_GetError());
        return false;
    }
    text_renderer.font.initialize(text_renderer.renderer, emuenv, window, "ltn4.pvf", 19, false);

    auto selected = std::find_if(SYSTEM_LANGUAGES.begin(), SYSTEM_LANGUAGES.end(), [&](const auto &option) {
        return static_cast<int>(option.id) == emuenv.cfg.sys_lang;
    });
    if (selected == SYSTEM_LANGUAGES.end()) {
        LOG_WARN("Unsupported system language {} in config; defaulting the SDL initial setup selection to English (United States).", emuenv.cfg.sys_lang);
        selected = std::find_if(SYSTEM_LANGUAGES.begin(), SYSTEM_LANGUAGES.end(), [](const auto &option) {
            return option.id == SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
        });
    }

    size_t selected_option = static_cast<size_t>(std::distance(SYSTEM_LANGUAGES.begin(), selected));
    size_t first_option = 0;
    size_t focused_option = selected_option;
    bool finish_focused = false;
    set_setup_system_language(static_cast<int>(SYSTEM_LANGUAGES[selected_option].id));
    keep_selection_visible(selected_option, get_setup_layout().visible_rows, first_option);
    bool complete = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset_y = 0.f;

    while (!complete) {
        const auto &current_viewport = sdl::get_viewport();
        if (current_viewport.system_dpi_scale <= 0.f || current_viewport.logical_width <= 0.f || current_viewport.logical_height <= 0.f
            || current_viewport.scale_x <= 0.f || current_viewport.scale_y <= 0.f || current_viewport.manual_dpi_scale <= 0.f
            || !SDL_SetRenderScale(text_renderer.renderer, current_viewport.system_dpi_scale, current_viewport.system_dpi_scale)) {
            LOG_ERROR("SDL initial setup renderer scaling update failed: {}", SDL_GetError());
            return false;
        }

        const auto layout = get_setup_layout();
        draw_setup(text_renderer, selected_option, focused_option, finish_focused, first_option);
        if (!SDL_RenderPresent(text_renderer.renderer)) {
            LOG_ERROR("SDL initial setup rendering failed: {}", SDL_GetError());
            return false;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const auto shared_event = sdl::process_event(emuenv, event);
            if (shared_event.outcome == sdl::EventOutcome::QuitRequested)
                return false;

            if ((event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                && !sdl::update_viewport(window, emuenv.cfg.current_config.stretch_the_display_area,
                    emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load())) {
                LOG_ERROR("SDL initial setup could not update the viewport: {}", SDL_GetError());
                return false;
            }

            const auto move_selection = [&](int offset) {
                const auto last_option = static_cast<int>(SYSTEM_LANGUAGES.size() - 1);
                focused_option = static_cast<size_t>(std::clamp(static_cast<int>(focused_option) + offset, 0, last_option));
                finish_focused = false;
                keep_selection_visible(focused_option, layout.visible_rows, first_option);
            };

            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP) {
                    if (finish_focused) {
                        finish_focused = false;
                        focused_option = selected_option;
                        keep_selection_visible(focused_option, layout.visible_rows, first_option);
                    } else
                        move_selection(-1);
                } else if (event.key.key == SDLK_DOWN) {
                    if (!finish_focused && focused_option + 1 == SYSTEM_LANGUAGES.size())
                        finish_focused = true;
                    else if (!finish_focused)
                        move_selection(1);
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER
                    || shared_event.vita_button == SCE_CTRL_CROSS) {
                    if (finish_focused)
                        complete = true;
                    else {
                        selected_option = focused_option;
                        set_setup_system_language(static_cast<int>(SYSTEM_LANGUAGES[selected_option].id));
                        finish_focused = true;
                    }
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                if (shared_event.vita_button == SCE_CTRL_UP) {
                    if (finish_focused) {
                        finish_focused = false;
                        focused_option = selected_option;
                        keep_selection_visible(focused_option, layout.visible_rows, first_option);
                    } else
                        move_selection(-1);
                } else if (shared_event.vita_button == SCE_CTRL_DOWN) {
                    if (!finish_focused && focused_option + 1 == SYSTEM_LANGUAGES.size())
                        finish_focused = true;
                    else if (!finish_focused)
                        move_selection(1);
                } else if (shared_event.vita_button == SCE_CTRL_CROSS) {
                    if (finish_focused)
                        complete = true;
                    else {
                        selected_option = focused_option;
                        set_setup_system_language(static_cast<int>(SYSTEM_LANGUAGES[selected_option].id));
                        finish_focused = true;
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float render_x = 0.f;
                float render_y = 0.f;
                if (!ui::window_to_render_coordinates(text_renderer.renderer, event.motion.x, event.motion.y,
                        render_x, render_y)) {
                    LOG_WARN_ONCE("SDL initial setup could not map mouse coordinates: {}", SDL_GetError());
                } else if (scrollbar_dragging) {
                    const auto scrollbar = get_scrollbar_layout(layout, first_option);
                    const float thumb_travel = scrollbar.track.h - scrollbar.thumb.h;
                    if (thumb_travel > 0.f && scrollbar.max_first_index > 0) {
                        const float thumb_top = std::clamp(render_y - scrollbar_drag_offset_y,
                            scrollbar.track.y, scrollbar.track.y + thumb_travel);
                        const float scroll_fraction = (thumb_top - scrollbar.track.y) / thumb_travel;
                        first_option = static_cast<size_t>(std::lround(
                            scroll_fraction * static_cast<float>(scrollbar.max_first_index)));
                    }
                } else if (ui::point_in_rect(layout.list, render_x, render_y)) {
                    const auto row = static_cast<size_t>((render_y - layout.list.y) / layout.row_height);
                    const auto option = first_option + row;
                    if (option < SYSTEM_LANGUAGES.size())
                        focused_option = option;
                    finish_focused = false;
                } else {
                    finish_focused = ui::point_in_rect(layout.finish_button, render_x, render_y);
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float render_x = 0.f;
                float render_y = 0.f;
                if (!ui::window_to_render_coordinates(text_renderer.renderer, event.button.x, event.button.y,
                        render_x, render_y)) {
                    LOG_WARN_ONCE("SDL initial setup could not map mouse coordinates: {}", SDL_GetError());
                } else if (const auto scrollbar = get_scrollbar_layout(layout, first_option);
                    scrollbar.max_first_index > 0 && ui::point_in_rect(scrollbar.thumb, render_x, render_y)) {
                    scrollbar_dragging = true;
                    scrollbar_drag_offset_y = render_y - scrollbar.thumb.y;
                } else if (ui::point_in_rect(layout.finish_button, render_x, render_y)) {
                    complete = true;
                } else if (ui::point_in_rect(layout.list, render_x, render_y)) {
                    const auto row = static_cast<size_t>((render_y - layout.list.y) / layout.row_height);
                    const auto option = first_option + row;
                    if (option < SYSTEM_LANGUAGES.size()) {
                        selected_option = option;
                        focused_option = option;
                        finish_focused = false;
                        set_setup_system_language(static_cast<int>(SYSTEM_LANGUAGES[selected_option].id));
                        keep_selection_visible(focused_option, layout.visible_rows, first_option);
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                scrollbar_dragging = false;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                const float wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
                const auto max_first_option = SYSTEM_LANGUAGES.size() > layout.visible_rows
                    ? SYSTEM_LANGUAGES.size() - layout.visible_rows
                    : 0;
                if (wheel_y > 0.f)
                    first_option = first_option > 2 ? first_option - 2 : 0;
                else if (wheel_y < 0.f)
                    first_option = std::min(first_option + 2, max_first_option);
            }
            if (complete)
                break;
        }
        if (!complete)
            SDL_Delay(16);
    }

    const int previous_language = emuenv.cfg.sys_lang;
    const bool previous_initial_setup = emuenv.cfg.initial_setup;
    emuenv.cfg.sys_lang = static_cast<int>(SYSTEM_LANGUAGES[selected_option].id);
    if (previous_language != emuenv.cfg.sys_lang && !app::scan_apps(emuenv)) {
        emuenv.cfg.sys_lang = previous_language;
        LOG_ERROR("SDL initial setup could not refresh app titles for system language {}.", static_cast<int>(SYSTEM_LANGUAGES[selected_option].id));
        return false;
    }

    emuenv.cfg.initial_setup = true;
    if (config::serialize_config(emuenv.cfg, emuenv.cfg.config_path) != Success) {
        emuenv.cfg.sys_lang = previous_language;
        emuenv.cfg.initial_setup = previous_initial_setup;
        LOG_ERROR("SDL initial setup could not save the selected system language.");
        return false;
    }

    return true;
}

} // namespace sdl_frontend