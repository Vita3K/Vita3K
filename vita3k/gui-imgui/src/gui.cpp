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

#include <gui-imgui/functions.h>

#include <app/functions.h>
#include <gui-imgui/imgui_impl_sdl.h>
#include <gui-imgui/popups.h>
#include <gui-imgui/state.h>
#include <renderer/functions.h>
#include <renderer/state.h>

#include <boost/algorithm/string/trim.hpp>
#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <io/vfs.h>
#include <kernel/state.h>
#include <packages/sfo.h>
#include <regmgr/functions.h>
#include <touch/functions.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <v3kn/state.h>
#include <vfs/appmeta.h>

#include <imgui_internal.h>

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <codecvt>
#include <locale>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <cstring>

namespace gui {

static void init_style(const GuiState &gui) {
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

    style->ScaleAllSizes(sdl::get_viewport().manual_dpi_scale);

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
    style->Colors[ImGuiCol_Separator] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_SeparatorActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.18f, 0.20f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    style->Colors[ImGuiCol_Tab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_TabHovered] = ImVec4(0.32f, 0.30f, 0.23f, 1.00f);
    style->Colors[ImGuiCol_TabSelected] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(1.f, 0.49f, 0.f, 1.f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 1.00f, 0.00f, 0.50f);
    style->Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

static void init_font(GuiState &gui, EmuEnvState &emuenv) {
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = sdl::get_viewport().manual_dpi_scale;
    gui.fw_font = false;

    // Set Large Font
    constexpr ImWchar large_font_chars[] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L':', L'A', L'M', L'P', 0 };

    // clang-format off
    constexpr ImWchar latin_range[] = {
        0x0020, 0x017F, // Basic Latin + Latin Supplement
        0x0370, 0x03FF, // Greek and Coptic
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x20A0, 0x20CF, // Currency Symbols
        0x2100, 0x214F, // Letter type symbols
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0,
    };

    constexpr ImWchar extra_range[] = {
        0x0100, 0x017F, // Latin Extended A
        0x2000, 0x206F, // General Punctuation
        0x2150, 0x218F, // Numeral forms
        0x2190, 0x21FF, // Arrows
        0x2200, 0x22FF, // Math operators
        0x2460, 0x24FF, // Enclosed Alphanumerics
        0x25A0, 0x26FF, // Miscellaneous symbols
        0x3130, 0x316F, // Unified alphabets CJK
        0xAC00, 0xD79F, // Unified characters CJK
        0x4E00, 0x9FFF, // Unified ideograms CJK
        0,
    };

    constexpr ImWchar korean_range[] = {
        0x3131, 0x3163, // Korean alphabets
        0xAC00, 0xD79D, // Korean characters
        0,
    };

    constexpr ImWchar chinese_range[] = {
        0x2000, 0x206F, // General Punctuation
        0x4E00, 0x9FAF, // CJK Ideograms
        0,
    };
    // clang-format on

    // Merge Japanese and Extra ranges
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddRanges(extra_range);
    ImVector<ImWchar> japanese_and_extra_ranges;
    builder.BuildRanges(&japanese_and_extra_ranges);

    // Set max texture size
    int max_texture_size = emuenv.renderer->get_max_2d_texture_width();
    io.Fonts->TexDesiredWidth = max_texture_size;

    for (int font_scale_count = std::size(FontScaleCandidates); font_scale_count > 0; font_scale_count--) {
        for (int i = 0; i < font_scale_count; i++) {
            float scale = FontScaleCandidates[i];

            ImFontConfig mono_font_config{};
            mono_font_config.SizePixels = 13.f;
            mono_font_config.OversampleH = 2;
            mono_font_config.OversampleV = 2;
            mono_font_config.RasterizerDensity = scale;

#ifdef _WIN32
            constexpr auto monospaced_font_path = "C:\\Windows\\Fonts\\consola.ttf";
            gui.monospaced_font[i] = io.Fonts->AddFontFromFileTTF(monospaced_font_path, mono_font_config.SizePixels, &mono_font_config, io.Fonts->GetGlyphRangesJapanese());
#else
            gui.monospaced_font[i] = io.Fonts->AddFontDefault(&mono_font_config);
#endif

            // Set Fw font paths
            const auto fw_font_path{ emuenv.vita_fs_path / "sa0/data/font/pvf" };
            const auto latin_fw_font_path{ fw_font_path / "ltn0.pvf" };

            ImFontConfig font_config{};
            ImFontConfig large_font_config{};

            // Check existence of fw font file
            if (fs::exists(latin_fw_font_path)) {
                // Add fw font to imgui

                gui.fw_font = true;
                font_config.SizePixels = 19.2f;
                font_config.OversampleH = 2;
                font_config.OversampleV = 2;
                font_config.RasterizerDensity = scale;

                gui.vita_font[i] = io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(latin_fw_font_path).c_str(), font_config.SizePixels, &font_config, latin_range);
                font_config.MergeMode = true;

                const auto sys_lang = static_cast<SceSystemParamLang>(emuenv.cfg.sys_lang);
                const bool is_chinese = (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_S) || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_T);

                // When the system language is Chinese, the Chinese fonts should be loaded before the Japanese fonts
                // So that the CJK characters can be displayed in Chinese glyphs
                if (is_chinese)
                    io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(fw_font_path / "cn0.pvf").c_str(), font_config.SizePixels, &font_config, chinese_range);

                io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(fw_font_path / "jpn0.pvf").c_str(), font_config.SizePixels, &font_config, japanese_and_extra_ranges.Data);

