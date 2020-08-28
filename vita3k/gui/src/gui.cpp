// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <gui/functions.h>

#include <gui/imgui_impl_sdl.h>

#include <boost/algorithm/string/trim.hpp>
#include <glutil/gl.h>
#include <host/functions.h>
#include <host/state.h>
#include <io/VitaIoDevice.h>
#include <io/vfs.h>
#include <util/fs.h>
#include <util/log.h>

#include <SDL_video.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <string>
#include <vector>

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
    style->Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
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
    style->Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.21f, 0.23f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.08f, 0.66f, 0.87f, 0.50f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.66f, 0.87f, 1.00f);
    style->Colors[ImGuiCol_Header] = ImVec4(1.00f, 1.00f, 0.00f, 0.50f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 1.00f, 0.00f, 0.30f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 1.00f, 0.00f, 0.70f);
    style->Colors[ImGuiCol_Separator] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_SeparatorActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
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
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 1.00f, 0.00f, 0.50f);
    style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

static void init_font(GuiState &gui, HostState &host) {
    const auto DATA_DIRNAME = "data";
    const auto FONT_DIRNAME = "fonts";
    const auto FONT_FILENAME = "mplus-1mn-bold.ttf";

    // set up font paths
    fs::path font_dir = fs::path(host.base_path) /= fs::path(DATA_DIRNAME) /= FONT_DIRNAME;
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
    gui.normal_font = io.Fonts->AddFontFromMemoryTTF(gui.font_data.data(), static_cast<int>(font_file_size), 16.f, &font_config, io.Fonts->GetGlyphRangesJapanese());
}

static void init_live_area_font(GuiState &gui, HostState &host) {
    const auto font_path{ fs::path(host.pref_path) / "sa0/data/font/pvf/jpn0.pvf" };

    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig font_config{};
    static const ImWchar large_font_chars[] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L':', L';', L'<', L'=', L'>' };

    // check existence of font file
    if (!fs::exists(font_path)) {
        LOG_WARN("Could not find firmware font file at \"{}\", using defaut vita3k font.", font_path.string());
        gui.live_area_font = gui.normal_font;
        gui.live_area_font_large = io.Fonts->AddFontFromMemoryTTF(gui.font_data.data(), static_cast<int>(gui.font_data.size()), 124.f, &font_config, large_font_chars);
        return;
    }

    // read font
    const auto font_file_size = fs::file_size(font_path);
    gui.live_area_font_data.resize(font_file_size);
    std::ifstream font_stream(font_path.string().c_str(), std::ios::in | std::ios::binary);
    font_stream.read(gui.live_area_font_data.data(), font_file_size);

    // add it to imgui
    gui.live_area_font = io.Fonts->AddFontFromMemoryTTF(gui.live_area_font_data.data(), static_cast<int>(font_file_size), 19.2f, &font_config, io.Fonts->GetGlyphRangesJapanese());
    gui.live_area_font_large = io.Fonts->AddFontFromMemoryTTF(gui.live_area_font_data.data(), static_cast<int>(font_file_size), 124.f, &font_config, large_font_chars);
}

static void init_user_backgrounds(GuiState &gui, HostState &host) {
    for (const auto &background : host.cfg.user_backgrounds) {
        if (!fs::exists(fs::path(background))) {
            LOG_WARN("Image doesn't exist: {}.", background);
            continue;
        }

        int32_t width = 0;
        int32_t height = 0;
        stbi_uc *data = stbi_load(background.c_str(), &width, &height, nullptr, STBI_rgb_alpha);

        if (!data) {
            LOG_ERROR("Invalid or corrupted image: {}.", background);
            continue;
        }

        gui.user_backgrounds[background].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    gui.current_user_bg = 0;
}

void init_apps_icon(GuiState &gui, HostState &host, const std::vector<App> &apps_list) {
    for (const App &app : apps_list) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        if (app.title_id.find("NPXS") != std::string::npos)
            vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + app.title_id + "/sce_sys/icon0.png");
        else
            vfs::read_app_file(buffer, host.pref_path, app.title_id, "sce_sys/icon0.png");

        const auto default_fw_icon{ fs::path(host.pref_path) / "vs0/data/internal/livearea/default/sce_sys/icon0.png" };
        const auto default_icon{ fs::path(host.base_path) / "data/image/icon.png" };

        if ((buffer.empty() && (fs::exists(default_fw_icon) || fs::exists(default_icon)))) {
            LOG_INFO("Default icon found for title {}, {}.", app.title_id, app.title);
            std::ifstream image_stream(default_fw_icon.string(), std::ios::binary | std::ios::ate);
            if (!fs::exists(default_fw_icon))
                image_stream = std::ifstream(default_icon.string(), std::ios::binary | std::ios::ate);
            const std::size_t fsize = image_stream.tellg();
            buffer.resize(fsize);
            image_stream.seekg(0, std::ios::beg);
            image_stream.read(reinterpret_cast<char *>(&buffer[0]), fsize);
        } else if (buffer.empty()) {
            LOG_WARN("Default icon not found for title {}, {}.", app.title_id, app.title);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data || width != 128 || height != 128) {
            LOG_ERROR("Invalid icon for title {}, {}.", app.title_id, app.title);
            continue;
        }
        gui.app_selector.icons[app.title_id].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }
}

