// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <gui/state.h>

#include <boost/algorithm/string/trim.hpp>
#include <glutil/gl.h>
#include <host/functions.h>
#include <io/VitaIoDevice.h>
#include <io/vfs.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL_video.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <string>
#include <vector>

namespace gui {

static void init_style(HostState &host) {
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

    style->ScaleAllSizes(host.dpi_scale);

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
    style->Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

static void init_font(GuiState &gui, HostState &host) {
    ImGuiIO &io = ImGui::GetIO();

#ifdef _WIN32
    if (host.dpi_scale > 1) {
        // Set monospaced font path -- ImGui's default is a bitmap font that does not scale well, so load Consolas instead
        const auto monospaced_font_path = "C:\\Windows\\Fonts\\consola.ttf";
        gui.monospaced_font = io.Fonts->AddFontFromFileTTF(monospaced_font_path, 13.f * host.dpi_scale, NULL, io.Fonts->GetGlyphRangesJapanese());
    } else {
        gui.monospaced_font = io.Fonts->AddFontDefault();
    }
#else
    gui.monospaced_font = io.Fonts->AddFontDefault();
#endif

    // Set Large Font
    static const ImWchar large_font_chars[] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L':', L'A', L'M', L'P' };

    // Set Fw font paths
    const auto fw_font_path{ fs::path(host.pref_path) / "sa0/data/font/pvf" };
    const auto latin_fw_font_path{ fw_font_path / "ltn0.pvf" };

    // clang-format off
    static const ImWchar latin_range[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0x2000, 0x206F, // General Punctuation
        0x20A0, 0x20CF, // Currency Symbols
        0x2100, 0x214F, // Letter type symbols
        0,
    };

    static const ImWchar extra_range[] = {
        0x2200, 0x22FF, // Math operators 
        0x2150, 0x218F, // Numeral forms
        0x2600, 0x26FF, // Miscellaneous symbols
        0x4E00, 0x9FFF, // Unified ideograms CJC
        0,
    };

    static const ImWchar korean_range[] = {
        0x3131, 0x3163, // Korean alphabets
        0xAC00, 0xD79D, // Korean characters
        0,
    };

    static const ImWchar chinese_range[] = {
        0x2000, 0x206F, // General Punctuation
        0x4e00, 0x9FAF, // CJK Ideograms
        0,
    };
    // clang-format on

    ImFontConfig font_config{};

    // Check existence of fw font file
    if (fs::exists(latin_fw_font_path)) {
        // Add fw font to imgui
        gui.fw_font = true;
        gui.vita_font = io.Fonts->AddFontFromFileTTF(latin_fw_font_path.string().c_str(), 19.2f * host.dpi_scale, &font_config, latin_range);
        font_config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF((fw_font_path / "jpn0.pvf").string().c_str(), 19.2f * host.dpi_scale, &font_config, io.Fonts->GetGlyphRangesJapanese());
        io.Fonts->AddFontFromFileTTF((fw_font_path / "jpn0.pvf").string().c_str(), 19.2f * host.dpi_scale, &font_config, extra_range);

        const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
        if (host.cfg.asia_font_support || (sys_lang == SCE_SYSTEM_PARAM_LANG_KOREAN))
            io.Fonts->AddFontFromFileTTF((fw_font_path / "kr0.pvf").string().c_str(), 19.2f * host.dpi_scale, &font_config, korean_range);
        if (host.cfg.asia_font_support || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_T) || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_S))
            io.Fonts->AddFontFromFileTTF((fw_font_path / "cn0.pvf").string().c_str(), 19.2f * host.dpi_scale, &font_config, chinese_range);

