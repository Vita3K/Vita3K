// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <renderer/state.h>

#include <boost/algorithm/string/trim.hpp>
#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <io/VitaIoDevice.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>
#include <io/vfs.h>
#include <lang/functions.h>
#include <packages/sfo.h>
#include <regmgr/functions.h>
#include <touch/functions.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <imgui_internal.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <codecvt>
#include <fstream>
#include <locale>
#include <string>
#include <vector>

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
        if (ImGui::Button(emuenv.common_dialog.lang.common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))
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
            const auto fw_font_path{ emuenv.pref_path / "sa0/data/font/pvf" };
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
                    if (!emuenv.cfg.initial_setup || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_S) || (sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_T)) {
                        std::vector<uint8_t> font_source{};
                        if (fs_utils::read_data(default_font_path / "SourceHanSansSC-Bold-Min.ttf", font_source)) {
                            font_data = IM_ALLOC(font_source.size());
                            memcpy(font_data, font_source.data(), font_source.size());
                            io.Fonts->AddFontFromMemoryTTF(font_data, font_source.size(), font_config.SizePixels, &font_config, japanese_and_extra_ranges.Data);
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

    const auto default_fw_icon{ emuenv.pref_path / "vs0/data/internal/livearea/default/sce_sys/icon0.png" };

    const fs::path default_icon{ emuenv.static_assets_path / "data/image/icon.png" };

    const fs::path icon_path = fs::exists(default_fw_icon) ? default_fw_icon : default_icon;
    fs_utils::read_data(icon_path, buffer);

    return buffer;
}

static IconData load_app_icon(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    IconData image;
    vfs::FileBuffer buffer;

    const auto APP_INDEX = get_app_index(gui, app_path);

    if (!vfs::read_app_file(buffer, emuenv.pref_path, app_path, "sce_sys/icon0.png")) {
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
        gui.app_selector.vita_apps_icon[app_path] = ImGui_Texture(gui.imgui_state.get(), data.data.get(), data.width, data.height);
    }
}

IconData::IconData()
    : data(nullptr, stbi_image_free) {}

void IconAsyncLoader::commit(GuiState &gui) {
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto &pair : icon_data) {
        if (pair.second.data) {
            gui.app_selector.vita_apps_icon[pair.first] = ImGui_Texture(gui.imgui_state.get(), pair.second.data.get(), pair.second.width, pair.second.height);
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

void init_apps_icon(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &apps_list) {
    gui.app_selector.icon_async_loader.emplace(gui, emuenv, apps_list);
}

void init_app_background(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if (gui.apps_background.contains(app_path))
        return;

    const auto APP_INDEX = get_app_index(gui, app_path);
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    vfs::read_app_file(buffer, emuenv.pref_path, app_path, "sce_sys/pic0.png");

    const auto &title = APP_INDEX ? APP_INDEX->title : app_path;

    if (buffer.empty()) {
        LOG_WARN("Background not found for application {} in path [{}].", title, app_path);
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

void draw_ellipsis_text(const std::string &text, const float wrap_width, const ImVec2 init_pos, const ImVec2 align, const ImVec4 &col, const uint32_t max_lines) {
    // Add any new separator your found
    const std::array<std::string, 14> full_width_separator{
        "\x0A", // U+000A: Line Feed
        "\x0D", // U+000D: Carriage Return
        "\x20", // U+0020: Space
        "\x28", // U+0028: Left Parenthesis
        "\x29", // U+0029: Right Parenthesis
        "\x2D", // U+002D: Hyphen-Minus
        "\x40", // U+0040: Commercial At
        "\x5B", // U+005B: Left Square Bracket
        "\x5D", // U+005D: Right Square Bracket
        "\xC2\xAE", // U+00AE: Registered Sign
        "\xE3\x80\x80", // U+3000: Ideographic Space
        "\xE3\x80\x8E", // U+300E: Left Corner Bracket
        "\xE3\x80\x8F", // U+300F: Right Corner Bracket
        "\xE3\x83\xBB" // U+30FB: Katakana Middle Dot
    };

    std::vector<std::string> words;
    // Split the text into words and separators
    std::string current;

    for (auto i = 0; i < text.length(); i++) {
        bool found_separator = false;
        for (const auto &sep : full_width_separator) {
            // Check if the current character is a separator
            if (text.substr(i, sep.length()) == sep) {
                if (!current.empty()) {
                    words.emplace_back(current);
                    current.clear();
                }
                words.emplace_back(sep);
                i += sep.length() - 1;
                found_separator = true;
                break;
            }
        }

        if (!found_separator)
            current += text[i];
    }

    if (!current.empty())
        words.emplace_back(current);

    std::vector<std::string> lines;
    lines.reserve(max_lines);
    size_t used_words = 0;
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    float max_width;
    // Iterate through the words and wrap them into lines
    for (int line = 0; line < max_lines; ++line) {
        std::string temp_line;
        size_t start_index = used_words;

        for (size_t i = start_index; i < words.size(); ++i) {
            if (words[i].find("\n") != std::string::npos) {
                lines.emplace_back(temp_line);
                temp_line.clear();
                used_words = i + 1;
                break;
            }

            const std::string test_line = temp_line + words[i];
            const float width = ImGui::CalcTextSize(test_line.c_str()).x;

            if (width <= wrap_width) {
                temp_line = test_line;
                used_words = i + 1;
            } else {
                // If the word alone doesn't fit and temp_line is empty, hard-wrap it safely
                if (temp_line.empty()) {
                    std::u32string utf32 = converter.from_bytes(words[i]);
                    std::u32string chopped;
                    for (char32_t c : utf32) {
                        std::string test = converter.to_bytes(chopped + c) + "...";
                        if (ImGui::CalcTextSize(test.c_str()).x > wrap_width)
                            break;
                        chopped += c;
                    }
                    lines.emplace_back(converter.to_bytes(chopped) + "...");
                    used_words = i + 1;
                }
                break;
            }
        }

        // If we reached the end of the words, add the remaining line
        if (!temp_line.empty()) {
            lines.emplace_back(temp_line);
        } else if (lines.size() >= max_lines) {
            break;
        }
    }

    // Final ellipsis cropping if text was cut
    if ((used_words < words.size()) && (lines.size() == static_cast<size_t>(max_lines))) {
        std::string &last_line = lines.back();
        if (!last_line.empty() && std::isspace(last_line[0]))
            last_line.erase(0, 1);

        std::u32string utf32 = converter.from_bytes(last_line);
        std::u32string cropped;
        for (char32_t c : utf32) {
            std::string test = converter.to_bytes(cropped + c) + "...";
            if (ImGui::CalcTextSize(test.c_str()).x > wrap_width)
                break;
            cropped += c;
        }

        std::string cropped_line = converter.to_bytes(cropped);
        if (!cropped_line.empty() && std::isspace(cropped_line.back()))
            cropped_line.pop_back();
        last_line = cropped_line + "...";
    }

    const float LINE_HEIGHT = ImGui::GetFontSize();
    const float STR_BLOCK_HEIGHT = (LINE_HEIGHT * lines.size()) + (ImGui::GetStyle().ItemSpacing.y * (lines.size() - 1));
    float str_pos_y = init_pos.y;
    if (align.y == -1.f)
        str_pos_y -= STR_BLOCK_HEIGHT;
    else if (align.y == 0.f)
        str_pos_y -= (STR_BLOCK_HEIGHT / 2.f);

    float max_line_width = 0.f;
    float str_pos_x = init_pos.x;
    if (align.x == 1.f) {
        for (const std::string &line : lines) {
            float width = ImGui::CalcTextSize(line.c_str()).x;
            if (width > max_line_width)
                max_line_width = width;
        }
        str_pos_x -= max_line_width;
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string &line = lines[i];
        const auto STR_POS_X = align.x == 0.f ? str_pos_x - (ImGui::CalcTextSize(line.c_str()).x / 2.f) : str_pos_x;
        ImGui::SetCursorPos(ImVec2(STR_POS_X, str_pos_y + (i * LINE_HEIGHT) + (i * ImGui::GetStyle().ItemSpacing.y)));
        ImGui::TextColored(col, "%s", line.c_str());
    }
}

std::string get_sys_lang_name(uint32_t lang_id) {
    const auto current_sys_lang = std::find_if(LIST_SYS_LANG.begin(), LIST_SYS_LANG.end(), [&](const auto &l) {
        return l.first == lang_id;
    });

    return current_sys_lang->second;
}

static bool get_vita_apps(GuiState &gui, EmuEnvState &emuenv) {
    const auto apps_cache_path{ emuenv.pref_path / "ux0/temp/apps.dat" };
    fs::ifstream apps_cache(apps_cache_path, std::ios::in | std::ios::binary);
    if (apps_cache.is_open()) {
        gui.app_selector.vita_apps["ux0"].clear();

        // Read size of apps list
        size_t size;
        apps_cache.read((char *)&size, sizeof(size));

        // Check version of cache
        uint32_t versionInFile;
        apps_cache.read((char *)&versionInFile, sizeof(uint32_t));
        if (versionInFile != 2) {
            LOG_WARN("Current version of cache: {}, is outdated, recreate it.", versionInFile);
            return false;
        }

        // Read language of cache
        apps_cache.read((char *)&gui.app_selector.apps_cache_lang, sizeof(uint32_t));
        if (gui.app_selector.apps_cache_lang != emuenv.cfg.sys_lang) {
            LOG_WARN("Current lang of cache: {}, is different configuration: {}, recreate it.", get_sys_lang_name(gui.app_selector.apps_cache_lang), get_sys_lang_name(emuenv.cfg.sys_lang));
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

            gui.app_selector.vita_apps["ux0"].push_back(app);
        }

        init_apps_icon(gui, emuenv, gui.app_selector.vita_apps["ux0"]);
        load_and_update_compat_user_apps(gui, emuenv);
    }

    return !gui.app_selector.vita_apps["ux0"].empty();
}

void save_apps_cache(GuiState &gui, EmuEnvState &emuenv) {
    const auto temp_path{ emuenv.pref_path / "ux0/temp" };
    fs::create_directories(temp_path);

    fs::ofstream apps_cache(temp_path / "apps.dat", std::ios::out | std::ios::binary);
    if (apps_cache.is_open()) {
        // Write Size of apps list
        const auto size = gui.app_selector.vita_apps["ux0"].size();
        apps_cache.write(reinterpret_cast<const char *>(&size), sizeof(size));

        // Write version of cache
        const uint32_t versionInFile = 2;
        apps_cache.write(reinterpret_cast<const char *>(&versionInFile), sizeof(uint32_t));

        // Write language of cache
        gui.app_selector.apps_cache_lang = emuenv.cfg.sys_lang;
        apps_cache.write(reinterpret_cast<const char *>(&gui.app_selector.apps_cache_lang), sizeof(uint32_t));

        // Write Apps list
        for (const App &app : gui.app_selector.vita_apps["ux0"]) {
            auto write = [&apps_cache](const std::string &i) {
                const size_t size = i.length();

                apps_cache.write(reinterpret_cast<const char *>(&size), sizeof(size));
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

void init_fw_apps(GuiState &gui, EmuEnvState &emuenv) {
    // Add firmware apps working here
    static std::vector<std::string> firmware_apps_paths = {
        "pd0:app/NPXS10007", // Welcome Park
        // "vs0:app/NPXS10000", // Near
        // "vs0:app/NPXS10001", // Party
        // "vs0:app/NPXS10002", // Ps Store
        // "vs0:app/NPXS10003", // Web Browser
        // "vs0:app/NPXS10004", // Picture
        // "vs0:app/NPXS10006", // Friends
        // "vs0:app/NPXS10008", // Trophy
        // "vs0:app/NPXS10009", // Music
        // "vs0:app/NPXS10010", // Video
        // "vs0:app/NPXS10012", // PS3 Remote Play
        // "vs0:app/NPXS10013", // PS4 Link
        // "vs0:app/NPXS10014", // Message
        // "vs0:app/NPXS10015", // Settings
        //"vs0:app/NPXS10018", // Suscription
        //"vs0:app/NPXS10021", // Mobile Network Operator
        "vs0:app/NPXS10026", // Content Manager
        // "vs0:app/NPXS10031", // Package Installer
        // "vs0:app/NPXS10072", // Email
        // "vs0:app/NPXS10078", // Cross-Controller
        // "vs0:app/NPXS10091", // Calendar
        // "vs0:app/NPXS10094", // Parental Controls
        // "vs0:app/NPXS10095", // Panoramic Camera
        // "vs0:app/NPXS10098", // Link to PS4
    };

    for (const auto &app_path : firmware_apps_paths) {
        if (fs::exists(emuenv.pref_path / convert_path(app_path) / "eboot.bin"))
            init_vita_app(gui, emuenv, app_path);
    }
}

void init_home(GuiState &gui, EmuEnvState &emuenv) {
    init_fw_apps(gui, emuenv);
    if (gui.app_selector.vita_apps["ux0"].empty() && (emuenv.cfg.load_app_list || !emuenv.cfg.run_app_path)) {
        if (!get_vita_apps(gui, emuenv))
            init_vita_apps(gui, emuenv);
    }

    init_app_background(gui, emuenv, "vs0:app/NPXS10015");

    regmgr::init_regmgr(emuenv.regmgr, emuenv.pref_path);

    const auto is_cmd = emuenv.cfg.run_app_path || emuenv.cfg.content_path;
    if (!gui.users.empty() && gui.users.contains(emuenv.cfg.user_id) && (is_cmd || emuenv.cfg.auto_user_login)) {
        init_user(gui, emuenv, emuenv.cfg.user_id);
        if (!is_cmd && emuenv.cfg.auto_user_login) {
            gui.vita_area.information_bar = true;
            open_user(gui, emuenv);
        }
    } else
        init_user_management(gui, emuenv);
}

void init_vita_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto device = device::get_device(app_path)._to_string();
    auto &apps = gui.app_selector.vita_apps[device];
    auto it = std::find_if(apps.begin(), apps.end(), [&](const App &a) {
        return a.path == app_path;
    });
    if (it != apps.end()) {
        // If the app already exists, remove it first
        apps.erase(it);
        if (gui.app_selector.vita_apps_icon.contains(app_path))
            gui.app_selector.vita_apps_icon.erase(app_path);
    }

    get_app_param(gui, emuenv, app_path);
    init_app_icon(gui, emuenv, app_path);

    if (!emuenv.io.user_id.empty()) {
        const auto TIME_APP_INDEX = get_time_app_index(gui, emuenv, app_path);
        if (TIME_APP_INDEX != gui.time_apps[emuenv.io.user_id].end())
            get_app_index(gui, app_path)->last_time = TIME_APP_INDEX->last_time_used;

        gui.app_selector.is_app_list_sorted = false;
    }
}

std::map<std::string, ImGui_Texture>::const_iterator get_app_icon(GuiState &gui, const std::string &app_path) {
    const auto &app_type = app_path.starts_with("emu") ? gui.app_selector.emu_apps_icon : gui.app_selector.vita_apps_icon;
    const auto app_icon = std::find_if(app_type.begin(), app_type.end(), [&](const auto &i) {
        return i.first == app_path;
    });

    return app_icon;
}

App *get_app_index(GuiState &gui, const std::string &app_path) {
    const auto device = device::get_device(app_path)._to_string();
    auto &app_type = app_path.starts_with("emu") ? gui.app_selector.emu_apps : gui.app_selector.vita_apps[device];
    const auto app_index = std::find_if(app_type.begin(), app_type.end(), [&](const App &a) {
        return a.path == app_path;
    });

    return (app_index != app_type.end()) ? &(*app_index) : nullptr;
}

void get_app_param(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    sfo::SfoAppInfo app_info;
    vfs::FileBuffer param;
    if (vfs::read_app_file(param, emuenv.pref_path, app_path, "sce_sys/param.sfo")) {
        sfo::get_param_info(app_info, param, emuenv.cfg.sys_lang);
    } else {
        app_info.app_addcont = app_info.app_savedata = app_info.app_short_title = app_info.app_title = app_info.app_title_id = fs::path(app_path).stem().string(); // Use app path as TitleID, addcont, Savedata, Short title and Title
        app_info.app_version = "1.00"; // Default version
        app_info.app_category = app_info.app_parental_level = "N/A";
    }
    const auto device = device::get_device(app_path)._to_string();
    LOG_DEBUG("App found: {}, [{}] in path {}.", app_info.app_title_id, app_info.app_title, app_path);
    gui.app_selector.vita_apps[device].push_back({ app_info.app_version, app_info.app_category, app_info.app_content_id, app_info.app_addcont, app_info.app_savedata, app_info.app_parental_level, app_info.app_short_title, app_info.app_title, app_info.app_title_id, app_path });
}

ImU32 get_selectable_color_pulse(const float max_alpha) {
    // Define constants for pulsing effect
    constexpr float speed = 3.f;
    constexpr float min_alpha = 0.1f;

    // Calculate a pulsing color based on time and a speed factor
    const float time = ImGui::GetTime() * speed;
    const float pulse = (std::sinf(time) + 1.0f) * 0.5f; // Normalize to [0, 1]
    const float alpha = min_alpha + pulse * ((max_alpha / 255.f) - min_alpha);

    // Create a base color with the calculated alpha
    ImVec4 base_color = ImVec4(105.f / 255.f, 250.f / 255.f, 255.f / 255.f, alpha);
    return ImGui::ColorConvertFloat4ToU32(base_color);
}

void get_user_apps_title(GuiState &gui, EmuEnvState &emuenv) {
    const fs::path apps_path{ emuenv.pref_path / "ux0/app" };
    if (!fs::exists(apps_path))
        return;

    for (const auto &app : fs::directory_iterator(apps_path)) {
        if (!app.path().empty() && fs::is_directory(app.path())
            && !app.path().filename_is_dot() && !app.path().filename_is_dot_dot()) {
            const auto app_path = app.path().stem().generic_string();
            get_app_param(gui, emuenv, "ux0:app/" + app_path);
        }
    }

    save_apps_cache(gui, emuenv);
}

void get_sys_apps_title(GuiState &gui, EmuEnvState &emuenv) {
    gui.app_selector.emu_apps.clear();
    constexpr std::array<const std::string_view, 5> sys_apps_list = { "NPXS10003", "NPXS10008", "NPXS10015", "NPXS10026", "NPXS19999" };
    for (const auto &app : sys_apps_list) {
        sfo::SfoAppInfo app_info;
        std::string app_path;
        if (app == "NPXS19999") {
            if (fs::exists(fs::path(emuenv.pref_path) / "vs0/vsh/shell")) {
                app_path = "emu:vsh/shell";
                app_info.app_title_id = app;
                app_info.app_short_title = "PS Vita OS";
                app_info.app_title = "PlayStation Vita OS";
            } else
                continue;
        } else {
            app_path = fmt::format("emu:app/{}", app);
            vfs::FileBuffer params;
            if (vfs::read_file(VitaIoDevice::vs0, params, emuenv.pref_path, fmt::format("app/{}/sce_sys/param.sfo", app))) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                sfo::get_data_by_key(app_info.app_version, sfo_handle, "APP_VER");
                if (app_info.app_version[0] == '0')
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
                } else if (app == "NPXS10008") {
                    app_info.app_short_title = lang["trophies"];
                    app_info.app_title = lang["trophy_collection"];
                } else if (app == "NPXS10015")
                    app_info.app_short_title = app_info.app_title = lang["settings"];
                else
                    app_info.app_short_title = app_info.app_title = lang["content_manager"];
            }
        }
        gui.app_selector.emu_apps.push_back({ app_info.app_version, app_info.app_category, {}, {}, {}, {}, app_info.app_short_title, app_info.app_title, app_info.app_title_id, std::string(app_path) });
    }

    std::sort(gui.app_selector.emu_apps.begin(), gui.app_selector.emu_apps.end(), [](const App &lhs, const App &rhs) {
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
    init_font(gui, emuenv);
    gui::init_bgm_player(emuenv.cfg.bgm_volume);
    lang::init_lang(gui.lang, emuenv);

    bool result = ImGui_ImplSdl_CreateDeviceObjects(gui.imgui_state.get());
    assert(result);
}

void init(GuiState &gui, EmuEnvState &emuenv) {
    get_modules_list(gui, emuenv);
    get_notice_list(emuenv);
    get_users_list(gui, emuenv);
    get_time_apps(gui, emuenv);

    if (emuenv.cfg.show_welcome)
        gui.help_menu.welcome_dialog = true;

    get_sys_apps_title(gui, emuenv);

    init_home(gui, emuenv);

    // Initialize trophy callback
    emuenv.np.trophy_state.trophy_unlock_callback = [&gui](NpTrophyUnlockCallbackData &callback_data) {
        const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
        gui.trophy_unlock_display_requests.insert(gui.trophy_unlock_display_requests.begin(), callback_data);
    };

#ifdef __ANDROID__
    // must be called once for the java side to get the scale
    set_controller_overlay_scale(emuenv.cfg.overlay_scale);
    set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
#endif
}

void draw_begin(GuiState &gui, EmuEnvState &emuenv) {
    ImGui_ImplSdl_NewFrame(gui.imgui_state.get());
    emuenv.renderer_focused = !ImGui::GetIO().WantCaptureMouse;

    // async loading, renderer texture creation needs to be synchronous
    // cant bind opengl context outside main thread on macos now
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->commit(gui);

    if (!gui.themes_package_async.empty()) {
        const auto &content_id = gui.themes_package_async.front();
        init_theme_package(gui, emuenv, content_id);
        gui.themes_package_async.erase(gui.themes_package_async.begin());
    }
}

void draw_end(GuiState &gui) {
    ImGui::Render();
    ImGui_ImplSdl_RenderDrawData(gui.imgui_state.get());
}

void draw_touchpad_cursor(EmuEnvState &emuenv) {
    SceTouchPortType port;
    const auto touchpad_fingers_pos = get_touchpad_fingers_pos(port);
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

void draw_vita_area(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.vita_area.start_screen)
        draw_start_screen(gui, emuenv);

    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    if ((emuenv.cfg.show_info_bar || !emuenv.display.imgui_render || !gui.vita_area.home_screen) && gui.vita_area.information_bar)
        draw_information_bar(gui, emuenv);

    if (gui.vita_area.app_close)
        draw_app_close(gui, emuenv);

    if (gui.vita_area.home_screen)
        draw_home_screen(gui, emuenv);

    if (gui.vita_area.manual)
        draw_manual(gui, emuenv);

    if (gui.vita_area.pkg_install)
        draw_pkg_install(gui, emuenv);

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

    if (emuenv.cfg.show_compile_shaders && gui.shaders_compiled_display_count > 0)
        draw_shaders_count_compiled(gui, emuenv);

    if (!gui.trophy_unlock_display_requests.empty())
        draw_trophies_unlocked(gui, emuenv);

    if (emuenv.ime.state && !gui.vita_area.home_screen && !gui.vita_area.user_management && get_sys_apps_state(gui))
        draw_ime(emuenv.ime, emuenv);

    // System App
    if (gui.vita_area.content_manager)
        draw_content_manager(gui, emuenv);

    if (gui.vita_area.settings)
        draw_settings(gui, emuenv);

    if (gui.vita_area.trophy_collection)
        draw_trophy_collection(gui, emuenv);

    if (gui.help_menu.vita3k_update)
        draw_vita3k_update(gui, emuenv);

    if (gui.vita_area.update_history_info)
        draw_update_history_infos(gui, emuenv);

    // Info Message
    if (!gui.info_message.msg.empty())
        draw_info_message(gui, emuenv);

    ImGui::PopFont();
}

void draw_ui(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    if ((gui.vita_area.home_screen || !emuenv.io.app_path.empty()) && get_sys_apps_state(gui) && !gui.vita_area.user_management && (!emuenv.cfg.show_info_bar || !gui.vita_area.information_bar))
        draw_main_menu_bar(gui, emuenv);

    if (gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog)
        draw_settings_dialog(gui, emuenv);

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
    if (g.HoveredWindow == window && ImGui::IsMouseDragging(0)) {
        ImGui::SetScrollY(window, window->Scroll.y - io.MouseDelta.y);
        ImGui::SetActiveID(0, window);
    }
}

} // namespace ImGui