                if (emuenv.cfg.asia_font_support || (sys_lang == SCE_SYSTEM_PARAM_LANG_KOREAN))
                    io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(fw_font_path / "kr0.pvf").c_str(), font_config.SizePixels, &font_config, korean_range);
                if (emuenv.cfg.asia_font_support && !is_chinese)
                    io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(fw_font_path / "cn0.pvf").c_str(), font_config.SizePixels, &font_config, chinese_range);
                font_config.MergeMode = false;

                large_font_config.SizePixels = 116.f;
                large_font_config.OversampleH = 2;
                large_font_config.OversampleV = 2;
                large_font_config.RasterizerDensity = scale;
                gui.large_font[i] = io.Fonts->AddFontFromFileTTF(fs_utils::path_to_utf8(latin_fw_font_path).c_str(), large_font_config.SizePixels, &large_font_config, large_font_chars);
            } else {
                LOG_WARN("Could not find firmware font file at {}, install firmware fonts package to fix this.", latin_fw_font_path);
                font_config.SizePixels = 22.f;
                font_config.OversampleH = 2;
                font_config.OversampleV = 2;
                font_config.RasterizerDensity = scale;

                // Set up default font path
                fs::path default_font_path = emuenv.static_assets_path / "data/fonts";

                // Check existence of default font file
                std::vector<uint8_t> font_mplus{};
                if (fs_utils::read_data(default_font_path / "mplus-1mn-bold.ttf", font_mplus)) {
                    // when calling AddFontFromMemoryTTF, we tranfer ownership to imgui and it is up to it to free the data
                    void *font_data = IM_ALLOC(font_mplus.size());
                    memcpy(font_data, font_mplus.data(), font_mplus.size());
                    gui.vita_font[i] = io.Fonts->AddFontFromMemoryTTF(font_data, font_mplus.size(), font_config.SizePixels, &font_config, latin_range);
                    font_config.MergeMode = true;

                    font_data = IM_ALLOC(font_mplus.size());
                    memcpy(font_data, font_mplus.data(), font_mplus.size());
                    io.Fonts->AddFontFromMemoryTTF(font_data, font_mplus.size(), font_config.SizePixels, &font_config, japanese_and_extra_ranges.Data);

                    const auto sys_lang = static_cast<SceSystemParamLang>(emuenv.cfg.sys_lang);
                    if (!emuenv.cfg.initial_setup || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_S) || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_T) || (sys_lang == SCE_SYSTEM_PARAM_LANG_KOREAN)) {
                        std::vector<uint8_t> font_source{};
                        std::vector<uint8_t> font_neodgm{};
                        if (fs_utils::read_data(default_font_path / "SourceHanSansSC-Bold-Min.ttf", font_source)) {
                            font_data = IM_ALLOC(font_source.size());
                            memcpy(font_data, font_source.data(), font_source.size());
                            io.Fonts->AddFontFromMemoryTTF(font_data, font_source.size(), font_config.SizePixels, &font_config, japanese_and_extra_ranges.Data);
                        }
                        if (fs_utils::read_data(default_font_path / "neodgm.ttf", font_neodgm)) {
                            font_data = IM_ALLOC(font_neodgm.size());
                            memcpy(font_data, font_neodgm.data(), font_neodgm.size());
                            io.Fonts->AddFontFromMemoryTTF(font_data, font_neodgm.size(), font_config.SizePixels, &font_config, japanese_and_extra_ranges.Data);
                        }
                    }
                    font_config.MergeMode = false;

                    large_font_config.SizePixels = 134.f;
                    large_font_config.OversampleH = 2;
                    large_font_config.OversampleV = 2;
                    large_font_config.RasterizerDensity = scale;
                    font_data = IM_ALLOC(font_mplus.size());
                    memcpy(font_data, font_mplus.data(), font_mplus.size());
                    gui.large_font[i] = io.Fonts->AddFontFromMemoryTTF(font_data, font_mplus.size(), large_font_config.SizePixels, &large_font_config, large_font_chars);

                    LOG_INFO("Using default Vita3K font.");
                } else
                    LOG_WARN("Could not find default Vita3K font at {}, using default ImGui font.", default_font_path);
            }
        }

        // Build font atlas loaded and upload to GPU
        io.Fonts->Build();
        LOG_INFO("Maximum font scale set to x{}, Font atlas size: {}x{}", FontScaleCandidates[font_scale_count - 1], io.Fonts->TexWidth, io.Fonts->TexHeight);
        if (io.Fonts->TexWidth > max_texture_size || io.Fonts->TexHeight > max_texture_size) {
            LOG_WARN("Font atlas size exceeds maximum texture size, retrying with smaller font size.\n");
            io.Fonts->Clear();
        } else {
            gui.max_font_level = font_scale_count - 1;
            return;
        }
    }
}

vfs::FileBuffer init_default_icon(GuiState &gui, EmuEnvState &emuenv) {
    vfs::FileBuffer buffer;

    const auto default_fw_icon{ emuenv.vita_fs_path / "vs0/data/internal/livearea/default/sce_sys/icon0.png" };

    const fs::path default_icon{ emuenv.static_assets_path / "data/image/icon.png" };

    const fs::path icon_path = fs::exists(default_fw_icon) ? default_fw_icon : default_icon;
    fs_utils::read_data(icon_path, buffer);

    return buffer;
}

static IconData load_app_icon(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    IconData image;
    vfs::FileBuffer buffer;

    const auto APP_INDEX = get_app_index(gui, emuenv, app_path);
    if (!APP_INDEX)
        return {};

    if (!vfs::read_app_icon(buffer, emuenv.vita_fs_path, app_path, APP_INDEX->title_id)) {
        buffer = init_default_icon(gui, emuenv);
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

    return image;
}

void init_app_icon(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    IconData data = load_app_icon(gui, emuenv, app_path);
    if (data.data) {
        gui.app_selector.user_apps_icon[app_path] = ImGui_Texture(gui.imgui_state.get(), data.data.get(), data.width, data.height);
    }
}

static void init_vita_icons(GuiState &gui, EmuEnvState &emuenv) {
    constexpr std::array<const char *, 26> icon_names = { "cloud", "comments", "game_start", "gate", "indicator_new", "indicator_no", "like", "likes", "manual", "playing_game", "refresh", "refresh_bg", "search", "small_like", "start", "trophy_all", "trophy_bronze", "trophy_gold", "trophy_hidden", "trophy_level", "trophy_locked", "trophy_platinum", "trophy_silver", "unlike", "update", "www" };
    const auto icon_directory = emuenv.static_assets_path / "data/image/vita";
    for (const auto *name : icon_names) {
        if (gui.vita_icons.contains(name))
            continue;

        const auto icon_path = icon_directory / (std::string(name) + ".png");
        vfs::FileBuffer buffer;
        if (!fs_utils::read_data(icon_path, buffer)) {
            LOG_WARN("Vita icon not found at {}.", icon_path);
            continue;
        }

        int32_t width = 0;
        int32_t height = 0;
        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid Vita icon: {}.", icon_path);
            continue;
        }

        gui.vita_icons[name] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }
}