        io.Fonts->Build();
        font_config.MergeMode = false;
        gui.large_font = io.Fonts->AddFontFromFileTTF(latin_fw_font_path.string().c_str(), 116.f * host.dpi_scale, &font_config, large_font_chars);
    } else {
        LOG_WARN("Could not find firmware font file at \"{}\", install firmware fonts package to fix this.", latin_fw_font_path.string());
        // Set up default font path
        const auto default_font_path{ fs::path(host.base_path) / "data/fonts/mplus-1mn-bold.ttf" };
        // Check existence of default font file
        if (fs::exists(default_font_path)) {
            gui.vita_font = io.Fonts->AddFontFromFileTTF(default_font_path.string().c_str(), 22.f * host.dpi_scale, &font_config, latin_range);
            font_config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(default_font_path.string().c_str(), 22.f * host.dpi_scale, &font_config, io.Fonts->GetGlyphRangesJapanese());

            io.Fonts->Build();
            font_config.MergeMode = false;
            gui.large_font = io.Fonts->AddFontFromFileTTF(default_font_path.string().c_str(), 134.f * host.dpi_scale, &font_config, large_font_chars);

            LOG_INFO("Using default Vita3K font.");
        } else
            LOG_WARN("Could not find default Vita3K font, using default ImGui font.", default_font_path.string());
    }
    // DPI scaling
    io.DisplayFramebufferScale = { host.dpi_scale, host.dpi_scale };
}

vfs::FileBuffer init_default_icon(GuiState &gui, HostState &host) {
    vfs::FileBuffer buffer;

    const auto default_fw_icon{ fs::path(host.pref_path) / "vs0/data/internal/livearea/default/sce_sys/icon0.png" };
    const auto default_icon{ fs::path(host.base_path) / "data/image/icon.png" };

    if (fs::exists(default_fw_icon) || fs::exists(default_icon)) {
        auto icon_path = fs::exists(default_fw_icon) ? default_fw_icon.string() : default_icon.string();
        std::ifstream image_stream(icon_path, std::ios::binary | std::ios::ate);
        const std::size_t fsize = image_stream.tellg();
        buffer.resize(fsize);
        image_stream.seekg(0, std::ios::beg);
        image_stream.read(reinterpret_cast<char *>(buffer.data()), fsize);
    }

    return buffer;
}

static IconData load_app_icon(GuiState &gui, HostState &host, const std::string &app_path) {
    IconData image;
    vfs::FileBuffer buffer;

    const auto APP_INDEX = get_app_index(gui, app_path);

    if (!vfs::read_app_file(buffer, host.pref_path, app_path, "sce_sys/icon0.png")) {
        buffer = init_default_icon(gui, host);
        if (buffer.empty()) {
            LOG_WARN("Default icon not found for title {}, [{}] in path {}.",
                APP_INDEX->title_id, APP_INDEX->title, app_path);
            return {};
        } else
            LOG_INFO("Default icon found for App {}, [{}] in path {}.", APP_INDEX->title_id, APP_INDEX->title, app_path);
    }
    image.data.reset(stbi_load_from_memory(
        buffer.data(), static_cast<int>(buffer.size()),
        &image.width, &image.height, nullptr, STBI_rgb_alpha));
    if (!image.data || image.width != 128 || image.height != 128) {
        LOG_ERROR("Invalid icon for title {}, [{}] in path {}.",
            APP_INDEX->title_id, APP_INDEX->title, app_path);
        return {};
    }

    return std::move(image);
}

void init_app_icon(GuiState &gui, HostState &host, const std::string &app_path) {
    IconData data = load_app_icon(gui, host, app_path);

    gui.app_selector.user_apps_icon[app_path].init(gui.imgui_state.get(), data.data.get(), data.width, data.height);
}

IconData::IconData()
    : data(nullptr, stbi_image_free) {}

void IconAsyncLoader::commit(GuiState &gui) {
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto &pair : icon_data)
        gui.app_selector.user_apps_icon[pair.first].init(gui.imgui_state.get(), pair.second.data.get(), pair.second.width, pair.second.height);

    icon_data.clear();
}

IconAsyncLoader::IconAsyncLoader(GuiState &gui, HostState &host, const std::vector<gui::App> &app_list) {
    // I don't feel comfortable passing app_list down to be iterated by thread.
    // Methods like delete_app might mutate it, so I'd like to copy what I need now.
    auto paths = [&app_list]() {
        std::vector<std::string> copy(app_list.size());
        std::transform(app_list.begin(), app_list.end(), copy.begin(), [](const auto &a) { return a.path; });

        return copy;
    };

    quit = false;
    thread = std::thread([&, paths = paths()]() {
        for (const auto &path : paths) {
            if (quit)
                return;

            // load the actual texture
            IconData data = load_app_icon(gui, host, path);

            // Duplicate code here from init_app_icon
            {
                std::lock_guard<std::mutex> lock(mutex);
                icon_data[path] = std::move(data);
            }
        }
    });
}

