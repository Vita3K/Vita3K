// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/functions.h>

#include <app/state.h>
#include <ctrl/ctrl.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <overlay/controls.h>
#include <util/fs.h>
#include <util/log.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sdl_frontend {

constexpr float USER_LIST_TOP = 44.f;
constexpr float USER_LIST_LEFT = 20.f;
constexpr float USER_CARD_WIDTH = 130.f;
constexpr float USER_CARD_HEIGHT = 190.f;
constexpr float USER_CARD_GAP = 20.f;
constexpr float USER_ICON_SIZE = 130.f;
constexpr float USER_SMALL_ICON_SIZE = 34.f;
constexpr float USER_FOOTER_SEPARATOR = 66.f;

struct UserIconTexture {
    SDL_Texture *texture{};
    int width{};
    int height{};
};

struct UserSelectorState {
    size_t selected_user_index = 0;
    size_t first_visible_user = 0;
};

static std::unordered_map<std::string, UserIconTexture> user_icon_textures;
static SDL_Texture *user_management_background{};
static bool user_management_background_loaded = false;
static UserSelectorState user_selector_state;

static SDL_Texture *load_user_management_background(EmuEnvState &emuenv, SDL_Renderer *renderer) {
    if (user_management_background_loaded)
        return user_management_background;
    user_management_background_loaded = true;

    const auto path = emuenv.vita_fs_path / "vs0/app/NPXS10013/sce_sys/pic0.png";
    std::vector<uint8_t> bytes;
    if (!fs_utils::read_data(path, bytes)) {
        LOG_WARN("Background not found for user management screen at '{}'.", path);
        return nullptr;
    }

    overlay::image_info image(bytes);
    if (!image.get_data() || image.w <= 0 || image.h <= 0) {
        LOG_WARN("Could not decode user management background '{}'.", path);
        return nullptr;
    }

    auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
    if (!texture) {
        LOG_WARN("Could not create user management background texture: {}", SDL_GetError());
        return nullptr;
    }
    if (!SDL_UpdateTexture(texture, nullptr, image.get_data(), image.w * 4)
        || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        LOG_WARN("Could not upload user management background: {}", SDL_GetError());
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    user_management_background = texture;
    return user_management_background;
}

static UserIconTexture load_user_icon(SDL_Renderer *renderer, const fs::path &path) {
    std::vector<uint8_t> bytes;
    if (!fs_utils::read_data(path, bytes)) {
        LOG_WARN("Could not read user avatar '{}'.", path);
        return {};
    }

    overlay::image_info image(bytes);
    if (!image.get_data() || image.w <= 0 || image.h <= 0) {
        LOG_WARN("Could not decode user avatar '{}'.", path);
        return {};
    }

    auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
    if (!texture) {
        LOG_WARN("Could not create user avatar texture '{}': {}", path, SDL_GetError());
        return {};
    }
    if (!SDL_UpdateTexture(texture, nullptr, image.get_data(), image.w * 4)
        || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)
        || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        LOG_WARN("Could not upload user avatar '{}': {}", path, SDL_GetError());
        SDL_DestroyTexture(texture);
        return {};
    }
    return { texture, image.w, image.h };
}

static UserIconTexture get_user_icon(EmuEnvState &emuenv, SDL_Renderer *renderer, const std::string &avatar) {
    const bool use_default_avatar = avatar == "default";
    const auto path = use_default_avatar
        ? emuenv.static_assets_path / "data/image/icon.png"
        : fs_utils::utf8_to_path(avatar);
    const auto key = fs_utils::path_to_utf8(path);
    if (const auto found = user_icon_textures.find(key); found != user_icon_textures.end())
        return found->second;

    auto icon = load_user_icon(renderer, path);
    if (!icon.texture && !use_default_avatar) {
        LOG_WARN("Using the default avatar for user profile '{}'.", avatar);
        icon = get_user_icon(emuenv, renderer, "default");
    }
    user_icon_textures.emplace(key, icon);
    return icon;
}

