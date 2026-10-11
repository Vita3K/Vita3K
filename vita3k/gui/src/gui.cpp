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

#include <gui/functions.h>

#include <app/functions.h>
#include <app/session_controller.h>
#include <bgm_player/functions.h>
#include <config/version.h>
#include <emuenv/app_launch_request.h>
#include <gui/imgui_impl_sdl.h>
#include <gui/state.h>
#include <interface.h>
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
#include <lang/functions.h>
#include <packages/sfo.h>
#include <regmgr/functions.h>
#include <touch/functions.h>
#include <util/fs.h>
#include <util/info_message.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <imgui_internal.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <stb_image.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL_video.h>
#include <fmt/format.h>

#include <cerrno>
#include <cstring>
#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <cstdlib>
#include <jni.h>
#else
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif
#endif

namespace gui {

void draw_info_message(GuiState &gui, EmuEnvState &emuenv) {
    if (emuenv.io.title_id.empty() && emuenv.cfg.display_info_message) {
        const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
        const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
        const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

        const ImVec2 WINDOW_SIZE(680.0f * SCALE.x, 320.0f * SCALE.y);
        const ImVec2 BUTTON_SIZE(160.f * SCALE.x, 46.f * SCALE.y);

        ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##information", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
        ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::BeginChild("##info", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
        const auto &title = gui.info_message.title;
        ImGui::SetWindowFontScale(RES_SCALE.x);
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        const auto text_size = ImGui::CalcTextSize(gui.info_message.msg.c_str(), 0, false, WINDOW_SIZE.x - (24.f * SCALE.x));
        const auto text_pos = ImVec2((WINDOW_SIZE.x / 2.f) - (text_size.x / 2.f), (WINDOW_SIZE.y / 2.f) - (text_size.y / 2.f) - (24 * SCALE.y));
        ImGui::SetCursorPos(text_pos);
        ImGui::TextWrapped("%s", gui.info_message.msg.c_str());
        ImGui::SetCursorPosY(WINDOW_SIZE.y - BUTTON_SIZE.y - (42.0f * SCALE.y));
        ImGui::Separator();
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
        if (ImGui::Button(emuenv.common_dialog.lang.common["ok"].c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross))
            gui.info_message = {};
        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::End();
    } else {
        spdlog::log(gui.info_message.level, "[{}] {}", gui.info_message.function, gui.info_message.msg);
        gui.info_message = {};
    }
}

static void init_style(EmuEnvState &emuenv) {
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

    style->ScaleAllSizes(emuenv.manual_dpi_scale);

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
    io.FontGlobalScale = emuenv.manual_dpi_scale;
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
            emuenv.max_font_level = font_scale_count - 1;
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

    if (!vfs::read_app_file(buffer, emuenv.vita_fs_path, app_path, "sce_sys/icon0.png")) {
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

    const auto is_sys = app_path.starts_with("NPXS") && (app_path != "NPXS10007");
    if (is_sys)
        vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.vita_fs_path, "app/" + app_path + "/sce_sys/pic0.png");
    else
        vfs::read_app_file(buffer, emuenv.vita_fs_path, app_path, "sce_sys/pic0.png");

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
    const auto current_sys_lang = std::find_if(LIST_SYS_LANG.begin(), LIST_SYS_LANG.end(), [&](const auto &l) {
        return l.first == lang_id;
    });

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
    // Persistent state for animation tracking (keeps values between frames)
    static float start_time = 0.f;
    static float initial_target_scroll = 0.f;
    static float initial_scroll = 0.f;
    static ImGuiID initial_target_id = 0;

    constexpr float duration = 0.25f; // Duration of the animation in seconds

    // Generate a unique ID for the current target based on its string identifier
    const auto CURRENT_TARGET_ID = ImGui::GetID(target_id.c_str());

    // Compute animation progress [0.0, 1.0]
    float elapsed = ImGui::GetTime() - start_time;
    float t = std::min(elapsed / duration, 1.0f);

    // Determine if a new animation target has been set (position or ID changed)
    const bool is_new_target = std::abs(target_scroll - initial_target_scroll) > 1.f || CURRENT_TARGET_ID != initial_target_id;

    if (is_new_target) {
        // Start a new animation
        start_time = ImGui::GetTime();
        initial_scroll = scroll;
        initial_target_scroll = target_scroll;
        initial_target_id = CURRENT_TARGET_ID;

        // Only reset t if the previous animation has finished
        if (t >= 1.f)
            t = 0.f;
    }

    // Apply smoothstep easing function: easeInOutCubic
    float eased_t = t * t * (3.0f - 2.0f * t);

    // Interpolate between the initial and target scroll values
    scroll = std::lerp(initial_scroll, initial_target_scroll, eased_t);

    // Ensure we exactly hit the target value at the end
    if (t >= 1.f)
        scroll = target_scroll;

    // Apply the updated scroll value via the provided callback
    set_scroll(scroll);

    // Return true if the animation is still running
    return t < 1.f;
}

void init_home(GuiState &gui, EmuEnvState &emuenv) {
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
    gui.app_selector.user_apps_icon.erase(app_path);

    app::update_app(emuenv, app::read_app_info(emuenv, app_path));
    init_app_icon(gui, emuenv, app_path);

    gui.app_selector.is_app_list_sorted = false;
}

std::map<std::string, ImGui_Texture>::const_iterator get_app_icon(GuiState &gui, const std::string &app_path) {
    const auto &app_type = app_path.starts_with("NPXS") && (app_path != "NPXS10007") ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;
    const auto app_icon = std::find_if(app_type.begin(), app_type.end(), [&](const auto &i) {
        return i.first == app_path;
    });

    return app_icon;
}

std::optional<App> get_app_index(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (!(app_path.starts_with("NPXS") && (app_path != "NPXS10007")))
        return app::get_app(emuenv, app_path);

    const auto &app_type = gui.app_selector.sys_apps;
    const auto app_index = std::find_if(app_type.begin(), app_type.end(), [&](const App &a) {
        return a.path == app_path;
    });

    return (app_index != app_type.end()) ? std::optional<App>(*app_index) : std::nullopt;
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

void get_sys_apps_title(GuiState &gui, EmuEnvState &emuenv) {
    gui.app_selector.sys_apps.clear();
    constexpr std::array<const std::string_view, 4> sys_apps_list = { "NPXS10003", "NPXS10008", "NPXS10015", "NPXS10026" };
    for (const auto &app : sys_apps_list) {
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::vs0, params, emuenv.vita_fs_path, fmt::format("app/{}/sce_sys/param.sfo", app))) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            sfo::get_data_by_key(emuenv.app_info.app_version, sfo_handle, "APP_VER");
            if (emuenv.app_info.app_version[0] == '0')
                emuenv.app_info.app_version.erase(emuenv.app_info.app_version.begin());
            sfo::get_data_by_key(emuenv.app_info.app_category, sfo_handle, "CATEGORY");
            sfo::get_data_by_key(emuenv.app_info.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", emuenv.cfg.sys_lang));
            sfo::get_data_by_key(emuenv.app_info.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang));
            boost::trim(emuenv.app_info.app_title);
            sfo::get_data_by_key(emuenv.app_info.app_title_id, sfo_handle, "TITLE_ID");
        } else {
            auto &lang = gui.lang.sys_apps_title;
            emuenv.app_info.app_version = "1.00";
            emuenv.app_info.app_category = "gda";
            emuenv.app_info.app_title_id = app;
            if (app == "NPXS10003") {
                emuenv.app_info.app_short_title = lang["browser"];
                emuenv.app_info.app_title = lang["internet_browser"];
            } else if (app == "NPXS10008") {
                emuenv.app_info.app_short_title = lang["trophies"];
                emuenv.app_info.app_title = lang["trophy_collection"];
            } else if (app == "NPXS10015")
                emuenv.app_info.app_short_title = emuenv.app_info.app_title = lang["settings"];
            else
                emuenv.app_info.app_short_title = emuenv.app_info.app_title = lang["content_manager"];
        }
        gui.app_selector.sys_apps.push_back({ emuenv.app_info.app_version, emuenv.app_info.app_category, {}, {}, {}, {}, emuenv.app_info.app_short_title, emuenv.app_info.app_title, emuenv.app_info.app_title_id, std::string(app) });
    }

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

