// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <util/types.h>

#include <SDL3/SDL.h>

#include <string_view>
#include <vector>

struct EmuEnvState;
struct SDL_Window;

namespace overlay {
class font;
}

namespace sdl_frontend::ui {

class FontRenderer {
public:
    FontRenderer() = default;
    ~FontRenderer();

    FontRenderer(const FontRenderer &) = delete;
    FontRenderer &operator=(const FontRenderer &) = delete;

    void initialize(SDL_Renderer *renderer, EmuEnvState &emuenv, SDL_Window *window,
        std::string_view font_name, int size, bool bold);
    void release();
    void draw_text(SDL_Renderer *renderer, SceFVector2 coordinate_scale, float x, float y,
        std::string_view text, SDL_Color color);
    float centered_y(std::string_view text, float area_height) const;
    float text_width(std::string_view text) const;

private:
    bool ensure_textures();
    bool draw_font(SceFVector2 coordinate_scale, float x, float y, std::string_view text, SDL_Color color);

    SDL_Renderer *renderer{};
    overlay::font *font{};
    std::vector<SDL_Texture *> textures;
    bool unavailable{};
};

} // namespace sdl_frontend::ui