static void draw_user_icon(EmuEnvState &emuenv, SDL_Renderer *renderer, const app::User &user,
    float icon_size, CoordinateScale coordinate_scale, float viewport_x, float viewport_y, float x, float y) {
    const auto icon = get_user_icon(emuenv, renderer, user.avatar);
    if (!icon.texture)
        return;

    const float target_width = icon_size * coordinate_scale.x;
    const float target_height = icon_size * coordinate_scale.y;
    const float aspect_scale = std::min(target_width / static_cast<float>(icon.width),
        target_height / static_cast<float>(icon.height));
    const float width = static_cast<float>(icon.width) * aspect_scale;
    const float height = static_cast<float>(icon.height) * aspect_scale;
    SDL_FRect destination{
        viewport_x + x * coordinate_scale.x + (target_width - width) / 2.f,
        viewport_y + y * coordinate_scale.y + (target_height - height) / 2.f,
        width,
        height
    };
    if (!SDL_RenderTexture(renderer, icon.texture, nullptr, &destination))
        LOG_WARN_ONCE("SDL user avatar rendering failed: {}", SDL_GetError());
}

static auto user_at_index(EmuEnvState &emuenv, size_t index) {
    auto user = emuenv.app.user_list.users.begin();
    for (size_t current = 0; current < index && user != emuenv.app.user_list.users.end(); ++current)
        ++user;
    return user;
}

size_t user_selector_visible_items(float logical_window_width) {
    const float list_width = std::max(0.f, logical_window_width - (USER_LIST_LEFT * 2.f));
    const float item_pitch = USER_CARD_WIDTH + USER_CARD_GAP;
    return static_cast<size_t>(std::max(1.f, std::floor((list_width + USER_CARD_GAP) / item_pitch)));
}

void begin_user_selector(EmuEnvState &emuenv) {
    user_selector_state.selected_user_index = 0;
    for (const auto &[user_id, user] : emuenv.app.user_list.users) {
        if (user_id == emuenv.io.user_id)
            break;
        ++user_selector_state.selected_user_index;
    }
    user_selector_state.first_visible_user = 0;
}

void reset_user_selector() {
    user_selector_state.selected_user_index = 0;
    user_selector_state.first_visible_user = 0;
}

static float user_cards_top(float logical_window_height) {
    return USER_LIST_TOP + std::max(0.f, (logical_window_height - USER_LIST_TOP - USER_CARD_HEIGHT) / 2.f);
}

size_t user_selector_item_at(float x, float y, CoordinateScale window_scale, float logical_window_height,
    size_t first_item, size_t visible_items, size_t user_count) {
    const float logical_x = x / window_scale.x;
    const float logical_y = y / window_scale.y;
    const float cards_top = user_cards_top(logical_window_height);
    if (logical_x < USER_LIST_LEFT || logical_y < cards_top
        || logical_y >= cards_top + USER_CARD_HEIGHT)
        return user_count;
    const float item_offset = logical_x - USER_LIST_LEFT;
    const float item_pitch = USER_CARD_WIDTH + USER_CARD_GAP;
    const auto column = static_cast<size_t>(item_offset / item_pitch);
    if (column >= visible_items || item_offset - static_cast<float>(column) * item_pitch >= USER_CARD_WIDTH
        || first_item + column >= user_count)
        return user_count;
    return first_item + column;
}

static void move_user_selection(EmuEnvState &emuenv, int offset) {
    const auto user_count = emuenv.app.user_list.users.size();
    if (user_count == 0)
        return;
    const auto last_index = static_cast<int64_t>(user_count - 1);
    user_selector_state.selected_user_index = static_cast<size_t>(std::clamp(
        static_cast<int64_t>(user_selector_state.selected_user_index) + offset, int64_t{ 0 }, last_index));
}