    init_style(emuenv);
    lang::init_lang(gui.lang, emuenv);

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

static bool run_execv(char *argv[], const AppLaunchRequest &request,
    const std::string &self_path, const std::string &configured_app_args) {
    const auto &exec_path = request.self_path.empty() ? self_path : request.self_path;
    std::string exec_args = configured_app_args;
    if (!request.argv.empty()) {
        exec_args = request.argv.front();
        for (size_t i = 1; i < request.argv.size(); ++i)
            exec_args += ", " + request.argv[i];
    }

#ifdef __ANDROID__
    (void)argv;
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity || request.app_path.empty())
        return false;

    jclass clazz = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(clazz, "restartApp", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!method)
        return false;

    jstring app_path = env->NewStringUTF(request.app_path.c_str());
    jstring jexec_path = env->NewStringUTF(exec_path.c_str());
    jstring app_args = env->NewStringUTF(exec_args.c_str());
    env->CallVoidMethod(activity, method, app_path, jexec_path, app_args);
    exit(0);
#else
    if (!argv || !argv[0] || request.app_path.empty())
        return false;

    const char *args[10] = { argv[0], "-a", "true", "-r", request.app_path.c_str() };
    if (!exec_path.empty()) {
        args[5] = "--self";
        args[6] = exec_path.c_str();
        if (!exec_args.empty()) {
            args[7] = "--app-args";
            args[8] = exec_args.c_str();
        }
    } else if (!exec_args.empty()) {
        args[5] = "--app-args";
        args[6] = exec_args.c_str();
    }

#ifdef _WIN32
    FreeConsole();
    _execv(argv[0], args);
#elif defined(__unix__) || defined(__APPLE__)
    execv(argv[0], const_cast<char *const *>(args));
#endif
    LOG_ERROR("Failed to relaunch Vita3K: {}", std::strerror(errno));
    return false;
#endif
}