IconAsyncLoader::~IconAsyncLoader() {
    quit = true;
    thread.join();
}

void init_apps_icon(GuiState &gui, HostState &host, const std::vector<gui::App> &app_list) {
    gui.app_selector.icon_async_loader.emplace(gui, host, app_list);
}

void init_app_background(GuiState &gui, HostState &host, const std::string &app_path) {
    if (gui.apps_background.find(app_path) != gui.apps_background.end())
        return;

    const auto APP_INDEX = get_app_index(gui, app_path);
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    const auto is_sys = app_path.find("NPXS") != std::string::npos;
    if (is_sys)
        vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + app_path + "/sce_sys/pic0.png");
    else
        vfs::read_app_file(buffer, host.pref_path, app_path, "sce_sys/pic0.png");

    if (buffer.empty()) {
        LOG_WARN("Background not found for application {} [{}].", APP_INDEX->title, app_path);
        return;
    }

    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid background for application {} [{}].", APP_INDEX->title, app_path);
        return;
    }
    gui.apps_background[app_path].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
}

static bool get_user_apps(GuiState &gui, HostState &host) {
    const auto apps_cache_path{ fs::path(host.pref_path) / "ux0/temp/apps.dat" };
    fs::ifstream apps_cache(apps_cache_path, std::ios::in | std::ios::binary);
    if (apps_cache.is_open()) {
        gui.app_selector.user_apps.clear();
        // Read size of apps list
        size_t size;
        apps_cache.read((char *)&size, sizeof(size));

        // Check version of cache
        size_t version_size;
        apps_cache.read((char *)&version_size, sizeof(version_size));
        std::vector<char> buffer(version_size); // dont trust std::string to hold buffer enough
        apps_cache.read(buffer.data(), version_size);
        const std::string versionInFile(buffer.begin(), buffer.end());
        if (versionInFile != "1.02") {
            LOG_WARN("Current version of cache: {}, is outdated, recreate it.", versionInFile);
            return false;
        }

        // Read App info value
        for (size_t a = 0; a < size; a++) {
            auto read = [&apps_cache]() {
                size_t size;

                apps_cache.read((char *)&size, sizeof(size));

                std::vector<char> buffer(size); // dont trust std::string to hold buffer enough
                apps_cache.read(buffer.data(), size);

                return std::string(buffer.begin(), buffer.end());
            };

            App app;

            app.app_ver = read();
            app.category = read();
            app.content_id = read();
            app.addcont = read();
            app.savedata = read();
            app.parental_level = read();
            app.stitle = read();
            app.title = read();
            app.title_id = read();
            app.path = read();

            gui.app_selector.user_apps.push_back(app);
        }

        init_apps_icon(gui, host, gui.app_selector.user_apps);
    }

    return !gui.app_selector.user_apps.empty();
}

void save_apps_cache(GuiState &gui, HostState &host) {
    const auto temp_path{ fs::path(host.pref_path) / "ux0/temp" };
    if (!fs::exists(temp_path))
        fs::create_directory(temp_path);

    fs::ofstream apps_cache(temp_path / "apps.dat", std::ios::out | std::ios::binary);
    if (apps_cache.is_open()) {
        // Write Size of apps list
        const auto size = gui.app_selector.user_apps.size();
        apps_cache.write((char *)&size, sizeof(size));

        // Write version of cache
        const std::string version = "1.02";
        const auto size_ver = version.length();
        apps_cache.write((char *)&size_ver, sizeof(size_ver));
        apps_cache.write(version.c_str(), size_ver);

        // Write Apps list
        for (const App &app : gui.app_selector.user_apps) {
            auto write = [&apps_cache](const std::string &i) {
                const auto size = i.length();

                apps_cache.write((char *)&size, sizeof(size));
                apps_cache.write(i.c_str(), size);
            };

            write(app.app_ver);
            write(app.category);
            write(app.content_id);
            write(app.addcont);
            write(app.savedata);
            write(app.parental_level);
            write(app.stitle);
            write(app.title);
            write(app.title_id);
            write(app.path);
        }
        apps_cache.close();
    }
}