IconData::IconData()
    : data(nullptr, stbi_image_free) {}

void IconAsyncLoader::commit(GuiState &gui) {
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto &pair : icon_data) {
        if (pair.second.data) {
            gui.app_selector.user_apps_icon[pair.first] = ImGui_Texture(gui.imgui_state.get(), pair.second.data.get(), pair.second.width, pair.second.height);
        }
    }

    icon_data.clear();
}

IconAsyncLoader::IconAsyncLoader(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &app_list) {
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
            IconData data = load_app_icon(gui, emuenv, path);

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

void init_apps_icon(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &app_list) {
    gui.app_selector.icon_async_loader.emplace(gui, emuenv, app_list);
}

void init_app_background(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (gui.apps_background.contains(app_path))
        return;

    const auto APP_INDEX = get_app_index(gui, emuenv, app_path);
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    const auto asset_path = is_emu_app(app_path) ? app_asset_path(app_path) : app_path;
    if (is_emu_app(app_path) || (asset_path.starts_with("NPXS") && asset_path != "NPXS10007")) {
        const auto id = fs::path(asset_path).filename().string();
        vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.vita_fs_path, "app/" + id + "/sce_sys/pic0.png");
    } else
        vfs::read_app_livearea_file(buffer, emuenv.vita_fs_path, asset_path, "sce_sys/pic0.png", APP_INDEX ? APP_INDEX->title_id : asset_path);

    const auto &title = APP_INDEX ? APP_INDEX->title : app_path;

    if (buffer.empty()) {
        LOG_WARN("Background not found for application {} [{}].", title, app_path);
        return;
    }

    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid background for application {} [{}].", title, app_path);
        return;
    }
    gui.apps_background[app_path] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
}

std::string get_sys_lang_name(uint32_t lang_id) {
    const auto current_sys_lang = std::ranges::find_if(LIST_SYS_LANG, [&](const auto &l) {
        return l.first == lang_id;
    });

    if (current_sys_lang == LIST_SYS_LANG.end())
        return fmt::format("Unknown system language ({})", lang_id);

    return current_sys_lang->second;
}

static bool get_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    const auto apps = app::get_apps(emuenv);
    if (!apps.empty()) {
        init_apps_icon(gui, emuenv, apps);
        load_and_update_compat_user_apps(gui, emuenv);
    }

    return !apps.empty();
}

bool set_scroll_animation(float &scroll, float target_scroll, const std::string &target_id, std::function<void(float)> set_scroll) {
    constexpr float duration = 0.25f; // Duration of the animation in seconds
    struct AnimationState {
        float start_time = 0.f;
        float initial_target_scroll = 0.f;
        float initial_scroll = 0.f;
        float last_used_time = 0.f;
        bool initialized = false;
    };
    static std::map<ImGuiID, AnimationState> animation_states;

    const auto now = ImGui::GetTime();
    for (auto it = animation_states.begin(); it != animation_states.end();) {
        if (now - it->second.last_used_time > duration * 4.f)
            it = animation_states.erase(it);
        else
            ++it;
    }

    // Generate a unique ID for the current target based on its string identifier.
    const auto CURRENT_TARGET_ID = ImGui::GetID(target_id.c_str());
    auto &state = animation_states[CURRENT_TARGET_ID];
    state.last_used_time = now;

    // Compute animation progress [0.0, 1.0]
    float elapsed = now - state.start_time;
    float t = std::min(elapsed / duration, 1.0f);

    // Restart when the destination changes.
    const bool is_new_target = !state.initialized || std::abs(target_scroll - state.initial_target_scroll) > 1.f;

    if (is_new_target) {
        // Start a new animation
        state.start_time = now;
        state.initial_scroll = scroll;
        state.initial_target_scroll = target_scroll;
        state.initialized = true;

        t = 0.f;
    }

    // Apply smoothstep easing function: easeInOutCubic
    float eased_t = t * t * (3.0f - 2.0f * t);

    // Interpolate between the initial and target scroll values
    scroll = std::lerp(state.initial_scroll, state.initial_target_scroll, eased_t);

    // Ensure we exactly hit the target value at the end
    if (t >= 1.f)
        scroll = target_scroll;

    // Apply the updated scroll value via the provided callback
    set_scroll(scroll);

    // Return true if the animation is still running
    if (t >= 1.f) {
        animation_states.erase(CURRENT_TARGET_ID);
        return false;
    }
    return true;
}

void init_home(GuiState &gui, EmuEnvState &emuenv) {
    init_vita_icons(gui, emuenv);
    if (!get_user_apps(gui, emuenv) && (emuenv.cfg.load_app_list || !emuenv.cfg.run_app_path))
        init_user_apps(gui, emuenv);
    init_app_background(gui, emuenv, "NPXS10015");

    regmgr::init_regmgr(emuenv.regmgr, emuenv.vita_fs_path);

    const bool is_command_launch = emuenv.cfg.run_app_path || emuenv.cfg.content_path;
    const bool has_configured_user = !emuenv.app.user_list.users.empty()
        && emuenv.app.user_list.users.contains(emuenv.cfg.user_id);
    const bool should_select_user = !has_configured_user
        || (!is_command_launch && !emuenv.cfg.auto_user_login);
    if (should_select_user) {
        emuenv.io.user_id.clear();
        emuenv.io.user_name.clear();
        init_user_management(gui, emuenv);
        return;
    }

    init_user(gui, emuenv, emuenv.cfg.user_id);
    if (!is_command_launch) {
        gui.vita_area.information_bar = true;
        open_user(gui, emuenv);
    }
}