ExitCode run_frontend(EmuEnvState &emuenv, char *argv[]) {
    if (emuenv.cfg.console)
        return Success;

    const auto renderer_config = emuenv.cfg.current_config;

    // Qt initializes this when its main window is created. The ImGui frontend
    // must do the same before the graphics settings dialog is drawn.
    if (!emuenv.vulkan_device_info)
        emuenv.vulkan_device_info = std::make_unique<renderer::VulkanDeviceInfo>(renderer::enumerate_vulkan_devices());

    const auto run_app_path = emuenv.cfg.run_app_path;
#ifdef __ANDROID__
    // The process outlives the frontend on Android. Declared before gui so that the ImGui context
    // is destroyed after it, on every exit path: a leftover context would keep its font atlas and
    // backend data pointing to the destroyed renderer.
    struct ImGuiContextGuard {
        ~ImGuiContextGuard() {
            if (ImGui::GetCurrentContext() != NULL)
                ImGui::DestroyContext();
        }
    } imgui_context_guard;
#endif
    GuiState gui;
    // Declared after gui so that the backend is shut down before the GUI textures are destroyed
    // and while the renderer is still alive.
    struct ImGuiBackendGuard {
        GuiState &gui;
        EmuEnvState &emuenv;
        ~ImGuiBackendGuard() {
            if (gui.imgui_state && !gui.imgui_state->shutdown && emuenv.renderer
                && gui.imgui_state->renderer == emuenv.renderer.get())
                ImGui_ImplSdl_Shutdown(gui.imgui_state.get());
        }
    } imgui_backend_guard{ gui, emuenv };
    pre_init(gui, emuenv);
#ifdef __ANDROID__
    app::update_viewport(emuenv);
#endif
    bgm_player::init_bgm_player(emuenv.cfg.bgm_volume);

    auto present = std::chrono::system_clock::now();
    auto later = std::chrono::system_clock::now();
    constexpr double frame_time = 1000.0 / 60.0;

    const auto wait_for_frame_done = [&]() {
        present = std::chrono::system_clock::now();
        const std::chrono::duration<double, std::milli> work_time = present - later;
        if (work_time.count() < frame_time) {
            const std::chrono::duration<double, std::milli> delta_ms(frame_time - work_time.count());
            const auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
            std::this_thread::sleep_for(delta_ms_duration);
        }
        later = std::chrono::system_clock::now();
    };

    if (!emuenv.cfg.initial_setup) {
        emuenv.cfg.system_music.emplace(false);
        if (bgm_player::init_bgm(emuenv.vita_fs_path, true))
            bgm_player::switch_bgm_state(true);

        while (!emuenv.cfg.initial_setup) {
            wait_for_frame_done();
            if (!handle_events(emuenv, gui)) {
                bgm_player::destroy_bgm_player();
                return QuitRequested;
            }

            draw_begin(gui, emuenv);
            draw_initial_setup(gui, emuenv);
            draw_end(gui);
            emuenv.renderer->swap_window();
        }

        if (!gui.fw_font)
            load_fonts(gui, emuenv, true);
    }

    init(gui, emuenv);
    app::update_viewport(emuenv);

    app::AppSessionController session(emuenv, true);
    bool renderer_cleaned = false;
    const auto stop_session = [&](const app::AppSessionStopReason reason) {
        const bool had_active_session = session.has_active_session();
        session.stop(reason);
#ifdef __ANDROID__
        if (had_active_session)
            set_controller_overlay_state(0);
#endif
        if (had_active_session && emuenv.renderer)
            renderer_cleaned = true;
        if (emuenv.renderer)
            emuenv.renderer->frontend_frame_callback = {};
    };
    const auto update_sdl_runtime_title = [&]() {
        const auto &cc = emuenv.cfg.current_config;
        const auto af = cc.anisotropic_filtering > 1
            ? fmt::format(" | AF {}x", cc.anisotropic_filtering)
            : "";
        const auto x = emuenv.display.next_rendered_frame.image_size.x * cc.resolution_multiplier;
        const auto y = emuenv.display.next_rendered_frame.image_size.y * cc.resolution_multiplier;
        const std::string title = fmt::format("{} | {} ({}) | {} | {} FPS ({} ms) | {}x{}{} | {}",
            window_title, emuenv.current_app_title, emuenv.io.title_id,
            cc.backend_renderer,
            emuenv.fps, emuenv.ms_per_frame,
            x, y, af, cc.screen_filter);
        SDL_SetWindowTitle(emuenv.window.get(), title.c_str());
    };
    const auto draw_frontend = [&](const bool renderer_owns_frame) {
        if (!renderer_owns_frame) {
            wait_for_frame_done();
            if (!emuenv.renderer->set_current())
                return;
        }

        std::lock_guard<std::mutex> render_lock(gui.render_mutex);
        draw_begin(gui, emuenv);
        if (gui.app_selector.icon_async_loader)
            gui.app_selector.icon_async_loader->commit(gui);
        draw_vita_area(gui, emuenv, &session);
        if (renderer_owns_frame) {
            if (emuenv.cfg.current_config.show_touchpad_cursor && !emuenv.kernel.is_threads_paused())
                draw_touchpad_cursor(emuenv);
        }

        if (emuenv.display.imgui_render)
            draw_ui(gui, emuenv, &session);

        draw_end(gui);
        if (!renderer_owns_frame)
            emuenv.renderer->swap_window();
    };
    bool auto_boot_consumed = !run_app_path.has_value();
    const auto launch_request = [&]() -> std::optional<AppLaunchRequest> {
        if (auto request = emuenv.take_app_launch_request())
            return request;
        if (run_app_path && !auto_boot_consumed) {
            auto_boot_consumed = true;
            AppLaunchRequest request{ .app_path = *run_app_path };
            return request;
        }
        return std::nullopt;
    };
    const auto launch_app = [&](const AppLaunchRequest &request) {
        if (!session.begin_launch(request, request.reason != AppLaunchReason::LoadExec)) {
            LOG_ERROR("Could not begin launching application {}.", request.app_path);
            gui.vita_area.home_screen = true;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = true;
            return false;
        }

        const auto &current_config = emuenv.cfg.current_config;
        const bool renderer_config_changed = emuenv.renderer
            && (emuenv.renderer->current_backend != emuenv.backend_renderer
                || !get_emulator_restart_required_settings(renderer_config, current_config).empty());
        if (renderer_config_changed) {
            const auto restart_self_path = request.self_path.empty() ? emuenv.self_path : request.self_path;
            const auto restart_app_args = emuenv.cfg.app_args;
            if (!renderer_cleaned)
                stop_session(app::AppSessionStopReason::FrontendShutdown);
            if (run_execv(argv, request, restart_self_path, restart_app_args))
                return true;
            gui.vita_area.home_screen = true;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = true;
            return false;
        }
        if (!emuenv.cfg.show_gui)
            emuenv.display.imgui_render = false;
        init_ime_lang(emuenv.ime, static_cast<SceImeLanguage>(emuenv.cfg.current_ime_lang));
        SDL_SetWindowTitle(emuenv.window.get(), fmt::format("{} | {} ({}) | Please wait, loading...", window_title, emuenv.current_app_title, emuenv.io.title_id).c_str());
        if (!emuenv.frame_host
            || !session.initialize_renderer(*emuenv.frame_host)
            || !session.initialize_runtime()) {
            stop_session(app::AppSessionStopReason::LaunchFailure);
            gui.vita_area.home_screen = true;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = true;
            return false;
        }
        renderer_cleaned = false;
        emuenv.renderer->frontend_frame_callback = [&]() { draw_frontend(true); };
        emuenv.np.trophy_state.add_trophy_unlock_callback([&gui](NpTrophyUnlockCallbackData &callback_data) {
            const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
            gui.trophy_unlock_display_requests.insert(gui.trophy_unlock_display_requests.begin(), callback_data);
        });
        if (!session.load_and_run()) {
            stop_session(app::AppSessionStopReason::LaunchFailure);
            gui.vita_area.home_screen = true;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = true;
            return false;
        }
        bgm_player::switch_bgm_state(true);
#ifdef __ANDROID__
        if (session.is_running())
            set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg));