void init_home(GuiState &gui, HostState &host) {
    const auto is_cmd = host.cfg.run_app_path || host.cfg.content_path;
    if (host.cfg.load_app_list || !is_cmd) {
        if (!get_user_apps(gui, host))
            init_user_apps(gui, host);
    }

    get_time_apps(gui, host);

    if (!gui.users.empty() && (gui.users.find(host.cfg.user_id) != gui.users.end()) && (is_cmd || host.cfg.auto_user_login)) {
        init_user(gui, host, host.cfg.user_id);
        if (!is_cmd && host.cfg.auto_user_login) {
            gui.live_area.information_bar = true;
            open_user(gui, host);
        }
    } else {
        gui.live_area.information_bar = true;
        gui.live_area.user_management = true;
    }
}

void init_user_app(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto APP_INDEX = get_app_index(gui, app_path);
    if (APP_INDEX != gui.app_selector.user_apps.end()) {
        gui.app_selector.user_apps.erase(APP_INDEX);
        if (gui.app_selector.user_apps_icon.find(app_path) != gui.app_selector.user_apps_icon.end())
            gui.app_selector.user_apps_icon.erase(app_path);
    }

    get_app_param(gui, host, app_path);
    init_app_icon(gui, host, app_path);

    gui.app_selector.is_app_list_sorted = false;
}

std::map<std::string, ImGui_Texture>::const_iterator get_app_icon(GuiState &gui, const std::string &app_path) {
    const auto &app_type = app_path.find("NPXS") != std::string::npos ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;
    const auto app_icon = std::find_if(app_type.begin(), app_type.end(), [&](const auto &i) {
        return i.first == app_path;
    });

    return app_icon;
}

std::vector<App>::iterator get_app_index(GuiState &gui, const std::string &app_path) {
    auto &app_type = app_path.find("NPXS") != std::string::npos ? gui.app_selector.sys_apps : gui.app_selector.user_apps;
    const auto app_index = std::find_if(app_type.begin(), app_type.end(), [&](const App &a) {
        return a.path == app_path;
    });

    return app_index;
}

void get_app_param(GuiState &gui, HostState &host, const std::string &app_path) {
    host.app_path = app_path;
    vfs::FileBuffer param;
    if (vfs::read_app_file(param, host.pref_path, app_path, "sce_sys/param.sfo")) {
        get_param_info(host, param);
    } else {
        host.app_addcont = host.app_savedata = host.app_short_title = host.app_title = host.app_title_id = host.app_path; // Use app path as TitleID, addcont, Savedata, Short title and Title
        host.app_version = host.app_category = host.app_parental_level = "N/A";
    }
    gui.app_selector.user_apps.push_back({ host.app_version, host.app_category, host.app_content_id, host.app_addcont, host.app_savedata, host.app_parental_level, host.app_short_title, host.app_title, host.app_title_id, host.app_path });
}