static bool event_to_render_coordinates(SDL_Renderer *renderer, float window_x, float window_y,
    float &render_x, float &render_y) {
    if (!renderer || !SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &render_x, &render_y)) {
        LOG_WARN_ONCE("SDL user selector could not map window coordinates to its logical viewport: {}", SDL_GetError());
        return false;
    }
    return true;
}

static std::optional<std::string> selected_user_id(EmuEnvState &emuenv) {
    const auto user = user_at_index(emuenv, user_selector_state.selected_user_index);
    if (user == emuenv.app.user_list.users.end())
        return std::nullopt;
    return user->first;
}

std::optional<std::string> handle_user_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window,
    SDL_Renderer *renderer, const SDL_Event &event, uint32_t vita_button, CoordinateScale coordinate_scale,
    float viewport_x, float viewport_y, float logical_window_width, float logical_window_height) {
    auto &user_state = user_selector_state;
    const auto user_count = emuenv.app.user_list.users.size();
    int window_width = 0;
    int window_height = 0;
    if (window)
        SDL_GetWindowSize(window, &window_width, &window_height);

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        const bool navigation_repeat = event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT
            || event.key.key == SDLK_PAGEUP || event.key.key == SDLK_PAGEDOWN
            || vita_button == SCE_CTRL_LEFT || vita_button == SCE_CTRL_RIGHT;
        if (event.key.repeat && !navigation_repeat)
            break;
        if (event.key.key == SDLK_ESCAPE || vita_button == SCE_CTRL_CIRCLE) {
            state.screen = FrontendScreen::AppSelector;
        } else if (event.key.key == SDLK_LEFT || vita_button == SCE_CTRL_LEFT) {
            move_user_selection(emuenv, -1);
        } else if (event.key.key == SDLK_RIGHT || vita_button == SCE_CTRL_RIGHT) {
            move_user_selection(emuenv, 1);
        } else if (event.key.key == SDLK_PAGEUP) {
            move_user_selection(emuenv, -static_cast<int>(user_selector_visible_items(logical_window_width)));
        } else if (event.key.key == SDLK_PAGEDOWN) {
            move_user_selection(emuenv, static_cast<int>(user_selector_visible_items(logical_window_width)));
        } else if (vita_button == SCE_CTRL_CROSS || event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
            return selected_user_id(emuenv);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (vita_button == SCE_CTRL_LEFT)
            move_user_selection(emuenv, -1);
        else if (vita_button == SCE_CTRL_RIGHT)
            move_user_selection(emuenv, 1);
        else if (vita_button == SCE_CTRL_CROSS)
            return selected_user_id(emuenv);
        else if (vita_button == SCE_CTRL_CIRCLE)
            state.screen = FrontendScreen::AppSelector;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (event.wheel.x != 0.f || event.wheel.y != 0.f) {
            const float wheel_delta = event.wheel.x != 0.f ? event.wheel.x : event.wheel.y;
            move_user_selection(emuenv, wheel_delta > 0.f ? -1 : 1);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        float logical_x = 0.f;
        float logical_y = 0.f;
        if (!event_to_render_coordinates(renderer, event.motion.x, event.motion.y, logical_x, logical_y))
            break;
        logical_x = (logical_x - viewport_x) / coordinate_scale.x;
        logical_y = (logical_y - viewport_y) / coordinate_scale.y;
        const auto visible_items = user_selector_visible_items(logical_window_width);
        const auto item = user_selector_item_at(logical_x, logical_y, {}, logical_window_height,
            user_state.first_visible_user, visible_items, user_count);
        if (item < user_count)
            user_state.selected_user_index = item;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
            float logical_x = 0.f;
            float logical_y = 0.f;
            if (!event_to_render_coordinates(renderer, event.button.x, event.button.y, logical_x, logical_y))
                break;
            logical_x = (logical_x - viewport_x) / coordinate_scale.x;
            logical_y = (logical_y - viewport_y) / coordinate_scale.y;
            const auto visible_items = user_selector_visible_items(logical_window_width);
            const auto item = user_selector_item_at(logical_x, logical_y, {}, logical_window_height,
                user_state.first_visible_user, visible_items, user_count);
            if (item < user_count) {
                user_state.selected_user_index = item;
                return selected_user_id(emuenv);
            }
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
        if (window_height > 0) {
            float logical_x = 0.f;
            float logical_y = 0.f;
            if (!event_to_render_coordinates(renderer,
                    event.tfinger.x * static_cast<float>(window_width),
                    event.tfinger.y * static_cast<float>(window_height), logical_x, logical_y))
                break;
            logical_x = (logical_x - viewport_x) / coordinate_scale.x;
            logical_y = (logical_y - viewport_y) / coordinate_scale.y;
            const auto visible_items = user_selector_visible_items(logical_window_width);
            const auto item = user_selector_item_at(logical_x, logical_y, {}, logical_window_height,
                user_state.first_visible_user, visible_items, user_count);
            if (item < user_count) {
                user_state.selected_user_index = item;
                return selected_user_id(emuenv);
            }
        }
        break;
    default:
        break;
    }
    return std::nullopt;
}

void render_user_selector(EmuEnvState &emuenv, SDL_Renderer *renderer, CoordinateScale coordinate_scale,
    float viewport_x, float viewport_y, float logical_window_width, float logical_window_height) {
    auto &user_state = user_selector_state;
    if (!renderer)
        return;

    SDL_SetRenderDrawColor(renderer, 10, 50, 140, 255);
    SDL_RenderClear(renderer);
    const auto background = load_user_management_background(emuenv, renderer);
    if (background) {
        SDL_FRect background_rect{
            viewport_x,
            viewport_y,
            logical_window_width * coordinate_scale.x,
            logical_window_height * coordinate_scale.y
        };
        if (!SDL_RenderTexture(renderer, background, nullptr, &background_rect))
            LOG_WARN_ONCE("SDL user management background rendering failed: {}", SDL_GetError());
    }
    draw_selector_text(renderer, coordinate_scale, viewport_x + 54.f * coordinate_scale.x,
        viewport_y + 24.f * coordinate_scale.y, "Select User", { 235, 238, 248, 255 });
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    if (!SDL_RenderLine(renderer, viewport_x, viewport_y + 66.f * coordinate_scale.y,
            viewport_x + logical_window_width * coordinate_scale.x,
            viewport_y + 66.f * coordinate_scale.y))
        LOG_WARN_ONCE("SDL user management separator rendering failed: {}", SDL_GetError());

    const auto user_count = emuenv.app.user_list.users.size();
    const auto visible_users = user_selector_visible_items(logical_window_width);
    const float cards_top = user_cards_top(logical_window_height);
    const float list_clip_width = std::max(0.f, logical_window_width - (USER_LIST_LEFT * 2.f));

    if (user_count > 0) {
        user_state.selected_user_index = std::min(user_state.selected_user_index, user_count - 1);
        if (user_state.selected_user_index < user_state.first_visible_user)
            user_state.first_visible_user = user_state.selected_user_index;
        else if (user_state.selected_user_index >= user_state.first_visible_user + visible_users)
            user_state.first_visible_user = user_state.selected_user_index - visible_users + 1;
        user_state.first_visible_user = std::min(user_state.first_visible_user,
            user_count > visible_users ? user_count - visible_users : 0);

        SDL_Rect user_clip{
            static_cast<int>(viewport_x + USER_LIST_LEFT * coordinate_scale.x),
            static_cast<int>(viewport_y + USER_LIST_TOP * coordinate_scale.y),
            static_cast<int>(list_clip_width * coordinate_scale.x),
            static_cast<int>(std::max(0.f, logical_window_height - USER_LIST_TOP) * coordinate_scale.y)
        };
        SDL_SetRenderClipRect(renderer, &user_clip);
        for (size_t column = 0; column < visible_users && user_state.first_visible_user + column < user_count; ++column) {
            const auto index = user_state.first_visible_user + column;
            const auto profile = user_at_index(emuenv, index);
            if (profile == emuenv.app.user_list.users.end())
                break;
            const float x = USER_LIST_LEFT + static_cast<float>(column) * (USER_CARD_WIDTH + USER_CARD_GAP);
            SDL_FRect user_box{
                viewport_x + x * coordinate_scale.x,
                viewport_y + cards_top * coordinate_scale.y,
                USER_CARD_WIDTH * coordinate_scale.x,
                USER_CARD_HEIGHT * coordinate_scale.y
            };
            SDL_FRect avatar_background{
                user_box.x,
                user_box.y,
                USER_CARD_WIDTH * coordinate_scale.x,
                USER_ICON_SIZE * coordinate_scale.y
            };
            SDL_FRect name_background{
                user_box.x,
                user_box.y + avatar_background.h,
                user_box.w,
                user_box.h - avatar_background.h
            };
            SDL_SetRenderDrawColor(renderer, 19, 69, 167, 150);
            SDL_RenderFillRect(renderer, &avatar_background);
            SDL_SetRenderDrawColor(renderer, 19, 69, 167, 255);
            SDL_RenderFillRect(renderer, &name_background);
            draw_user_icon(emuenv, renderer, profile->second, USER_ICON_SIZE, coordinate_scale,
                viewport_x, viewport_y, x + (USER_CARD_WIDTH - USER_ICON_SIZE) / 2.f, cards_top);
            std::string label = profile->second.name.empty() ? "User " + profile->first : profile->second.name;
            draw_selector_text(renderer, coordinate_scale, viewport_x + (x + 8.f) * coordinate_scale.x,
                viewport_y + (cards_top + USER_ICON_SIZE + 8.f) * coordinate_scale.y,
                label.c_str(), { 235, 238, 248, 255 });
            if (index == user_state.selected_user_index) {
                SDL_SetRenderDrawColor(renderer, 235, 238, 248, 255);
                SDL_RenderRect(renderer, &user_box);
            }
        }
        SDL_SetRenderClipRect(renderer, nullptr);
    } else {
        draw_selector_text(renderer, coordinate_scale, viewport_x + 24.f * coordinate_scale.x,
            viewport_y + USER_LIST_TOP * coordinate_scale.y, "No users found.", { 235, 238, 248, 255 });
    }
    const auto current_user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    if (current_user != emuenv.app.user_list.users.end()) {
        const float footer_y = logical_window_height - USER_FOOTER_SEPARATOR;
        SDL_SetRenderDrawColor(renderer, 24, 26, 38, 190);
        SDL_RenderLine(renderer, viewport_x, viewport_y + footer_y * coordinate_scale.y,
            viewport_x + logical_window_width * coordinate_scale.x, viewport_y + footer_y * coordinate_scale.y);
        draw_user_icon(emuenv, renderer, current_user->second, USER_SMALL_ICON_SIZE, coordinate_scale,
            viewport_x, viewport_y, std::max(0.f, logical_window_width - 220.f),
            logical_window_height - USER_FOOTER_SEPARATOR / 2.f - USER_SMALL_ICON_SIZE / 2.f);
        draw_selector_text(renderer, coordinate_scale,
            viewport_x + (logical_window_width - 180.f) * coordinate_scale.x,
            viewport_y + (logical_window_height - USER_FOOTER_SEPARATOR / 2.f - 11.f) * coordinate_scale.y,
            current_user->second.name.c_str(), { 235, 238, 248, 255 });
    }
}

void destroy_user_selector_icons() {
    std::unordered_set<SDL_Texture *> destroyed_textures;
    for (const auto &[_, icon] : user_icon_textures) {
        if (icon.texture && destroyed_textures.insert(icon.texture).second)
            SDL_DestroyTexture(icon.texture);
    }
    user_icon_textures.clear();
    if (user_management_background)
        SDL_DestroyTexture(user_management_background);
    user_management_background = nullptr;
    user_management_background_loaded = false;
}

} // namespace sdl_frontend