#endif
        return true;
    };

    bool running = true;
#if USE_DISCORD
    bool discord_rich_presence_old = false;
    const auto sync_discord = [&]() {
        if (!discordrpc::update_init_status(emuenv.cfg.discord_rich_presence, &discord_rich_presence_old))
            return;
        // Discord has just been connected: publish the current state.
        if (session.has_active_session())
            discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
        else
            discordrpc::update_presence();
    };
#endif
    while (running) {
        if (auto request = launch_request())
            launch_app(*request);

        if (!session.has_active_session()) {
            if (!emuenv.cfg.show_gui)
                emuenv.display.imgui_render = true;
            SDL_SetWindowTitle(emuenv.window.get(), window_title);
            if (!handle_events(emuenv, gui))
                break;
#if USE_DISCORD
            sync_discord();
#endif
            draw_frontend(false);
            continue;
        }

        app::LaunchRuntimeMetrics metrics;
        while (session.is_running()) {
            wait_for_frame_done();
            if (!handle_events(emuenv, gui, &session)) {
                stop_session(app::AppSessionStopReason::FrontendShutdown);
                running = false;
                break;
            }
#if USE_DISCORD
            sync_discord();
#endif
            if (!emuenv.kernel.is_threads_paused() && app::update_runtime_metrics(emuenv, metrics)) {
                update_sdl_runtime_title();
            }
            const auto request = emuenv.take_app_launch_request();
            if (request) {
                stop_session(app::AppSessionStopReason::Relaunch);
                if (request->reason != AppLaunchReason::ProcessExit) {
                    gui.vita_area.app_close = false;
                    gui.vita_area.home_screen = false;
                    gui.vita_area.live_area_screen = false;
                    gui.vita_area.information_bar = false;
                    launch_app(*request);
                } else {
                    gui.vita_area.app_close = false;
                    if (!gui.vita_area.online_storage
                        && !gui.vita_area.live_area_screen
                        && !gui.vita_area.home_screen) {
                        gui.vita_area.home_screen = true;
                        gui.vita_area.information_bar = true;
                    }
                }
                break;
            }
            bool app_close = false;
            bool live_area_screen = false;
            {
                std::lock_guard<std::mutex> render_lock(gui.render_mutex);
                app_close = gui.vita_area.app_close;
                live_area_screen = gui.vita_area.live_area_screen;
            }
            if (app_close && !live_area_screen) {
                stop_session(app::AppSessionStopReason::UserRequest);
                gui.vita_area.app_close = false;
                gui.vita_area.home_screen = true;
                gui.vita_area.information_bar = true;
                break;
            }

            if (emuenv.renderer->precompile_requested
                && !emuenv.renderer->precompile_complete.load(std::memory_order_acquire)) {
                continue;
            }

            if (emuenv.kernel.threads.empty()) {
                stop_session(app::AppSessionStopReason::UserRequest);
                break;
            }
        }
    }

    if (session.has_active_session())
        stop_session(app::AppSessionStopReason::FrontendShutdown);
    if (emuenv.renderer)
        emuenv.renderer->frontend_frame_callback = {};
    bgm_player::destroy_bgm_player();
    return Success;
}