void get_param_info(HostState &host, const vfs::FileBuffer &param) {
    SfoFile sfo_handle;
    sfo::load(sfo_handle, param);
    sfo::get_data_by_key(host.app_version, sfo_handle, "APP_VER");
    if (host.app_version[0] == '0')
        host.app_version.erase(host.app_version.begin());
    sfo::get_data_by_key(host.app_category, sfo_handle, "CATEGORY");
    sfo::get_data_by_key(host.app_content_id, sfo_handle, "CONTENT_ID");
    if (!sfo::get_data_by_key(host.app_addcont, sfo_handle, "INSTALL_DIR_ADDCONT"))
        sfo::get_data_by_key(host.app_addcont, sfo_handle, "TITLE_ID");
    if (!sfo::get_data_by_key(host.app_savedata, sfo_handle, "INSTALL_DIR_SAVEDATA"))
        sfo::get_data_by_key(host.app_savedata, sfo_handle, "TITLE_ID");
    sfo::get_data_by_key(host.app_parental_level, sfo_handle, "PARENTAL_LEVEL");
    if (!sfo::get_data_by_key(host.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", host.cfg.sys_lang)))
        sfo::get_data_by_key(host.app_short_title, sfo_handle, "STITLE");
    if (!sfo::get_data_by_key(host.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
        sfo::get_data_by_key(host.app_title, sfo_handle, "TITLE");
    std::replace(host.app_title.begin(), host.app_title.end(), '\n', ' ');
    boost::trim(host.app_title);
    sfo::get_data_by_key(host.app_title_id, sfo_handle, "TITLE_ID");
}

void get_user_apps_title(GuiState &gui, HostState &host) {
    fs::path app_path{ fs::path{ host.pref_path } / "ux0/app" };
    if (!fs::exists(app_path))
        return;

    gui.app_selector.user_apps.clear();
    for (const auto &app : fs::directory_iterator(app_path)) {
        if (!app.path().empty() && fs::is_directory(app.path())
            && !app.path().filename_is_dot() && !app.path().filename_is_dot_dot()) {
            const auto app_path = app.path().stem().generic_string();
            get_app_param(gui, host, app_path);
        }
    }
}

void get_sys_apps_title(GuiState &gui, HostState &host) {
    gui.app_selector.sys_apps.clear();
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
            sfo::get_data_by_key(host.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", host.cfg.sys_lang));
            sfo::get_data_by_key(host.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang));
            boost::trim(host.app_title);
            sfo::get_data_by_key(host.app_title_id, sfo_handle, "TITLE_ID");
        } else {
            host.app_version = "1.00";
            host.app_category = "gda";
            host.app_title_id = app;
            if (app == "NPXS10008") {
                host.app_short_title = "Trophies";
                host.app_title = "Trophy Collection";
            } else if (app == "NPXS10015")
                host.app_short_title = host.app_title = "Settings";
            else
                host.app_short_title = host.app_title = "Content Manager";
        }
        gui.app_selector.sys_apps.push_back({ host.app_version, host.app_category, {}, {}, {}, {}, host.app_short_title, host.app_title, host.app_title_id, app });
    }

    std::sort(gui.app_selector.sys_apps.begin(), gui.app_selector.sys_apps.end(), [](const App &lhs, const App &rhs) {
        return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
    });
}

static const char *const ymonth[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *const small_ymonth[] = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"
};

static const char *const wday[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

std::map<DateTime, std::string> get_date_time(GuiState &gui, HostState &host, const tm &date_time) {
    std::map<DateTime, std::string> date_time_str;
    if (!host.io.user_id.empty()) {
        const auto day_str = !gui.lang.common.wday.empty() ? gui.lang.common.wday[date_time.tm_wday] : wday[date_time.tm_wday];
        const auto month_str = !gui.lang.common.ymonth.empty() ? gui.lang.common.ymonth[date_time.tm_mon] : ymonth[date_time.tm_mon];
        const auto year = date_time.tm_year + 1900;
        const auto month = date_time.tm_mon + 1;
        switch (gui.users[host.io.user_id].date_format) {
        case DateFormat::YYYY_MM_DD:
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", month_str, date_time.tm_mday, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", year, month, date_time.tm_mday);
            break;
        case DateFormat::DD_MM_YYYY: {
            const auto small_month_str = !gui.lang.common.small_ymonth.empty() ? gui.lang.common.small_ymonth[date_time.tm_mon] : small_ymonth[date_time.tm_mon];
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", date_time.tm_mday, small_month_str, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", date_time.tm_mday, month, year);
            break;
        }
        case DateFormat::MM_DD_YYYY:
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", month_str, date_time.tm_mday, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", month, date_time.tm_mday, year);
            break;
        default: break;
        }
    }
    const auto is_afternoon = date_time.tm_hour > 12;
    const auto clock_12h = is_afternoon && (host.io.user_id.empty() || gui.users[host.io.user_id].clock_12_hour);
    date_time_str[DateTime::HOUR] = std::to_string(clock_12h ? (date_time.tm_hour - 12) : date_time.tm_hour);
    date_time_str[DateTime::CLOCK] = fmt::format("{}:{:0>2d}", date_time_str[DateTime::HOUR], date_time.tm_min);
    date_time_str[DateTime::DAY_MOMENT] = is_afternoon ? "PM" : "AM";

    return date_time_str;
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

    init_style(host);
    init_font(gui, host);
    init_lang(gui, host);
    get_notice_list(host);
    get_users_list(gui, host);

    bool result = ImGui_ImplSdl_CreateDeviceObjects(gui.imgui_state.get());
    assert(result);

    if (host.cfg.show_welcome)
        gui.help_menu.welcome_dialog = true;

    get_sys_apps_title(gui, host);

    init_home(gui, host);

    // Initialize trophy callback
    host.np.trophy_state.trophy_unlock_callback = [&gui](NpTrophyUnlockCallbackData &callback_data) {
        const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
        gui.trophy_unlock_display_requests.insert(gui.trophy_unlock_display_requests.begin(), callback_data);
    };
}