void init_user_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto info = app::read_app_info(emuenv, app_path);
    gui.app_selector.user_apps_icon.erase(info.path);
    app::update_app(emuenv, info);
    init_app_icon(gui, emuenv, info.path);

    gui.app_selector.is_app_list_sorted = false;
}

const ImGui_Texture *get_app_icon(GuiState &gui, const std::string &app_path) {
    const auto key = app_ui_path(app_path);
    const auto &app_type = is_emu_app(key) ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;
    const auto app_icon = app_type.find(key);

    return app_icon != app_type.end() && app_icon->second ? &app_icon->second : nullptr;
}

std::optional<App> get_app_index(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    auto key = app_ui_path(app_path);
    if (key.find(':') == std::string::npos) {
        if (const auto installed = app::get_app(emuenv, key))
            return installed;
    }
    if (!is_emu_app(key) && key.find(':') == std::string::npos && key.starts_with("NPXS") && key != "NPXS10007")
        key = emu_app_path(key);
    if (!is_emu_app(key))
        return app::get_app(emuenv, app_path);

    const auto &app_type = gui.app_selector.sys_apps;
    const auto app_index = std::ranges::find_if(app_type, [&](const App &a) {
        return a.path == key;
    });

    if (app_index != app_type.end())
        return *app_index;
    return app_path.find(':') == std::string::npos ? app::get_app(emuenv, app_path) : std::nullopt;
}

ImU32 get_selectable_color_pulse(const float max_alpha) {
    // Define constants for pulsing effect
    constexpr float speed = 3.f;
    constexpr float min_alpha = 0.1f;

    // Calculate a pulsing color based on time and a speed factor
    const float time = ImGui::GetTime() * speed;
    const float pulse = (sinf(time) + 1.0f) * 0.5f; // Normalize to [0, 1]
    const float alpha = min_alpha + pulse * ((max_alpha / 255.f) - min_alpha);

    // Create a base color with the calculated alpha
    ImVec4 base_color = ImVec4(0.412f, 0.98f, 1.f, alpha);
    return ImGui::ColorConvertFloat4ToU32(base_color);
}

void draw_stretchable_horizontal_image(ImDrawList *draw_list, ImTextureID texture, const ImVec2 &pos, const ImVec2 &size,
    float texture_width, float texture_height, float left_slice_width, float right_slice_width, ImU32 tint,
    float source_x, float source_y, float source_width, float source_height) {
    if (!texture || !draw_list || (size.x <= 0.f) || (size.y <= 0.f) || (texture_width <= 0.f) || (texture_height <= 0.f))
        return;

    if (source_width <= 0.f)
        source_width = texture_width;
    if (source_height <= 0.f)
        source_height = texture_height;

    const float uniform_scale = size.y / source_height;
    float left_dest_width = left_slice_width * uniform_scale;
    float right_dest_width = right_slice_width * uniform_scale;
    if ((left_dest_width + right_dest_width) > size.x) {
        const float edge_scale = size.x / (left_dest_width + right_dest_width);
        left_dest_width *= edge_scale;
        right_dest_width *= edge_scale;
    }

    const float center_dest_width = std::max(0.f, size.x - left_dest_width - right_dest_width);
    const float u0 = source_x / texture_width;
    const float u1 = (source_x + left_slice_width) / texture_width;
    const float u2 = (source_x + source_width - right_slice_width) / texture_width;
    const float u3 = (source_x + source_width) / texture_width;
    const float v0 = source_y / texture_height;
    const float v1 = (source_y + source_height) / texture_height;

    if (left_dest_width > 0.f)
        draw_list->AddImage(texture, pos, ImVec2(pos.x + left_dest_width, pos.y + size.y), ImVec2(u0, v0), ImVec2(u1, v1), tint);

    if (center_dest_width > 0.f) {
        const ImVec2 center_min(pos.x + left_dest_width, pos.y);
        draw_list->AddImage(texture, center_min, ImVec2(center_min.x + center_dest_width, center_min.y + size.y), ImVec2(u1, v0), ImVec2(u2, v1), tint);
    }

    if (right_dest_width > 0.f) {
        const ImVec2 right_min(pos.x + size.x - right_dest_width, pos.y);
        draw_list->AddImage(texture, right_min, ImVec2(right_min.x + right_dest_width, right_min.y + size.y), ImVec2(u2, v0), ImVec2(u3, v1), tint);
    }
}