void draw_begin(GuiState &gui, EmuEnvState &emuenv) {
    ImGui_ImplSdl_NewFrame(gui.imgui_state.get());
}

void draw_end(GuiState &gui) {
    ImGui::Render();
    ImGui_ImplSdl_RenderDrawData(gui.imgui_state.get());
}

void draw_touchpad_cursor(EmuEnvState &emuenv) {
    SceTouchPortType port;
    const auto touchpad_fingers_pos = get_touchpad_fingers_pos(emuenv.touch, port);
    if (touchpad_fingers_pos.empty())
        return;

    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto color = (port == SCE_TOUCH_PORT_FRONT) ? IM_COL32(0.f, 102.f, 204.f, 255.f) : IM_COL32(255.f, 0.f, 0.f, 255.f);
    for (const auto &pos : touchpad_fingers_pos) {
        auto x = emuenv.logical_viewport_pos.x + (pos.x * emuenv.logical_viewport_size.x);
        auto y = emuenv.logical_viewport_pos.y + (pos.y * emuenv.logical_viewport_size.y);
        ImGui::GetForegroundDrawList()->AddCircle(ImVec2(x, y), 20.f * SCALE.x, color, 0, 4.f * SCALE.x);
    }
}

static void draw_connecting_please_wait(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(VIEWPORT_POS);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE);
    ImGui::Begin("please_wait", &gui.vita_area.connecting_please_wait, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
    const ImVec2 WINDOW_SIZE(520.f * SCALE.x, 120.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f)));
    ImGui::BeginChild("please_wait_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const auto text = emuenv.common_dialog.lang.common["connecting_please_wait"];
    const auto text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - text_size.x) / 2.f, (WINDOW_SIZE.y - text_size.y) / 2.f));
    ImGui::Text("%s", text.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static void draw_please_wait(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(VIEWPORT_POS);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE);
    ImGui::Begin("please_wait", &gui.vita_area.please_wait, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration);
    const ImVec2 WINDOW_SIZE(520.f * SCALE.x, 120.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f)));
    ImGui::BeginChild("please_wait_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const auto text = ::lang::get(::lang::str::please_wait);
    const auto text_size = ImGui::CalcTextSize(text.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - text_size.x) / 2.f, (WINDOW_SIZE.y - text_size.y) / 2.f));
    ImGui::Text("%s", text.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void draw_vita_area(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session) {
    if (gui.vita_area.start_screen)
        draw_start_screen(gui, emuenv);

    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);

    if (gui.vita_area.app_close)
        draw_app_close(gui, emuenv);

    if (gui.vita_area.home_screen)
        draw_home_screen(gui, emuenv, session);

    if (gui.vita_area.live_area_screen)
        draw_live_area_screen(gui, emuenv, session);
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

    if (emuenv.ime.state && !gui.vita_area.home_screen && !gui.vita_area.live_area_screen && !gui.vita_area.user_management && get_sys_apps_state(gui))
        draw_ime(emuenv.ime, emuenv);

    // System App
    if (gui.vita_area.content_manager)
        draw_content_manager(gui, emuenv);

    if (gui.vita_area.online_storage)
        draw_online_storage(gui, emuenv, session);

    if (gui.vita_area.settings)
        draw_settings(gui, emuenv);

    if (gui.vita_area.trophy_collection)
        draw_trophy_collection(gui, emuenv);

    if (gui.help_menu.vita3k_update)
        draw_vita3k_update(gui, emuenv);

    if ((emuenv.cfg.show_info_bar || !emuenv.display.imgui_render || !gui.vita_area.home_screen) && gui.vita_area.information_bar)
        draw_information_bar(gui, emuenv);

    if (gui.info_message.msg.empty()) {
        if (auto message = util::info_message_queue().try_pop())
            gui.info_message = std::move(*message);
    }

    // Info Message
    if (!gui.info_message.msg.empty())
        draw_info_message(gui, emuenv);

    if (gui.vita_area.connecting_please_wait)
        draw_connecting_please_wait(gui, emuenv);
    if (gui.vita_area.please_wait)
        draw_please_wait(gui, emuenv);

    ImGui::PopFont();
}

void draw_ui(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session) {
    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    if ((gui.vita_area.home_screen || !emuenv.io.app_path.empty()) && get_sys_apps_state(gui) && !gui.vita_area.live_area_screen && !gui.vita_area.user_management && (!emuenv.cfg.show_info_bar || !gui.vita_area.information_bar))
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

    ImGui::PushFont(gui.monospaced_font[emuenv.current_font_level]);

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
