// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <gui/functions.h>

#include "private.h"

#include <gui/imgui_impl_sdl_gl3.h>

#include <glutil/gl.h>
#include <host/state.h>
#include <io/io.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL_video.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <string>

namespace gui {

static void init_style() {
    ImGui::StyleColorsDark();

    ImGuiStyle *style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(11, 11);
    style->WindowRounding = 4.0f;
    style->FramePadding = ImVec2(4, 4);
    style->FrameRounding = 3.0f;
    style->ItemSpacing = ImVec2(10, 5);
    style->ItemInnerSpacing = ImVec2(6, 5);
    style->IndentSpacing = 20.0f;
    style->ScrollbarSize = 12.0f;
    style->ScrollbarRounding = 8.0f;
    style->GrabMinSize = 4.0f;
    style->GrabRounding = 2.5f;

    style->Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.80f);
    style->Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.90f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.90f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.80f, 0.88f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 0.80f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 0.40f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 0.70f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 0.90f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.46f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.55f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.55f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.32f, 0.30f, 0.23f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_Column] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ColumnHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_ColumnActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.18f, 0.20f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    style->Colors[ImGuiCol_Tab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_TabHovered] = ImVec4(0.32f, 0.30f, 0.23f, 1.00f);
    style->Colors[ImGuiCol_TabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

static GLuint load_texture(int32_t width, int32_t height, const unsigned char *data, GLenum type = GL_RGBA) {
    GLuint texture;
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, type, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return texture;
}

static void init_font(State &gui) {
    const auto DATA_PATH = "data";
    const auto FONT_PATH = "fonts";
    const auto FONT_FILENAME = "mplus-1mn-bold.ttf";

    // set up font paths
    fs::path font_dir = fs::path(DATA_PATH) /= FONT_PATH;
    fs::path font_path(fs::absolute(font_dir /= FONT_FILENAME));

    // check existence of font file
    if (!fs::exists(font_path)) {
        LOG_WARN("Could not find font file at \"{}\", falling back to default imgui font.", font_path.string());
        return;
    }

    // read font
    const auto font_file_size = fs::file_size(font_path);
    gui.font_data.resize(font_file_size);
    std::ifstream font_stream(font_path.string().c_str(), std::ios::in | std::ios::binary);
    font_stream.read(gui.font_data.data(), font_file_size);

    // add it to imgui
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig font_config{};
    gui.monospaced_font = io.Fonts->AddFontDefault();
    gui.normal_font = io.Fonts->AddFontFromMemoryTTF(gui.font_data.data(), static_cast<int>(font_file_size), 16, &font_config, io.Fonts->GetGlyphRangesJapanese());
}

static void init_background(State &gui, const std::string &image_path) {
    if (!fs::exists(image_path)) {
        LOG_INFO("Invalid background file path {}.", image_path);
        return;
    }

    int32_t width = 0;
    int32_t height = 0;
    stbi_uc *data = stbi_load(image_path.c_str(), &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_INFO("Could not load image from {}.", image_path);
        return;
    }

    gui.background_texture.init(load_texture(width, height, data), glDeleteTextures);
    stbi_image_free(data);
}

static void init_icons(HostState &host) {
    for (Game &game : host.gui.game_selector.games) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        vfs::read_app_file(buffer, host.pref_path, game.title_id, "sce_sys/icon0.png");
        if (buffer.empty()) {
            LOG_INFO("Could not load icon or image for title {}.", game.title_id);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], buffer.size(), &width, &height, nullptr, STBI_rgb);
        if (width != 128 || height != 128) {
            LOG_INFO("Invalid icon or image for title {}.", game.title_id);
            continue;
        }
        host.gui.game_selector.icons[game.title_id].init(load_texture(width, height, data, GL_RGB), glDeleteTextures);
        stbi_image_free(data);
    }
}

void init(HostState &host) {
    ImGui::CreateContext();
    ImGui_ImplSdlGL3_Init(host.window.get());

    init_style();
    init_font(host.gui);
    init_icons(host);
    if (host.cfg.background_image) {
        init_background(host.gui, host.cfg.background_image.value());
    }
}

void draw_begin(HostState &host) {
    ImGui_ImplSdlGL3_NewFrame(host.window.get());
    host.gui.renderer_focused = !ImGui::GetIO().WantCaptureMouse;

    ImGui::PushFont(host.gui.normal_font);
}

void draw_end(SDL_Window *window) {
    ImGui::PopFont();

    glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    ImGui::Render();
    ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}

void draw_ui(HostState &host) {
    draw_main_menu_bar(host);

    ImGui::PushFont(host.gui.monospaced_font);
    if (host.gui.debug_menu.threads_dialog) {
        draw_threads_dialog(host);
    }
    if (host.gui.debug_menu.thread_details_dialog) {
        draw_thread_details_dialog(host);
    }
    if (host.gui.debug_menu.semaphores_dialog) {
        draw_semaphores_dialog(host);
    }
    if (host.gui.debug_menu.mutexes_dialog) {
        draw_mutexes_dialog(host);
    }
    if (host.gui.debug_menu.lwmutexes_dialog) {
        draw_lw_mutexes_dialog(host);
    }
    if (host.gui.debug_menu.condvars_dialog) {
        draw_condvars_dialog(host);
    }
    if (host.gui.debug_menu.lwcondvars_dialog) {
        draw_lw_condvars_dialog(host);
    }
    if (host.gui.debug_menu.eventflags_dialog) {
        draw_event_flags_dialog(host);
    }
    if (host.gui.debug_menu.allocations_dialog) {
        draw_allocations_dialog(host);
    }
    if (host.gui.debug_menu.disassembly_dialog) {
        draw_disassembly_dialog(host);
    }
    if (host.gui.configuration_menu.settings_dialog) {
        draw_settings_dialog(host);
    }
    if (host.gui.help_menu.controls_dialog) {
        draw_controls_dialog(host);
    }
    if (host.gui.help_menu.about_dialog) {
        draw_about_dialog(host);
    }
    ImGui::PopFont();
}

} // namespace gui