void draw_stretchable_9slice_image(ImDrawList *draw_list, ImTextureID texture, const ImVec2 &pos, const ImVec2 &size,
    float texture_width, float texture_height, float left_slice_width, float right_slice_width, float top_slice_height, float bottom_slice_height, ImU32 tint,
    float source_x, float source_y, float source_width, float source_height,
    float dest_left_width, float dest_right_width, float dest_top_height, float dest_bottom_height) {
    if (!texture || !draw_list || (size.x <= 0.f) || (size.y <= 0.f) || (texture_width <= 0.f) || (texture_height <= 0.f))
        return;

    if (source_width <= 0.f)
        source_width = texture_width;
    if (source_height <= 0.f)
        source_height = texture_height;

    float left_dest_width = dest_left_width >= 0.f ? dest_left_width : left_slice_width;
    float right_dest_width = dest_right_width >= 0.f ? dest_right_width : right_slice_width;
    float top_dest_height = dest_top_height >= 0.f ? dest_top_height : top_slice_height;
    float bottom_dest_height = dest_bottom_height >= 0.f ? dest_bottom_height : bottom_slice_height;

    if ((left_dest_width + right_dest_width) > size.x) {
        const float width_scale = size.x / (left_dest_width + right_dest_width);
        left_dest_width *= width_scale;
        right_dest_width *= width_scale;
    }
    if ((top_dest_height + bottom_dest_height) > size.y) {
        const float height_scale = size.y / (top_dest_height + bottom_dest_height);
        top_dest_height *= height_scale;
        bottom_dest_height *= height_scale;
    }

    const float center_dest_width = std::max(0.f, size.x - left_dest_width - right_dest_width);
    const float center_dest_height = std::max(0.f, size.y - top_dest_height - bottom_dest_height);
    const float u0 = source_x / texture_width;
    const float u1 = (source_x + left_slice_width) / texture_width;
    const float u2 = (source_x + source_width - right_slice_width) / texture_width;
    const float u3 = (source_x + source_width) / texture_width;
    const float v0 = source_y / texture_height;
    const float v1 = (source_y + top_slice_height) / texture_height;
    const float v2 = (source_y + source_height - bottom_slice_height) / texture_height;
    const float v3 = (source_y + source_height) / texture_height;
    const float x0 = pos.x;
    const float x1 = pos.x + left_dest_width;
    const float x2 = pos.x + size.x - right_dest_width;
    const float x3 = pos.x + size.x;
    const float y0 = pos.y;
    const float y1 = pos.y + top_dest_height;
    const float y2 = pos.y + size.y - bottom_dest_height;
    const float y3 = pos.y + size.y;

    if ((left_dest_width > 0.f) && (top_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(u0, v0), ImVec2(u1, v1), tint);
    if ((center_dest_width > 0.f) && (top_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x1, y0), ImVec2(x2, y1), ImVec2(u1, v0), ImVec2(u2, v1), tint);
    if ((right_dest_width > 0.f) && (top_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x2, y0), ImVec2(x3, y1), ImVec2(u2, v0), ImVec2(u3, v1), tint);
    if ((left_dest_width > 0.f) && (center_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x0, y1), ImVec2(x1, y2), ImVec2(u0, v1), ImVec2(u1, v2), tint);
    if ((center_dest_width > 0.f) && (center_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(u1, v1), ImVec2(u2, v2), tint);
    if ((right_dest_width > 0.f) && (center_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x2, y1), ImVec2(x3, y2), ImVec2(u2, v1), ImVec2(u3, v2), tint);
    if ((left_dest_width > 0.f) && (bottom_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x0, y2), ImVec2(x1, y3), ImVec2(u0, v2), ImVec2(u1, v3), tint);
    if ((center_dest_width > 0.f) && (bottom_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x1, y2), ImVec2(x2, y3), ImVec2(u1, v2), ImVec2(u2, v3), tint);
    if ((right_dest_width > 0.f) && (bottom_dest_height > 0.f))
        draw_list->AddImage(texture, ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(u2, v2), ImVec2(u3, v3), tint);
}

void get_sys_apps_title(GuiState &gui, EmuEnvState &emuenv) {
    gui.app_selector.sys_apps.clear();
    constexpr std::array<const std::string_view, 6> sys_apps_list = { "NPXS10003", "NPXS10006", "NPXS10008", "NPXS10014", "NPXS10015", "NPXS10026" };
    for (const auto &app : sys_apps_list) {
        sfo::SfoAppInfo app_info;
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::vs0, params, emuenv.vita_fs_path, fmt::format("app/{}/sce_sys/param.sfo", app))) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            sfo::get_data_by_key(app_info.app_version, sfo_handle, "APP_VER");
            if (!app_info.app_version.empty() && app_info.app_version[0] == '0')
                app_info.app_version.erase(app_info.app_version.begin());
            sfo::get_data_by_key(app_info.app_category, sfo_handle, "CATEGORY");
            sfo::get_data_by_key(app_info.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", emuenv.cfg.sys_lang));
            sfo::get_data_by_key(app_info.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang));
            boost::trim(app_info.app_title);
            sfo::get_data_by_key(app_info.app_title_id, sfo_handle, "TITLE_ID");
        } else {
            auto &lang = gui.lang.sys_apps_title;
            app_info.app_version = "1.00";
            app_info.app_category = "gda";
            app_info.app_title_id = app;
            if (app == "NPXS10003") {
                app_info.app_short_title = lang["browser"];
                app_info.app_title = lang["internet_browser"];
            } else if (app == "NPXS10006") {
                app_info.app_short_title = app_info.app_title = lang["friends"];
            } else if (app == "NPXS10008") {
                app_info.app_short_title = lang["trophies"];
                app_info.app_title = lang["trophy_collection"];
            } else if (app == "NPXS10014") {
                app_info.app_short_title = app_info.app_title = lang["messages"];
            } else if (app == "NPXS10015")
                app_info.app_short_title = app_info.app_title = lang["settings"];
            else
                app_info.app_short_title = app_info.app_title = lang["content_manager"];
        }
        gui.app_selector.sys_apps.push_back({ app_info.app_version, app_info.app_category, {}, {}, {}, {}, app_info.app_short_title, app_info.app_title, app_info.app_title_id, emu_app_path(std::string(app)) });
    }

    if (fs::exists(emuenv.vita_fs_path / "vs0/vsh/shell"))
        gui.app_selector.sys_apps.push_back({ "1.00", "gda", {}, {}, {}, {}, "PS Vita OS", "PlayStation Vita OS", "NPXS19999", "emu:vsh/shell" });

    std::sort(gui.app_selector.sys_apps.begin(), gui.app_selector.sys_apps.end(), [](const App &lhs, const App &rhs) {
        return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
    });
}