void draw_begin(GuiState &gui, HostState &host) {
    ImGui_ImplSdl_NewFrame(gui.imgui_state.get());
    host.renderer_focused = !ImGui::GetIO().WantCaptureMouse;

    // async loading, renderer texture creation needs to be synchronous
    // cant bind opengl context outside main thread on macos now
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->commit(gui);
}

void draw_end(GuiState &gui, SDL_Window *window) {
    ImGui::Render();
    ImGui_ImplSdl_RenderDrawData(gui.imgui_state.get());
    SDL_GL_SwapWindow(window);
}

void draw_live_area(GuiState &gui, HostState &host) {
    ImGui::PushFont(gui.vita_font);

    if (gui.live_area.app_close)
        draw_app_close(gui, host);

    if (gui.live_area.app_selector)
        draw_app_selector(gui, host);

    if ((host.cfg.show_info_bar || !host.display.imgui_render || !gui.live_area.app_selector) && gui.live_area.information_bar)
        draw_information_bar(gui, host);

    if (gui.live_area.live_area_screen)
        draw_live_area_screen(gui, host);
    if (gui.live_area.manual)
        draw_manual(gui, host);

    if (gui.file_menu.archive_install_dialog)
        draw_archive_install_dialog(gui, host);
    if (gui.file_menu.pkg_install_dialog)
        draw_pkg_install_dialog(gui, host);

    if (gui.live_area.user_management)
        draw_user_management(gui, host);

    if (!gui.trophy_unlock_display_requests.empty())
        gui::draw_trophies_unlocked(gui, host);

    if (host.ime.state && !gui.live_area.app_selector && !gui.live_area.live_area_screen && get_sys_apps_state(gui))
        draw_ime(host.ime, host);

    // System App
    if (gui.live_area.content_manager)
        draw_content_manager(gui, host);

    if (gui.live_area.settings)
        draw_settings(gui, host);

    if (gui.live_area.trophy_collection)
        draw_trophy_collection(gui, host);

    ImGui::PopFont();

    if (gui.live_area.start_screen)
        draw_start_screen(gui, host);
}

void draw_ui(GuiState &gui, HostState &host) {
    ImGui::PushFont(gui.vita_font);
    draw_main_menu_bar(gui, host);
    if (gui.controls_menu.controllers_dialog)
        draw_controllers_dialog(gui, host);

    ImGui::PopFont();

    ImGui::PushFont(gui.monospaced_font);

    if (gui.file_menu.firmware_install_dialog)
        draw_firmware_install_dialog(gui, host);
    if (gui.file_menu.license_install_dialog)
        draw_license_install_dialog(gui, host);
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

    if (gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog)
        draw_settings_dialog(gui, host);

    if (gui.controls_menu.controls_dialog)
        draw_controls_dialog(gui, host);

    if (gui.help_menu.about_dialog)
        draw_about_dialog(gui, host);
    if (gui.help_menu.welcome_dialog)
        draw_welcome_dialog(gui, host);

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