void init_app_background(GuiState &gui, HostState &host, const std::string &title_id) {
    if (gui.apps_background.find(title_id) != gui.apps_background.end())
        return;

    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (title_id.find("NPXS") != std::string::npos)
        vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + title_id + "/sce_sys/pic0.png");
    else
        vfs::read_app_file(buffer, host.pref_path, title_id, "sce_sys/pic0.png");

    if (buffer.empty()) {
        LOG_WARN("Background not found for application {} [{}].", get_app_index(gui, title_id)->title, title_id);
        return;
    }

    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid background for application {} [{}].", get_app_index(gui, title_id)->title, title_id);
        return;
    }
    gui.apps_background[title_id].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
}

std::vector<App>::iterator get_app_index(GuiState &gui, const std::string &title_id) {
    auto &type_app = title_id.find("NPXS") != std::string::npos ? gui.app_selector.sys_apps : gui.app_selector.user_apps;
    const auto app_index = std::find_if(type_app.begin(), type_app.end(), [&](const App &a) {
        return a.title_id == title_id;
    });

    return app_index;
}

void get_sys_apps_title(GuiState &gui, HostState &host) {
    const std::vector<std::string> sys_apps_list = { "NPXS10008", "NPXS10015", "NPXS10026" };
    for (const auto &app : sys_apps_list) {
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::vs0, params, host.pref_path, "app/" + app + "/sce_sys/param.sfo")) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            sfo::get_data_by_key(host.app_version, sfo_handle, "APP_VER");
            if (host.app_version[0] == '0')
                host.app_version.erase(host.app_version.begin());
            sfo::get_data_by_key(host.app_category, sfo_handle, "CATEGORY");
            if (app != "NPXS10015") {
                if (!sfo::get_data_by_key(host.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(host.app_short_title, sfo_handle, "STITLE");
                if (!sfo::get_data_by_key(host.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(host.app_title, sfo_handle, "TITLE");
                std::replace(host.app_title.begin(), host.app_title.end(), '\n', ' ');
            } else
                host.app_short_title = host.app_title = "Theme & Background";
            boost::trim(host.app_title);
        } else {
            host.app_version = host.app_category = "N/A";
            if (app == "NPXS10008")
                host.app_short_title = host.app_title = "Trophy Collection";
            else if (app == "NPXS10015")
                host.app_short_title = host.app_title = "Theme & Background";
            else
                host.app_short_title = host.app_title = "Content Manager";
        }
        gui.app_selector.sys_apps.push_back({ host.app_version, host.app_category, {}, host.app_short_title, host.app_title, app });
    }
}

void get_user_apps_title(GuiState &gui, HostState &host) {
    fs::path app_path{ fs::path{ host.pref_path } / "ux0/app" };
    if (!fs::exists(app_path))
        return;

    for (const auto &app : fs::directory_iterator(app_path)) {
        if (!app.path().empty() && fs::is_directory(app.path())
            && !app.path().filename_is_dot() && !app.path().filename_is_dot_dot()) {
            vfs::FileBuffer params;
            host.app_title_id = app.path().stem().generic_string();
            if (vfs::read_app_file(params, host.pref_path, host.app_title_id, "sce_sys/param.sfo")) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                sfo::get_data_by_key(host.app_version, sfo_handle, "APP_VER");
                if (host.app_version[0] == '0')
                    host.app_version.erase(host.app_version.begin());
                sfo::get_data_by_key(host.app_category, sfo_handle, "CATEGORY");
                sfo::get_data_by_key(host.app_parental_level, sfo_handle, "PARENTAL_LEVEL");
                if (!sfo::get_data_by_key(host.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(host.app_short_title, sfo_handle, "STITLE");
                if (!sfo::get_data_by_key(host.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(host.app_title, sfo_handle, "TITLE");
                std::replace(host.app_title.begin(), host.app_title.end(), '\n', ' ');
                boost::trim(host.app_title);
            } else {
                host.app_short_title = host.app_title = host.app_title_id; // Use TitleID as Short title and Title
                host.app_version = host.app_category = host.app_parental_level = "N/A";
            }
            gui.app_selector.user_apps.push_back({ host.app_version, host.app_category, host.app_parental_level, host.app_short_title, host.app_title, host.app_title_id });
        }
    }
}

void update_notice_info(GuiState &gui, HostState &host, const std::string &type) {
    const auto pos = gui.notice_info.size();

    std::string msg;
    if (type == "content") {
        if (host.app_category == "gd") {
            msg = "The application has been added to the home screen.";

            int32_t width = 0;
            int32_t height = 0;
            vfs::FileBuffer buffer;

            vfs::read_app_file(buffer, host.pref_path, host.app_title_id, "sce_sys/pic0.png");

            if (buffer.empty()) {
                LOG_WARN("Notice icon not found for content {} [{}].", host.app_title_id, host.app_title);
                return;
            }
            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Invalid background for application {} [{}].", host.app_title_id, host.app_title);
                return;
            }

            gui.notice_info_icon[pos].init(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
        } else
            msg = "Installation complete.";

        gui.notice_info.push_back({ host.app_title_id, host.app_category, type, pos, std::time(0), host.app_title, msg });
    } else {
        const auto trophy_data = gui.trophy_unlock_display_requests.back();
        int32_t width = 0;
        int32_t height = 0;
        stbi_uc *data = stbi_load_from_memory(&trophy_data.icon_buf[0], static_cast<int>(trophy_data.icon_buf.size()), &width, &height, nullptr, STBI_rgb_alpha);

        gui.notice_info_icon[pos].init(gui.imgui_state.get(), data, width, height);

        std::string trophy_kind_s = "?";

        switch (trophy_data.trophy_kind) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM: {
            trophy_kind_s = "(Platinum)";
            break;
        }

        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD: {
            trophy_kind_s = ("Gold");
            break;
        }

        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER: {
            trophy_kind_s = "(Silver)";
            break;
        }

        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE: {
            trophy_kind_s = "(Bronze)";
            break;
        }

        default:
            break;
        }
        const auto name = trophy_kind_s + " " + trophy_data.trophy_name;
        msg = "You have earned a trophy!";
        gui.notice_info.push_back({ trophy_data.np_com_id, trophy_data.trophy_id, type, pos, std::time(0), name, msg });
    }

    ++gui.notice_info_count_new;
    gui.notice_info_new[pos] = true;

    std::sort(gui.notice_info.begin(), gui.notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
        return na.date > nb.date;
    });
}

ImTextureID load_image(GuiState &gui, const char *data, const std::uint32_t size) {
    int width;
    int height;

    stbi_uc *img_data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(data), size, &width, &height,
        nullptr, STBI_rgb_alpha);

    if (!data)
        return nullptr;

    const auto handle = ImGui_ImplSdl_CreateTexture(gui.imgui_state.get(), img_data, width, height);
    stbi_image_free(img_data);

    return handle;
}

void init(GuiState &gui, HostState &host) {
    ImGui::CreateContext();
    gui.imgui_state.reset(ImGui_ImplSdl_Init(host.renderer.get(), host.window.get(), host.base_path));

    assert(gui.imgui_state);

    init_style();
    init_font(gui, host);
    init_live_area_font(gui, host);

    bool result = ImGui_ImplSdl_CreateDeviceObjects(gui.imgui_state.get());
    assert(result);

    get_modules_list(gui, host);
    get_user_apps_title(gui, host);
    get_sys_apps_title(gui, host);
    init_apps_icon(gui, host, gui.app_selector.user_apps);

    if (!host.cfg.user_backgrounds.empty())
        init_user_backgrounds(gui, host);

    init_theme(gui, host, host.cfg.theme_content_id.empty() ? "default" : host.cfg.theme_content_id);
    init_theme_apps_icon(gui, host, host.cfg.theme_content_id.empty() ? "default" : host.cfg.theme_content_id);

    if (host.cfg.start_background == "image")
        init_user_start_background(gui, host.cfg.user_start_background);
    else
        init_theme_start_background(gui, host, host.cfg.start_background.empty() ? "default" : host.cfg.theme_content_id);

    if (!host.cfg.run_title_id && !host.cfg.vpk_path) {
        gui.live_area.information_bar = true;
        if (gui.start_background)
            gui.live_area.start_screen = true;
        else
            gui.live_area.app_selector = true;
    }

    // Initialize trophy callback
    host.np.trophy_state.trophy_unlock_callback = [&gui](NpTrophyUnlockCallbackData &callback_data) {
        const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
        gui.trophy_unlock_display_requests.push(std::move(callback_data));
    };
}

void draw_begin(GuiState &gui, HostState &host) {
    ImGui_ImplSdl_NewFrame(gui.imgui_state.get());
    host.renderer_focused = !ImGui::GetIO().WantCaptureMouse;

    ImGui::PushFont(gui.normal_font);
}

void draw_end(GuiState &gui, SDL_Window *window) {
    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplSdl_RenderDrawData(gui.imgui_state.get());
    SDL_GL_SwapWindow(window);
}

void draw_live_area(GuiState &gui, HostState &host) {
    ImGui::PushFont(gui.live_area_font);

    if (gui.live_area.app_selector)
        draw_app_selector(gui, host);

    if (gui.live_area.information_bar)
        draw_information_bar(gui, host);

    if (gui.live_area.live_area_screen)
        draw_live_area_screen(gui, host);
    if (gui.live_area.manual)
        draw_manual(gui, host);

    if (gui.live_area.theme_background)
        draw_themes_selection(gui, host);
    if (gui.live_area.start_screen)
        draw_start_screen(gui, host);

    if (gui.live_area.trophy_collection)
        draw_trophy_collection(gui, host);

    if (gui.live_area.content_manager)
        draw_content_manager(gui, host);

    ImGui::PopFont();
}

void draw_ui(GuiState &gui, HostState &host) {
    draw_main_menu_bar(gui, host);

    ImGui::PushFont(gui.monospaced_font);

    if (gui.file_menu.firmware_install_dialog)
        draw_firmware_install_dialog(gui, host);
    if (gui.file_menu.pkg_install_dialog)
        draw_pkg_install_dialog(gui, host);
    if (gui.file_menu.archive_install_dialog)
        draw_archive_install_dialog(gui, host);
    if (gui.debug_menu.threads_dialog)
        draw_threads_dialog(gui, host);
    if (gui.debug_menu.thread_details_dialog)
        draw_thread_details_dialog(gui, host);
    if (gui.debug_menu.semaphores_dialog)
        draw_semaphores_dialog(gui, host);
    if (gui.debug_menu.mutexes_dialog)
        draw_mutexes_dialog(gui, host);
    if (gui.debug_menu.lwmutexes_dialog)
        draw_lw_mutexes_dialog(gui, host);
    if (gui.debug_menu.condvars_dialog)
        draw_condvars_dialog(gui, host);
    if (gui.debug_menu.lwcondvars_dialog)
        draw_lw_condvars_dialog(gui, host);
    if (gui.debug_menu.eventflags_dialog)
        draw_event_flags_dialog(gui, host);
    if (gui.debug_menu.allocations_dialog)
        draw_allocations_dialog(gui, host);
    if (gui.debug_menu.disassembly_dialog)
        draw_disassembly_dialog(gui, host);

    if (gui.configuration_menu.profiles_manager_dialog)
        draw_profiles_manager_dialog(gui, host);
    if (gui.configuration_menu.settings_dialog)
        draw_settings_dialog(gui, host);

    if (gui.controls_menu.controls_dialog)
        draw_controls_dialog(gui, host);

    if (gui.help_menu.about_dialog)
        draw_about_dialog(gui);

    ImGui::PopFont();
}

} // namespace gui

namespace ImGui {

bool vector_getter(void *vec, int idx, const char **out_text) {
    auto &vector = *static_cast<std::vector<std::string> *>(vec);
    if (idx < 0 || idx >= static_cast<int>(vector.size())) {
        return false;
    }
    *out_text = vector.at(idx).c_str();
    return true;
}

} // namespace ImGui