std::map<DateTime, std::string> get_date_time(GuiState &gui, EmuEnvState &emuenv, const tm &date_time) {
    std::map<DateTime, std::string> date_time_str;
    if (!emuenv.io.user_id.empty()) {
        const auto &day_str = gui.lang.common.wday[date_time.tm_wday];
        const auto &month_str = gui.lang.common.ymonth[date_time.tm_mon];
        const auto &days_str = gui.lang.common.mday[date_time.tm_mday];
        const auto year = date_time.tm_year + 1900;
        const auto month = date_time.tm_mon + 1;
        const auto day = date_time.tm_mday;
        switch (emuenv.cfg.sys_date_format) {
        case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD:
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", month_str, days_str, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", year, month, day);
            break;
        case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY: {
            const auto &small_month_str = gui.lang.common.small_ymonth[date_time.tm_mon];
            const auto &small_days_str = gui.lang.common.small_mday[day];
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", small_days_str, small_month_str, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", day, month, year);
            break;
        }
        case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY:
            date_time_str[DateTime::DATE_DETAIL] = fmt::format("{} {} ({})", month_str, days_str, day_str);
            date_time_str[DateTime::DATE_MINI] = fmt::format("{}/{}/{}", month, day, year);
            break;
        }
    }
    const auto clock_12h = emuenv.io.user_id.empty() || (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR);
    if (clock_12h && date_time.tm_hour == 0)
        date_time_str[DateTime::HOUR] = std::to_string(12);
    else
        date_time_str[DateTime::HOUR] = std::to_string(clock_12h && date_time.tm_hour > 12 ? (date_time.tm_hour - 12) : date_time.tm_hour);

    date_time_str[DateTime::CLOCK] = fmt::format("{}:{:0>2d}", date_time_str[DateTime::HOUR], date_time.tm_min);
    date_time_str[DateTime::DAY_MOMENT] = date_time.tm_hour >= 12 ? "PM" : "AM";

    return date_time_str;
}

ImTextureID load_image(GuiState &gui, const uint8_t *data, const int size) {
    int width;
    int height;

    stbi_uc *img_data = stbi_load_from_memory(data, size, &width, &height,
        nullptr, STBI_rgb_alpha);

    if (!img_data)
        return nullptr;

    const auto handle = ImGui_ImplSdl_CreateTexture(gui.imgui_state.get(), img_data, width, height);
    stbi_image_free(img_data);

    return handle;
}

void pre_init(GuiState &gui, EmuEnvState &emuenv) {
    if (ImGui::GetCurrentContext() == NULL) {
        ImGui::CreateContext();
    }
    gui.imgui_state.reset(ImGui_ImplSdl_Init(emuenv.renderer.get(), emuenv.window.get()));

    assert(gui.imgui_state);

    init_style(gui);
    gui::init_lang(gui.lang, emuenv);

    load_fonts(gui, emuenv, false);
}

void load_fonts(GuiState &gui, EmuEnvState &emuenv, bool reload) {
    assert(gui.imgui_state);

    if (reload) {
        ImGui_ImplSdl_InvalidateDeviceObjects(gui.imgui_state.get());
        ImGui::GetIO().Fonts->Clear();
    }

    init_font(gui, emuenv);

    const bool result = ImGui_ImplSdl_CreateDeviceObjects(gui.imgui_state.get());
    assert(result);
}

void init(GuiState &gui, EmuEnvState &emuenv) {
    refresh_modules_list(emuenv);
    get_notice_list(emuenv);
    init_users_avatars(gui, emuenv);
    app::load_app_times(emuenv);

    if (emuenv.cfg.show_welcome)
        gui.help_menu.welcome_dialog = true;

    get_sys_apps_title(gui, emuenv);

    init_home(gui, emuenv);

#ifdef __ANDROID__
    // must be called once for the java side to get the scale
    set_controller_overlay_scale(emuenv.cfg.overlay_scale);
    set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
#endif
}

void draw_begin(GuiState &gui, EmuEnvState &emuenv) {
    ImGui_ImplSdl_NewFrame(gui.imgui_state.get());
}

void draw_end(GuiState &gui) {
    ImGui::Render();
    ImGui_ImplSdl_RenderDrawData(gui.imgui_state.get());
}

void draw_touchpad_cursor(GuiState &gui, EmuEnvState &emuenv) {
    SceTouchPortType port;
    const auto touchpad_fingers_pos = get_touchpad_fingers_pos(emuenv.touch, port);
    if (touchpad_fingers_pos.empty())
        return;

    const auto &viewport = sdl::get_viewport();
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto color = (port == SCE_TOUCH_PORT_FRONT) ? IM_COL32(0.f, 102.f, 204.f, 255.f) : IM_COL32(255.f, 0.f, 0.f, 255.f);
    for (const auto &pos : touchpad_fingers_pos) {
        auto x = viewport.logical_x + (pos.x * viewport.logical_width);
        auto y = viewport.logical_y + (pos.y * viewport.logical_height);
        ImGui::GetForegroundDrawList()->AddCircle(ImVec2(x, y), 20.f * SCALE.x, color, 0, 4.f * SCALE.x);
    }
}

static void process_v3kn_events(GuiState &gui, EmuEnvState &emuenv) {
    for (const auto &event : emuenv.v3kn.take_social_events()) {
        switch (event.type) {
        case SocialEventType::MessagePollError:
            init_info_message({ .function = "v3kn_messages_poll", .title = "V3KN Connection Error", .level = spdlog::level::err, .msg = get_v3kn_error_message(gui.lang, event.response) });
            break;
        case SocialEventType::Message:
            init_info_message({ .function = "v3kn_messages_poll", .title = "V3KN Message", .level = spdlog::level::info, .msg = fmt::format("New message from {}", event.online_id) });
            break;
        case SocialEventType::Friend:
        case SocialEventType::Activity:
            add_social_notice(gui, emuenv, event);
            break;
        }
    }
}

