// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/font_renderer.h>

#include <config/state.h>
#include <emuenv/state.h>
#include <overlay/element.h>
#include <overlay/font.h>
#include <sdl/frame_host.h>
#include <util/fs.h>
#include <util/log.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sdl_frontend::ui {

void draw_background(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color color) {
    SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
    const bool background_blended = SDL_GetRenderDrawBlendMode(renderer, &previous_blend_mode)
        && SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (!background_blended)
        LOG_WARN_ONCE("Could not enable SDL blending for the UI background: {}", SDL_GetError());

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, background_blended ? color.a : 255);
    if (!SDL_RenderFillRect(renderer, &rect))
        LOG_WARN_ONCE("SDL UI background rendering failed: {}", SDL_GetError());

    if (background_blended && !SDL_SetRenderDrawBlendMode(renderer, previous_blend_mode))
        LOG_WARN_ONCE("Could not restore SDL draw blending after the UI background: {}", SDL_GetError());
}

void draw_inset_border(SDL_Renderer *renderer, const SDL_FRect &rect, float inset_x, float inset_y,
    SDL_Color color) {
    const SDL_FRect inner_rect{
        rect.x + inset_x,
        rect.y + inset_y,
        rect.w - 2.f * inset_x,
        rect.h - 2.f * inset_y
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(renderer, &inner_rect);
}

void draw_rect(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void draw_scrollbar(SDL_Renderer *renderer, const SDL_FRect &track, const SDL_FRect &thumb,
    SDL_Color track_color, SDL_Color thumb_color) {
    draw_rect(renderer, track, track_color);
    draw_rect(renderer, thumb, thumb_color);
}

void draw_selection_highlight(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color fill_color,
    const SDL_Color *outline_color) {
    draw_rect(renderer, rect, fill_color);
    if (outline_color) {
        SDL_SetRenderDrawColor(renderer, outline_color->r, outline_color->g, outline_color->b, outline_color->a);
        SDL_RenderRect(renderer, &rect);
    }
}

bool point_in_rect(const SDL_FRect &rect, float x, float y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

std::string to_ascii(std::string_view text) {
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

bool window_to_render_coordinates(SDL_Renderer *renderer, float window_x, float window_y,
    float &render_x, float &render_y) {
    return renderer && SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &render_x, &render_y);
}

float FontRenderer::centered_y(std::string_view text, float area_height) const {
    if (!font)
        return (area_height - 8.f) / 2.f;

    const auto unicode_text = overlay::utf8_to_u32string(text);
    const auto glyphs = font->render_text(unicode_text.c_str());
    if (glyphs.empty())
        return (area_height - static_cast<float>(font->get_size_px())) / 2.f;

    float min_y = glyphs.front().y();
    float max_y = min_y;
    for (const auto &glyph : glyphs) {
        min_y = std::min(min_y, glyph.y());
        max_y = std::max(max_y, glyph.y());
    }
    return area_height / 2.f - static_cast<float>(font->get_size_px()) - (min_y + max_y) / 2.f;
}

bool FontRenderer::draw_font(SceFVector2 coordinate_scale, float x, float y,
    std::string_view text, SDL_Color color) {
    if (!font || unavailable)
        return false;

    const auto unicode_text = overlay::utf8_to_u32string(text);
    const auto glyphs = font->render_text(unicode_text.c_str());
    if (glyphs.empty() || !ensure_textures())
        return false;

    struct Batch {
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
    };
    std::vector<Batch> batches(textures.size());

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
                                           y + (font->get_size_px() + glyph.y()) * coordinate_scale.y },
                .color = { static_cast<float>(color.r) / 255.f, static_cast<float>(color.g) / 255.f, static_cast<float>(color.b) / 255.f, static_cast<float>(color.a) / 255.f },
                .tex_coord = { glyph.z(), glyph.w() - static_cast<float>(page) } });
        }
        batch.indices.insert(batch.indices.end(), { base_vertex, base_vertex + 1, base_vertex + 2, base_vertex + 1, base_vertex + 3, base_vertex + 2 });
    }

    for (size_t page = 0; page < batches.size(); ++page) {
        auto &batch = batches[page];
        if (batch.indices.empty())
            continue;
        if (!SDL_RenderGeometry(renderer, textures[page], batch.vertices.data(),
                static_cast<int>(batch.vertices.size()), batch.indices.data(), static_cast<int>(batch.indices.size()))) {
            LOG_WARN_ONCE("SDL UI font rendering failed: {}", SDL_GetError());
            unavailable = true;
            return false;
        }
    }

    return true;
}

void FontRenderer::draw_text(SDL_Renderer *renderer_, SceFVector2 coordinate_scale, float x, float y,
    std::string_view text, SDL_Color color) {
    if (renderer_ != renderer) {
        LOG_ERROR("SDL UI text renderer does not match its initialized renderer.");
        return;
    }
    if (draw_font(coordinate_scale, x, y, text, color))
        return;
    if (!renderer_)
        return;

    const auto ascii_text = to_ascii(text);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(renderer_, x, y, ascii_text.c_str());
}

bool FontRenderer::ensure_textures() {
    if (!font || !renderer)
        return false;

    const auto dimensions = font->get_glyph_data_dimensions();
    if (dimensions.depth <= textures.size())
        return true;

    const auto &glyph_data = font->get_glyph_data();
    const size_t page_size = static_cast<size_t>(dimensions.width) * dimensions.height;
    for (size_t page = textures.size(); page < dimensions.depth; ++page) {
        auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            static_cast<int>(dimensions.width), static_cast<int>(dimensions.height));
        if (!texture) {
            LOG_WARN_ONCE("Could not create SDL UI font texture: {}", SDL_GetError());
            unavailable = true;
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
            LOG_WARN_ONCE("Could not upload SDL UI font texture: {}", SDL_GetError());
            SDL_DestroyTexture(texture);
            unavailable = true;
            return false;
        }
        textures.push_back(texture);
    }

    return true;
}

void FontRenderer::initialize(SDL_Renderer *renderer_, EmuEnvState &emuenv, SDL_Window *window,
    std::string_view font_name, int size, bool bold) {
    release();
    if (!renderer_ || !window) {
        LOG_ERROR("SDL UI font renderer received no renderer or window.");
        return;
    }

    renderer = renderer_;
    sdl::FrameHost frame_host(window, nullptr);
    overlay::fontmgr::set_system_lang(emuenv.cfg.sys_lang);
    overlay::fontmgr::set_system_font_dirs(frame_host.font_dirs());

    auto firmware_font_dir = fs_utils::path_to_utf8(emuenv.vita_fs_path / "sa0" / "data" / "font" / "pvf");
    if (!firmware_font_dir.empty() && firmware_font_dir.back() != '/' && firmware_font_dir.back() != '\\')
        firmware_font_dir += '/';
    overlay::fontmgr::set_firmware_font_dir(firmware_font_dir);
    font = overlay::fontmgr::get(font_name, size, bold);
}

void FontRenderer::release() {
    for (auto *texture : textures)
        SDL_DestroyTexture(texture);
    textures.clear();
    renderer = nullptr;
    font = nullptr;
    unavailable = false;
}

float FontRenderer::text_width(std::string_view text) const {
    if (!font || unavailable)
        return static_cast<float>(text.size()) * 8.f;

    const auto unicode_text = overlay::utf8_to_u32string(text);
    return font->get_char_offset(unicode_text.c_str(), unicode_text.size()).first;
}

FontRenderer::~FontRenderer() {
    release();
}

} // namespace sdl_frontend::ui