void draw_vita_area(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session) {
    process_v3kn_events(gui, emuenv);
    if (gui.vita_area.start_screen)
        draw_start_screen(gui, emuenv);

    ImGui::PushFont(gui.vita_font[gui.current_font_level]);

    if (gui.vita_area.home_screen)
        draw_home_screen(gui, emuenv, session);

    if (gui.vita_area.manual)
        draw_manual(gui, emuenv);

    // Draw install dialogs
    if (gui.file_menu.archive_install_dialog)
        draw_archive_install_dialog(gui, emuenv);
    if (gui.file_menu.firmware_install_dialog)
        draw_firmware_install_dialog(gui, emuenv);
    if (gui.file_menu.license_install_dialog)
        draw_license_install_dialog(gui, emuenv);
    if (gui.file_menu.pkg_install_dialog)
        draw_pkg_install_dialog(gui, emuenv);

    if (gui.vita_area.user_management)
        draw_user_management(gui, emuenv);

    if (!gui.trophy_unlock_display_requests.empty()) {
        const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
        while (!gui.trophy_unlock_display_requests.empty()) {
            update_notice_info(gui, emuenv, "trophy");
            gui.trophy_unlock_display_requests.pop_back();
        }
    }

    if (emuenv.ime.state && !gui.vita_area.home_screen && !gui.vita_area.user_management && get_sys_apps_state(gui))
        draw_ime(gui, emuenv.ime, emuenv);

    // System App
    if (gui.vita_area.content_manager)
        draw_content_manager(gui, emuenv, session);

    if (gui.vita_area.online_storage)
        draw_online_storage(gui, emuenv, session);

    if (gui.vita_area.settings)
        draw_settings(gui, emuenv);

    if (gui.vita_area.trophy_collection)
        draw_trophy_collection(gui, emuenv);

    if (gui.vita_area.friends)
        draw_friend(gui, emuenv);

    if (gui.vita_area.friend_profile)
        draw_friend_profile(gui, emuenv);

    if (gui.vita_area.friend_activity)
        draw_friend_activity(gui, emuenv);

    if (gui.vita_area.messages)
        draw_messages(gui, emuenv);

    poll_vita3k_update_check(gui);
    if (gui.help_menu.vita3k_update)
        draw_vita3k_update(gui, emuenv);

    if ((emuenv.cfg.show_info_bar || !gui.imgui_render || !gui.vita_area.home_screen) && gui.vita_area.information_bar)
        draw_information_bar(gui, emuenv);

    if (!gui.vita_area.start_screen && gui.vita_area.home_screen)
        draw_indicator(gui, emuenv);

    if (gui.vita_area.pkg_install)
        draw_pkg_install(gui, emuenv);
    draw_notifications(gui, emuenv);

    if (gui.vita_area.update_history_info)
        draw_update_history_infos(gui, emuenv);
    draw_popups(gui, emuenv);

    ImGui::PopFont();
}

void draw_ui(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session) {
    ImGui::PushFont(gui.vita_font[gui.current_font_level]);
    if ((gui.vita_area.home_screen || !emuenv.io.app_path.empty()) && get_sys_apps_state(gui) && !gui.vita_area.user_management && (!emuenv.cfg.show_info_bar || !gui.vita_area.information_bar))
        draw_main_menu_bar(gui, emuenv, session);

    if (gui.configuration_menu.v3kn_dialog)
        draw_v3kn_dialog(gui, emuenv);

    if (gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog)
        draw_settings_dialog(gui, emuenv);

#ifdef __ANDROID__
    if (gui.controls_menu.overlay_dialog)
        draw_overlay_dialog(gui, emuenv);
#endif
    if (gui.controls_menu.controls_dialog)
        draw_controls_dialog(gui, emuenv);
    if (gui.controls_menu.controllers_dialog)
        draw_controllers_dialog(gui, emuenv);

    if (gui.help_menu.about_dialog)
        draw_about_dialog(gui, emuenv);
    if (gui.help_menu.welcome_dialog)
        draw_welcome_dialog(gui, emuenv);

    ImGui::PopFont();

    ImGui::PushFont(gui.monospaced_font[gui.current_font_level]);

    if (gui.debug_menu.threads_dialog)
        draw_threads_dialog(gui, emuenv);
    if (gui.debug_menu.thread_details_dialog)
        draw_thread_details_dialog(gui, emuenv);
    if (gui.debug_menu.semaphores_dialog)
        draw_semaphores_dialog(gui, emuenv);
    if (gui.debug_menu.mutexes_dialog)
        draw_mutexes_dialog(gui, emuenv);
    if (gui.debug_menu.lwmutexes_dialog)
        draw_lw_mutexes_dialog(gui, emuenv);
    if (gui.debug_menu.condvars_dialog)
        draw_condvars_dialog(gui, emuenv);
    if (gui.debug_menu.lwcondvars_dialog)
        draw_lw_condvars_dialog(gui, emuenv);
    if (gui.debug_menu.eventflags_dialog)
        draw_event_flags_dialog(gui, emuenv);
    if (gui.debug_menu.allocations_dialog)
        draw_allocations_dialog(gui, emuenv);
    if (gui.debug_menu.disassembly_dialog)
        draw_disassembly_dialog(gui, emuenv);

    ImGui::PopFont();
}

void draw_ellipsis_text(const std::string &text, const float wrap_width, const ImVec2 init_pos, const ImVec2 align, const ImVec4 &col, const uint32_t max_lines) {
    if (text.empty() || max_lines == 0 || wrap_width <= 0.f)
        return;

    const std::array<std::string, 14> separators{
        "\x0A", "\x0D", "\x20", "\x28", "\x29", "\x2D", "\x40", "\x5B", "\x5D",
        "\xC2\xAE", "\xE3\x80\x80", "\xE3\x80\x8E", "\xE3\x80\x8F", "\xE3\x83\xBB"
    };

    std::vector<std::string> words;
    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        bool found_separator = false;
        for (const auto &separator : separators) {
            if (text.compare(i, separator.size(), separator) != 0)
                continue;

            if (!current.empty()) {
                words.emplace_back(current);
                current.clear();
            }
            words.emplace_back(separator);
            i += separator.size() - 1;
            found_separator = true;
            break;
        }

        if (!found_separator)
            current += text[i];
    }
    if (!current.empty())
        words.emplace_back(current);

    std::vector<std::string> lines;
    lines.reserve(max_lines);
    std::size_t used_words = 0;
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    for (uint32_t line = 0; line < max_lines; ++line) {
        std::string temp_line;
        for (std::size_t i = used_words; i < words.size(); ++i) {
            if (words[i].find('\n') != std::string::npos) {
                lines.emplace_back(temp_line);
                temp_line.clear();
                used_words = i + 1;
                break;
            }

            const std::string test_line = temp_line + words[i];
            if (ImGui::CalcTextSize(test_line.c_str()).x <= wrap_width) {
                temp_line = test_line;
                used_words = i + 1;
                continue;
            }

            if (temp_line.empty()) {
                const std::u32string utf32 = converter.from_bytes(words[i]);
                std::u32string chopped;
                for (const char32_t character : utf32) {
                    const std::string test = converter.to_bytes(chopped + character) + "...";
                    if (ImGui::CalcTextSize(test.c_str()).x > wrap_width)
                        break;
                    chopped += character;
                }
                lines.emplace_back(converter.to_bytes(chopped) + "...");
                used_words = i + 1;
            }
            break;
        }

        if (!temp_line.empty())
            lines.emplace_back(temp_line);
        else if (lines.size() >= max_lines)
            break;
    }

    if (used_words < words.size() && lines.size() == max_lines) {
        std::string &last_line = lines.back();
        if (!last_line.empty() && std::isspace(static_cast<unsigned char>(last_line.front())))
            last_line.erase(last_line.begin());

        const std::u32string utf32 = converter.from_bytes(last_line);
        std::u32string cropped;
        for (const char32_t character : utf32) {
            const std::string test = converter.to_bytes(cropped + character) + "...";
            if (ImGui::CalcTextSize(test.c_str()).x > wrap_width)
                break;
            cropped += character;
        }

        std::string cropped_line = converter.to_bytes(cropped);
        if (!cropped_line.empty() && std::isspace(static_cast<unsigned char>(cropped_line.back())))
            cropped_line.pop_back();
        last_line = cropped_line + "...";
    }

    const float line_height = ImGui::GetFontSize();
    const float block_height = line_height * static_cast<float>(lines.size())
        + ImGui::GetStyle().ItemSpacing.y * static_cast<float>(lines.size() - 1);
    float text_y = init_pos.y;
    if (align.y == -1.f)
        text_y -= block_height;
    else if (align.y == 0.f)
        text_y -= block_height / 2.f;

    float text_x = init_pos.x;
    if (align.x == 1.f) {
        float max_line_width = 0.f;
        for (const auto &line : lines)
            max_line_width = std::max(max_line_width, ImGui::CalcTextSize(line.c_str()).x);
        text_x -= max_line_width;
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto line_width = ImGui::CalcTextSize(lines[i].c_str()).x;
        const auto line_x = align.x == 0.f ? text_x - (line_width / 2.f) : text_x;
        ImGui::SetCursorPos(ImVec2(line_x, text_y + (static_cast<float>(i) * line_height) + (static_cast<float>(i) * ImGui::GetStyle().ItemSpacing.y)));
        ImGui::TextColored(col, "%s", lines[i].c_str());
    }
}

void SetTooltipEx(const char *tooltip) {
    if (ImGui::IsItemHovered()) {
        if (!ImGui::BeginTooltip())
            return;
        ImGui::PushTextWrapPos(ImGui::GetIO().DisplaySize.x - ImGui::GetStyle().WindowPadding.x * 2);
        ImGui::Text("%s", tooltip);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void TextColoredCentered(const ImVec4 &col, const char *text) {
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x) * 0.5f);
    ImGui::TextColored(col, "%s", text);
}

void TextCentered(const char *text) {
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x) * 0.5f);
    ImGui::Text("%s", text);
}

void TextColoredCentered(const ImVec4 &col, const char *text, float wrap_width) {
    const auto window_width = ImGui::GetWindowWidth();
    ImGui::PushTextWrapPos(window_width - wrap_width);
    ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize(text, nullptr, false, window_width - 2.f * wrap_width).x) * 0.5f);
    ImGui::TextColored(col, "%s", text);
    ImGui::PopTextWrapPos();
}

void TextCentered(const char *text, float wrap_width) {
    const auto window_width = ImGui::GetWindowWidth();
    ImGui::PushTextWrapPos(window_width - wrap_width);
    ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize(text, nullptr, false, window_width - 2.f * wrap_width).x) * 0.5f);
    ImGui::Text("%s", text);
    ImGui::PopTextWrapPos();
}

} // namespace gui

namespace ImGui {

void ScrollWhenDragging() {
    ImGuiContext &g = *ImGui::GetCurrentContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGuiWindow *window = g.CurrentWindow;

    static ImGuiID drag_scroll_window_id = 0;
    static bool drag_scroll_active = false;

    if (!io.MouseDown[ImGuiMouseButton_Left]) {
        drag_scroll_active = false;
        drag_scroll_window_id = 0;
        return;
    }

    if (g.HoveredWindow != window)
        return;

    const ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    const float abs_drag_x = fabsf(drag_delta.x);
    const float abs_drag_y = fabsf(drag_delta.y);
    const ImGuiID scroll_x_id = window->GetID("#SCROLLX");
    const ImGuiID scroll_y_id = window->GetID("#SCROLLY");
    const bool active_item_is_scrollbar = (g.ActiveId == scroll_x_id) || (g.ActiveId == scroll_y_id);

    if (!drag_scroll_active) {
        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            return;

        if (abs_drag_y <= abs_drag_x)
            return;

        if ((g.ActiveId != 0) && (g.ActiveIdWindow == window) && active_item_is_scrollbar)
            return;

        drag_scroll_active = true;
        drag_scroll_window_id = window->ID;

        if (g.ActiveId != 0 && g.ActiveIdWindow == window)
            ImGui::ClearActiveID();
    } else if (drag_scroll_window_id != window->ID)
        return;

    ImGui::SetScrollY(window, window->Scroll.y - io.MouseDelta.y);
}

} // namespace ImGui
