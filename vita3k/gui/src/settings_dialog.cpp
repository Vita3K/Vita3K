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

#include <app/functions.h>
#include <audio/state.h>
#include <bgm_player/functions.h>
#include <config/functions.h>
#include <config/settings.h>
#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <gui/functions.h>
#include <gui/state.h>
#include <host/dialog/filesystem.h>
#include <io/state.h>
#include <kernel/state.h>
#include <lang/functions.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/texture_cache.h>

#include <emuenv/state.h>

#include <string>
#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <SDL3/SDL_video.h>

#include <SDL3/SDL_camera.h>
#include <algorithm>
#include <camera/state.h>
#include <imgui_internal.h>
#include <pugixml.hpp>
#include <util/vector_utils.h>

#undef ERROR

namespace gui {

std::vector<config::RestartRequiredSetting> get_emulator_restart_required_settings(
    const Config::CurrentConfig &before,
    const Config::CurrentConfig &after) {
    const auto changed = config::get_restart_required_settings(before, after);
    std::vector<config::RestartRequiredSetting> emulator_restart_required;
    const bool uses_vulkan = before.backend_renderer == "Vulkan" || after.backend_renderer == "Vulkan";

    for (const auto setting : changed) {
        switch (setting) {
        case config::RestartRequiredSetting::BackendRenderer:
            emulator_restart_required.emplace_back(setting);
            break;
        case config::RestartRequiredSetting::GraphicsDevice:
        case config::RestartRequiredSetting::HighAccuracy:
        case config::RestartRequiredSetting::MemoryMapping:
        case config::RestartRequiredSetting::ValidationLayer:
            if (uses_vulkan)
                emulator_restart_required.emplace_back(setting);
            break;
#ifdef __ANDROID__
        case config::RestartRequiredSetting::CustomDriver:
            if (uses_vulkan)
                emulator_restart_required.emplace_back(setting);
            break;
#endif
        default:
            break;
        }
    }

    return emulator_restart_required;
}

/**
 * @brief Struct used to abstract the code that manages app-specific config files
 *
 * This struct matches in type to the one found in `emuenv.cfg.current_config` and is
 * used to collect the proper values that must be set in `emuenv.cfg.current_config`
 * depending on whether an app-specific config file is being used or not.
 *
 * If an app-specific config file is loaded, then the values in this struct will be
 * set to those of the app-specific config file before getting used to set up `emuenv.cfg.current_config`
 * with those same values. If an app-specific config isn't loaded, then the values in this struct
 * will be set those of the global emulator settings before getting used to set up `emuenv.cfg.current_config`.
 */
static Config::CurrentConfig config;

enum class SettingsDialogSection {
    Core,
    CPU,
    GPU,
    Audio,
    Camera,
    System,
    Emulator,
    Gui,
    Network,
    Debug
};

static SettingsDialogSection current_settings_section = SettingsDialogSection::Core;
static std::vector<std::pair<std::string, bool>> modules;

void refresh_modules_list(const EmuEnvState &emuenv) {
    modules = config::get_modules_list(emuenv.vita_fs_path, config.lle_modules);
}

bool has_available_modules() {
    return !modules.empty();
}

static void reset_emulator(GuiState &gui, EmuEnvState &emuenv) {
    gui.configuration_menu.settings_dialog = false;
    gui.vita_area.home_screen = false;

    // Clean and save new config value
    emuenv.cfg.auto_user_login = false;
    emuenv.cfg.user_id.clear();
    emuenv.cfg.set_vita_fs_path(emuenv.vita_fs_path);
    emuenv.io.user_id.clear();
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    // Stop the Background Music
    bgm_player::stop_bgm();

    refresh_modules_list(emuenv);
    get_sys_apps_title(gui, emuenv);
    get_notice_list(emuenv);
    app::load_app_times(emuenv);
    init_users_avatars(gui, emuenv);
    init_home(gui, emuenv);
}

static void change_emulator_path(GuiState &gui, EmuEnvState &emuenv) {
    fs::path emulator_path{};
    host::dialog::filesystem::Result result = host::dialog::filesystem::pick_folder(emulator_path);

    if (result == host::dialog::filesystem::Result::SUCCESS && emulator_path != emuenv.vita_fs_path) {
        // Refresh the working paths
        app::switch_emulator_path(emuenv, emulator_path);

        // TODO: Move app old to new path
        reset_emulator(gui, emuenv);
        LOG_INFO("Successfully moved Vita3K path to: {}", emuenv.vita_fs_path);
    }

    if (result == host::dialog::filesystem::Result::ERROR) {
        LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
    }
}

static int current_aniso_filter_log, max_aniso_filter_log, audio_backend_idx, current_user_lang;
static std::vector<std::string> list_user_lang;

/** Initialize the dialog draft from the global or app-specific configuration. */
void init_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    Config config_copy;
    config_copy = emuenv.cfg;
    config::set_current_config(config_copy, emuenv.config_path, app_path);
    config = config_copy.current_config;

    list_user_lang.clear();
    const auto get_list_user_lang = [&](const fs::path &path) {
        for (const auto &lang : fs::directory_iterator(path / "lang/user")) {
            if (lang.path().extension() == ".xml") {
                const auto lang_file_name = lang.path().filename().replace_extension().string();
                list_user_lang.push_back(lang_file_name);
            }
        }
    };

#ifndef __ANDROID__
    get_list_user_lang(emuenv.static_assets_path);
    if (emuenv.static_assets_path != emuenv.shared_path)
        get_list_user_lang(emuenv.shared_path);
#else
    list_user_lang.insert(list_user_lang.end(), { "id", "ms", "ua" });
#endif

    current_user_lang = emuenv.cfg.user_lang.empty() ? 0 : (vector_utils::find_index(list_user_lang, emuenv.cfg.user_lang) + 1);

    refresh_modules_list(emuenv);
    current_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(config.anisotropic_filtering)));
    max_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(emuenv.renderer->get_max_anisotropic_filtering())));
    audio_backend_idx = (config.audio_backend == "SDL") ? 0 : 1;
    emuenv.app_path = app_path;
    gui.vita_area.home_screen = false;
    gui.vita_area.information_bar = true;
}

static app::SettingsCommitResult save_config(GuiState &gui, EmuEnvState &emuenv) {
    Config desired_cfg;
    desired_cfg = emuenv.cfg;
    desired_cfg.current_config = config;

    const auto scope_app_path = gui.configuration_menu.custom_settings_dialog ? emuenv.app_path : std::string{};
    if (scope_app_path.empty())
        config::copy_current_config_to_global(desired_cfg);
    const auto result = app::commit_settings(emuenv, desired_cfg, scope_app_path);

    if (!scope_app_path.empty())
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    if (result.runtime_settings_applied && emuenv.renderer) {
        emuenv.renderer->set_stretch_display(emuenv.cfg.current_config.stretch_the_display_area);
        emuenv.renderer->stretch_hd_pixel_perfect(emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect);
    }

    bgm_player::set_bgm_volume(emuenv.cfg.bgm_volume);
    return result;
}

static std::vector<const char *> cameras_list(2);
static const std::vector<const char *> *get_cameras_list(LangState::SettingsDialog &lang) {
    // set on every frame in case of lang changes
    cameras_list[0] = lang.camera["solid_color"].c_str();
    cameras_list[1] = lang.camera["static_image"].c_str();
    static bool camera_list_initialised = false;
    if (!camera_list_initialised) {
        int cameras_count = 0;
        auto sdl_cameras = SDL_GetCameras(&cameras_count);
        for (int i = 0; i < cameras_count; i++) {
            cameras_list.push_back(SDL_GetCameraName(sdl_cameras[i]));
        }
        SDL_free(sdl_cameras);
        camera_list_initialised = true;
    }
    return &cameras_list;
}

static int get_camera_index_by_name(const char *camera_name) {
    for (size_t i = 2; i < cameras_list.size(); ++i) {
        if (strcmp(cameras_list[i], camera_name) == 0) {
            return static_cast<int>(i - 2);
        }
    }
    return -1;
}

static int get_camera_combobox_index(int camera_type, const std::string &camera_id) {
    if (camera_type >= 2) {
        int camera_index = get_camera_index_by_name(camera_id.c_str());
        if (camera_index >= 0) {
            camera_type = camera_index + 2;
        } else {
            camera_type = 0; // Default to solid color if camera not found
        }
    }
    return camera_type;
}

void draw_settings_dialog(GuiState &gui, EmuEnvState &emuenv) {
    auto &user = emuenv.app.user_list.users[emuenv.io.user_id];
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    auto &lang = gui.lang.settings_dialog;
    auto &common = emuenv.common_dialog.lang;
    auto &firmware_font = gui.lang.install_dialog.firmware_install;

    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    const auto is_custom_config = gui.configuration_menu.custom_settings_dialog;
    auto &show_settings_dialog = is_custom_config ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    constexpr auto flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##settings", &show_settings_dialog, flags);
    ImGui::PopStyleVar();

    constexpr auto BG_PATH = "NPXS10015";
    const auto draw_list = ImGui::GetWindowDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (gui.apps_background.contains(BG_PATH))
        draw_list->AddImage(gui.apps_background[BG_PATH], VIEWPORT_POS, BG_POS_MAX);
    else
        draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const auto TITLE_BAR_HEIGHT = 64.f * SCALE.y;
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    const auto settings_str = lang.main_window["title"];
    const auto title_text_width = ImGui::CalcTextSize(settings_str.c_str());
    ImGui::SetCursorPosY((TITLE_BAR_HEIGHT - title_text_width.y) / 2.f);
    const auto app = get_app_index(gui, emuenv, emuenv.app_path);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, (is_custom_config && app ? fmt::format("{}: {} [{}]", settings_str, app->title, fs::path(emuenv.app_path).stem().string()) : settings_str).c_str());
    ImGui::SetCursorPosY(TITLE_BAR_HEIGHT);
    ImGui::Separator();
    ImGui::SetWindowFontScale(0.85f * RES_SCALE.x);

    const auto BUTTON_SIZE = ImVec2(240.f * SCALE.x, 44.f * SCALE.y);
    const float sidebar_width = 190.f * SCALE.x;
    const float sidebar_item_height = 38.f * SCALE.y;
    const float sidebar_item_spacing = 6.f * SCALE.y;

    const ImVec4 BASE_COLOR(0.70f, 0.90f, 1.00f, 0.20f);
    const ImVec4 HOVERED_COLOR(0.78f, 0.94f, 1.00f, 0.30f);
    const ImVec4 ACTIVE_COLOR(0.62f, 0.86f, 1.00f, 0.38f);

    const auto draw_settings_section_button = [&](const char *label, const SettingsDialogSection section) {
        const bool is_selected = current_settings_section == section;
        ImGui::PushID(static_cast<int>(section));
        ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? GUI_COLOR_TEXT_TITLE : GUI_COLOR_TEXT);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.95f, 1.00f, 0.28f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.84f, 0.97f, 1.00f, 0.36f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.90f, 1.00f, 0.42f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BASE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ACTIVE_COLOR);
        }
        if (ImGui::Button(label, ImVec2(-1.f, sidebar_item_height)))
            current_settings_section = section;
        ImGui::PopStyleColor(4);
        ImGui::PopID();
    };

    ImGui::SetCursorPosY(TITLE_BAR_HEIGHT);
    const auto CONTENT_HEIGHT = WINDOW_SIZE.y - (TITLE_BAR_HEIGHT * 2.f);
    const float ROUNDED_SIZE = 8.f * SCALE.x;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ROUNDED_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, ROUNDED_SIZE);

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.92f, 0.97f, 1.00f, 0.42f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, BASE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.10f, 0.16f, 0.18f, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, BASE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, HOVERED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ACTIVE_COLOR);

    ImGui::BeginChild("##settings_sections", ImVec2(sidebar_width, CONTENT_HEIGHT), 1, flags);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, sidebar_item_spacing));
    draw_settings_section_button(lang.core["title"].c_str(), SettingsDialogSection::Core);
    draw_settings_section_button("CPU", SettingsDialogSection::CPU);
    draw_settings_section_button("GPU", SettingsDialogSection::GPU);
    draw_settings_section_button(lang.audio["title"].c_str(), SettingsDialogSection::Audio);
    draw_settings_section_button(lang.camera["title"].c_str(), SettingsDialogSection::Camera);
    draw_settings_section_button(lang.system["title"].c_str(), SettingsDialogSection::System);
    draw_settings_section_button(lang.emulator["title"].c_str(), SettingsDialogSection::Emulator);
    draw_settings_section_button(lang.gui["title"].c_str(), SettingsDialogSection::Gui);
    draw_settings_section_button(lang.network["title"].c_str(), SettingsDialogSection::Network);
    draw_settings_section_button(gui.lang.main_menubar.debug["title"].c_str(), SettingsDialogSection::Debug);
    ImGui::ScrollWhenDragging();
    ImGui::PopStyleVar();
    ImGui::EndChild();

    const ImVec2 CONTENT_SIZE = ImVec2(VIEWPORT_SIZE.x - sidebar_width, CONTENT_HEIGHT);
    ImGui::SameLine(0.f, 0.f);
    ImGui::BeginChild("##settings_content", CONTENT_SIZE, 1, flags);

    switch (current_settings_section) {
    case SettingsDialogSection::Core: {
        ImGui::Spacing();
        if (!modules.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang.core["modules_mode"].c_str());
            ImGui::Spacing();
            ImGui::RadioButton(lang.core["automatic"].c_str(), &config.modules_mode, 0);
            SetTooltipEx(lang.core["automatic_description"].c_str());
            ImGui::SameLine();
            ImGui::RadioButton(lang.core["auto_manual"].c_str(), &config.modules_mode, 1);
            SetTooltipEx(lang.core["auto_manual_description"].c_str());
            ImGui::SameLine();
            ImGui::RadioButton(lang.core["manual"].c_str(), &config.modules_mode, 2);
            SetTooltipEx(lang.core["manual_description"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang.core["modules_list"].c_str());
            SetTooltipEx(lang.core["select_modules"].c_str());
            ImGui::Spacing();
            ImGui::PushItemWidth(260 * SCALE.x);
            if (ImGui::BeginListBox("##modules_list", { 0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.25f + ImGui::GetStyle().FramePadding.y * 2.0f })) {
                for (auto &m : modules) {
                    const auto module = std::find(config.lle_modules.begin(), config.lle_modules.end(), m.first);
                    const bool module_existed = (module != config.lle_modules.end());
                    if (!gui.module_search_bar.PassFilter(m.first.c_str()))
                        continue;
                    if (ImGui::Selectable(m.first.c_str(), &m.second, config.modules_mode == ModulesMode::AUTOMATIC ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                        if (module_existed)
                            config.lle_modules.erase(module);
                        else
                            config.lle_modules.push_back(m.first);
                    }
                    ImGui::ScrollWhenDragging();
                }
                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.core["search_modules"].c_str());
            gui.module_search_bar.Draw("##module_search_bar", 260 * SCALE.x);
            ImGui::Spacing();
            if (ImGui::Button(lang.core["clear_list"].c_str())) {
                config.lle_modules.clear();
                for (auto &m : modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.core["no_modules"].c_str());
            if (ImGui::Button(gui.lang.welcome["download_firmware"].c_str()))
                get_firmware_file(emuenv);
        }
        if (ImGui::Button(lang.core["refresh_list"].c_str()))
            refresh_modules_list(emuenv);
        break;
    }
    case SettingsDialogSection::CPU: {
        ImGui::Spacing();
        ImGui::Checkbox(lang.cpu["cpu_opt"].c_str(), &config.cpu_opt);
        SetTooltipEx(lang.cpu["cpu_opt_description"].c_str());
        break;
    }
    case SettingsDialogSection::GPU: {
        ImGui::Spacing();

#ifdef __APPLE__
        ImGui::BeginDisabled();
#endif
        static const char *LIST_BACKEND_RENDERER[] = { "OpenGL", "Vulkan" };
        if (ImGui::Combo(lang.gpu["backend_renderer"].c_str(), reinterpret_cast<int *>(&emuenv.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
            config.backend_renderer = LIST_BACKEND_RENDERER[static_cast<int>(emuenv.backend_renderer)];
        SetTooltipEx(lang.gpu["select_backend_renderer"].c_str());
#ifdef __APPLE__
        ImGui::EndDisabled();
#else
        ImGui::Spacing();
#endif

        const bool is_vulkan = (emuenv.backend_renderer == renderer::Backend::Vulkan);
        const bool is_ingame = !emuenv.io.title_id.empty();
        const bool is_renderer_changed = (emuenv.backend_renderer != emuenv.renderer->current_backend);
        if (is_vulkan && !is_renderer_changed) {
#ifdef __ANDROID__
            const bool supports_custom_drivers = emuenv.renderer->support_custom_drivers();
            static std::vector<std::string> custom_driver_names;
            static bool custom_driver_names_loaded = false;
            if (supports_custom_drivers && !custom_driver_names_loaded) {
                custom_driver_names = app::get_custom_drivers();
                custom_driver_names_loaded = true;
            }
#endif

            std::vector<std::string> gpu_list_str;
#ifdef __ANDROID__
            if (supports_custom_drivers) {
                gpu_list_str.push_back(lang.gpu["default_custom_driver"]);
                gpu_list_str.insert(gpu_list_str.end(), custom_driver_names.begin(), custom_driver_names.end());
            } else
#endif
                gpu_list_str = emuenv.vulkan_device_info ? emuenv.vulkan_device_info->gpu_names : std::vector<std::string>{};

            if (!gpu_list_str.empty())
                config.gpu_idx = std::clamp(config.gpu_idx, 0, static_cast<int>(gpu_list_str.size()) - 1);

            // must convert to a vector of char*
            std::vector<const char *> gpu_list;
            for (const auto &gpu : gpu_list_str)
                gpu_list.push_back(gpu.c_str());
            ImGui::Combo(lang.gpu["gpu"].c_str(), &config.gpu_idx, gpu_list.data(), static_cast<int>(gpu_list.size()));
            SetTooltipEx(lang.gpu["select_gpu"].c_str());

#ifdef __ANDROID__
            if (supports_custom_drivers) {
                config.custom_driver_name = config.gpu_idx == 0 ? "" : custom_driver_names[config.gpu_idx - 1];

                if (ImGui::Button(lang.gpu["add_custom_driver"].c_str())) {
                    app::add_custom_driver(emuenv);
                    custom_driver_names = app::get_custom_drivers();
                    config.gpu_idx = 0;
                    config.custom_driver_name = "";
                }

                ImGui::SameLine();
                if (ImGui::Button(lang.gpu["download_custom_driver"].c_str()))
                    open_path("https://github.com/K11MCH1/AdrenoToolsDrivers/releases/");

                if (config.gpu_idx > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button(lang.gpu["remove_custom_driver"].c_str())) {
                        app::remove_custom_driver(emuenv, config.custom_driver_name);
                        config.gpu_idx = 0;
                        config.custom_driver_name = "";
                        custom_driver_names = app::get_custom_drivers();
                    }
                }
            }
#endif

            if (is_ingame)
                ImGui::BeginDisabled();

            const char *LIST_RENDERER_ACCURACY[] = { lang.gpu["standard"].c_str(), lang.gpu["high"].c_str() };
            int is_high_accuracy = static_cast<int>(config.high_accuracy);
            ImGui::Combo(lang.gpu["renderer_accuracy"].c_str(), &is_high_accuracy, LIST_RENDERER_ACCURACY, IM_ARRAYSIZE(LIST_RENDERER_ACCURACY));
            config.high_accuracy = static_cast<bool>(is_high_accuracy);

            if (is_ingame)
                ImGui::EndDisabled();
        } else if (!is_vulkan) {
            ImGui::Checkbox(lang.gpu["v_sync"].c_str(), &config.v_sync);
            SetTooltipEx(lang.gpu["v_sync_description"].c_str());
            ImGui::SameLine();
        }
        bool has_surface_sync = !is_vulkan || (emuenv.renderer->supported_mapping_methods_mask > 1);
#ifdef __ANDROID__
        has_surface_sync &= is_vulkan;
#endif

        const bool has_integer_multiplier = static_cast<int>(config.resolution_multiplier * 4.0f) % 4 == 0;
        // OpenGL does not support surface sync with a non-integer resolution multiplier
        if (!is_vulkan && !has_integer_multiplier)
            has_surface_sync = false;

        if (!has_surface_sync)
            config.disable_surface_sync = true;

        if (has_surface_sync) {
            // surface sync is supported on vulkan only when memory mapping is enabled
            ImGui::Checkbox(lang.gpu["disable_surface_sync"].c_str(), &config.disable_surface_sync);
            SetTooltipEx(lang.gpu["surface_sync_description"].c_str());

            if (is_vulkan)
                ImGui::SameLine();
        }

        if (is_vulkan) {
            ImGui::Checkbox(lang.gpu["async_pipeline_compilation"].c_str(), &config.async_pipeline_compilation);
            SetTooltipEx(lang.gpu["async_pipeline_compilation_description"].c_str());
        }

        // Screen Filter
        ImGui::Spacing();
        int curr_filter = 0;
        const std::array<const char *, 5> possible_filters = {
            lang.gpu["nearest"].c_str(),
            lang.gpu["bilinear"].c_str(),
            lang.gpu["bicubic"].c_str(),
            "FXAA",
            "FSR"
        };
        const int filters_available = emuenv.renderer->get_supported_filters();
        std::vector<const char *> filters;
        for (int i = 0; i < possible_filters.size(); i++) {
            if (config.screen_filter == possible_filters[i])
                curr_filter = filters.size();

            if ((1 << i) & filters_available)
                filters.push_back(possible_filters[i]);
        }

        if (ImGui::Combo(lang.gpu["screen_filter"].c_str(), &curr_filter, filters.data(), filters.size()))
            config.screen_filter = filters[curr_filter];
        SetTooltipEx(lang.gpu["screen_filter_description"].c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Resolution Upscaling
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["internal_resolution_upscaling"].c_str());
        ImGui::Spacing();
        ImGui::PushID("Res scal");
        if (config.resolution_multiplier == 0.5f)
            ImGui::BeginDisabled();
        if (ImGui::Button("<", ImVec2(20.f * SCALE.x, 0)))
            config.resolution_multiplier -= 0.25f;
        if (config.resolution_multiplier == 0.5f)
            ImGui::EndDisabled();
        ImGui::SameLine(0, 5.f * SCALE.x);
        ImGui::PushItemWidth(-100.f * SCALE.x);
        int slider_position = static_cast<int>(config.resolution_multiplier * 4);
        if (ImGui::SliderInt("##res_scal", &slider_position, 2, 32, fmt::format("{}x", config.resolution_multiplier).c_str(), ImGuiSliderFlags_None)) {
            config.resolution_multiplier = static_cast<float>(slider_position) / 4.0f;
            if (config.resolution_multiplier != 1.0f && !is_vulkan)
                config.disable_surface_sync = true;
        }
        ImGui::PopItemWidth();
        SetTooltipEx(lang.gpu["internal_resolution_upscaling_description"].c_str());
        ImGui::SameLine(0, 5 * SCALE.x);
        if (config.resolution_multiplier == 8.0f)
            ImGui::BeginDisabled();
        if (ImGui::Button(">", ImVec2(20.f * SCALE.x, 0)))
            config.resolution_multiplier += 0.25f;
        if (config.resolution_multiplier == 8.0f)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if ((config.resolution_multiplier == 1.0f) && !config.disable_surface_sync)
            ImGui::BeginDisabled();
        if (ImGui::Button(lang.gpu["reset"].c_str(), ImVec2(60.f * SCALE.x, 0)))
            config.resolution_multiplier = 1.0f;

        if ((config.resolution_multiplier == 1.0f) && !config.disable_surface_sync)
            ImGui::EndDisabled();
        ImGui::Spacing();
        const auto res_scal = fmt::format("{}x{}", static_cast<int>(960 * config.resolution_multiplier), static_cast<int>(544 * config.resolution_multiplier));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(res_scal.c_str()).x / 2.f) - (35.f * SCALE.x));
        ImGui::Text("%s", res_scal.c_str());
        ImGui::PopID();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Anisotropic Filtering
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["anisotropic_filtering"].c_str());
        ImGui::Spacing();
        ImGui::PushID("Aniso filter");
        if (config.anisotropic_filtering == 1)
            ImGui::BeginDisabled();
        if (ImGui::Button("<", ImVec2(20.f * SCALE.x, 0)))
            config.anisotropic_filtering = 1 << --current_aniso_filter_log;
        if (config.anisotropic_filtering == 1)
            ImGui::EndDisabled();
        ImGui::SameLine(0, 5 * SCALE.x);
        ImGui::PushItemWidth(-100.f * SCALE.x);
        if (ImGui::SliderInt("##aniso_filter", &current_aniso_filter_log, 0, max_aniso_filter_log, fmt::format("{}x", config.anisotropic_filtering).c_str()))
            config.anisotropic_filtering = 1 << current_aniso_filter_log;
        ImGui::PopItemWidth();
        SetTooltipEx(lang.gpu["anisotropic_filtering_description"].c_str());
        ImGui::SameLine(0, 5 * SCALE.x);
        if (current_aniso_filter_log == max_aniso_filter_log)
            ImGui::BeginDisabled();
        if (ImGui::Button(">", ImVec2(20.f * SCALE.x, 0)))
            config.anisotropic_filtering = 1 << ++current_aniso_filter_log;
        if (current_aniso_filter_log == max_aniso_filter_log)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (config.anisotropic_filtering == 1)
            ImGui::BeginDisabled();
        if (ImGui::Button(lang.gpu["reset"].c_str(), ImVec2(60.f * SCALE.x, 0))) {
            config.anisotropic_filtering = 1;
            current_aniso_filter_log = 0;
        }
        if (config.anisotropic_filtering == 1)
            ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Texture Replacement
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["texture_replacement"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gpu["export_textures"].c_str(), &config.export_textures);
        ImGui::SameLine();
        ImGui::Checkbox(lang.gpu["import_textures"].c_str(), &config.import_textures);

        static const char *export_formats[] = { "PNG", "DDS" };
        int export_format_pos = config.export_as_png ? 0 : 1;
        if (ImGui::Combo(lang.gpu["texture_exporting_format"].c_str(), &export_format_pos, export_formats, IM_ARRAYSIZE(export_formats)))
            config.export_as_png = export_format_pos == 0;

        // FPS hack
        ImGui::Checkbox(lang.gpu["fps_hack"].c_str(), &config.fps_hack);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", lang.gpu["fps_hack_description"].c_str());
        }

        if (emuenv.renderer->supported_mapping_methods_mask > 1 && !is_renderer_changed) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (is_ingame)
                ImGui::BeginDisabled();

            std::vector<const char *> mapping_methods_strings = {
                lang.gpu["disabled"].c_str(),
                lang.gpu["double_buffer"].c_str(),
                lang.gpu["external_host"].c_str(),
                lang.gpu["page_table"].c_str(),
                lang.gpu["native_buffer"].c_str()
            };
            std::vector<std::string_view> mapping_methods_indexes = {
                "disabled",
                "double-buffer",
                "external-host",
                "page-table",
                "native-buffer"
            };

            // only get the mapping methods that are available on this GPU
            int list_pos = 0;
            for (int i = 0; i < 5; i++) {
                if ((1 << i) & emuenv.renderer->supported_mapping_methods_mask) {
                    list_pos++;
                } else {
                    mapping_methods_strings.erase(mapping_methods_strings.begin() + list_pos);
                    mapping_methods_indexes.erase(mapping_methods_indexes.begin() + list_pos);
                }
            }

            static int current_mapping = std::find(mapping_methods_indexes.begin(), mapping_methods_indexes.end(), config.memory_mapping) - mapping_methods_indexes.begin();
            if (ImGui::Combo(lang.gpu["mapping_method"].c_str(), &current_mapping, mapping_methods_strings.data(), mapping_methods_strings.size())) {
                config.memory_mapping = mapping_methods_indexes[current_mapping];
            }
            SetTooltipEx(lang.gpu["mapping_method_description"].c_str());

            if (is_ingame)
                ImGui::EndDisabled();
        }

#ifdef __ANDROID__
        if (emuenv.renderer->support_custom_drivers()) {
            ImGui::Spacing();
            ImGui::Checkbox(lang.gpu["turbo_mode"].c_str(), &emuenv.cfg.turbo_mode);
            SetTooltipEx(lang.gpu["turbo_mode_description"].c_str());
        }
#endif

        // Shaders
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["shaders"].c_str());
        ImGui::Spacing();
        if (!is_vulkan) {
            ImGui::Checkbox(lang.gpu["shader_cache"].c_str(), &emuenv.cfg.shader_cache);
            SetTooltipEx(lang.gpu["shader_cache_description"].c_str());
        }
        if (emuenv.renderer->features.spirv_shader) {
            ImGui::SameLine();
            ImGui::Checkbox(lang.gpu["spirv_shader"].c_str(), &emuenv.cfg.spirv_shader);
            SetTooltipEx(lang.gpu["spirv_shader_description"].c_str());
        }
        const auto shaders_cache_path{ emuenv.cache_path / "shaders" };
        if (fs::exists(shaders_cache_path) && !fs::is_empty(shaders_cache_path)) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gpu["clean_shaders"].c_str())) {
                fs::remove_all(shaders_cache_path);
                fs::remove_all(emuenv.cache_path / "shaderlog");
                fs::remove_all(emuenv.log_path / "shaderlog");
            }
        }

        break;
    }

    case SettingsDialogSection::Audio: {
        ImGui::Spacing();
        if (!emuenv.io.app_path.empty())
            ImGui::BeginDisabled();
        static const char *LIST_BACKEND_AUDIO[] = { "SDL", "Cubeb" };
        if (ImGui::Combo(lang.audio["audio_backend"].c_str(), &audio_backend_idx, LIST_BACKEND_AUDIO, IM_ARRAYSIZE(LIST_BACKEND_AUDIO)))
            config.audio_backend = LIST_BACKEND_AUDIO[audio_backend_idx];
        SetTooltipEx(lang.audio["select_audio_backend"].c_str());
        if (!emuenv.io.app_path.empty())
            ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::SliderInt(lang.audio["audio_volume"].c_str(), &config.audio_volume, 0, 100, "%d %%", ImGuiSliderFlags_AlwaysClamp);
        SetTooltipEx(lang.audio["audio_volume_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.audio["enable_ngs_support"].c_str(), &config.ngs_enable);
        SetTooltipEx(lang.audio["ngs_description"].c_str());
        ImGui::Spacing();
        ImGui::SliderInt(lang.audio["bgm_volume"].c_str(), &emuenv.cfg.bgm_volume, 0, 100, "%d %%", ImGuiSliderFlags_AlwaysClamp);
        SetTooltipEx(lang.audio["bgm_volume_description"].c_str());
        ImGui::Separator();
        ImGui::Spacing();
        break;
    }

    case SettingsDialogSection::Camera: {
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.camera["front_camera"].c_str());
        // Combo box for camera selection
        static int front_camera = -1;
        auto &cameras = *get_cameras_list(lang);
        if (front_camera == -1) {
            init_default_cameras(emuenv.cfg);
            front_camera = get_camera_combobox_index(emuenv.cfg.front_camera_type, emuenv.cfg.front_camera_id);
        }
        ImGui::Combo("##Front_camera", &front_camera, cameras.data(), cameras.size());
        auto old_front_camera_id = emuenv.cfg.front_camera_id;
        auto old_front_camera_type = emuenv.cfg.front_camera_type;
        auto old_front_camera_color = emuenv.cfg.front_camera_color;
        auto old_front_camera_image = emuenv.cfg.front_camera_image;
        emuenv.cfg.front_camera_type = front_camera;
        if (front_camera >= 2) {
            // Save selected camera name
            emuenv.cfg.front_camera_id = cameras[front_camera];
            emuenv.cfg.front_camera_type = 2;
        } else if (front_camera == 0) {
            // Color input field
            static ImVec4 color = ImGui::ColorConvertU32ToFloat4(emuenv.cfg.front_camera_color);
            ImGui::ColorEdit3("##front_color", (float *)&color);
            emuenv.cfg.front_camera_color = ImGui::ColorConvertFloat4ToU32(color);
        } else if (front_camera == 1) {
            ImGui::Spacing();
            if (emuenv.cfg.front_camera_image.empty()) {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.camera["image_not_set"].c_str());
            } else {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.cfg.front_camera_image.c_str());
            }
            ImGui::Spacing();
            if (ImGui::Button((lang.camera["set_image"] + "##front_camera").c_str())) {
                fs::path camera_image_path = fs_utils::utf8_to_path(emuenv.cfg.front_camera_image);
                host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(camera_image_path);
                if (result == host::dialog::filesystem::SUCCESS && camera_image_path != fs_utils::utf8_to_path(emuenv.cfg.front_camera_image)) {
                    emuenv.cfg.front_camera_image = fs_utils::path_to_utf8(camera_image_path);
                }
                if (result == host::dialog::filesystem::ERROR) {
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                }
            }
        }
        if (old_front_camera_type != emuenv.cfg.front_camera_type || old_front_camera_id != emuenv.cfg.front_camera_id || old_front_camera_color != emuenv.cfg.front_camera_color || old_front_camera_image != emuenv.cfg.front_camera_image) {
            emuenv.camera.front()->update_config(emuenv.cfg.front_camera_type, emuenv.cfg.front_camera_id, emuenv.cfg.front_camera_image, emuenv.cfg.front_camera_color);
        }
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.camera["back_camera"].c_str());
        static int back_camera = -1;
        if (back_camera == -1) {
            back_camera = get_camera_combobox_index(emuenv.cfg.back_camera_type, emuenv.cfg.back_camera_id);
        }
        ImGui::Combo("##Back_camera", &back_camera, cameras.data(), cameras.size());
        auto old_back_camera_id = emuenv.cfg.back_camera_id;
        auto old_back_camera_type = emuenv.cfg.back_camera_type;
        auto old_back_camera_color = emuenv.cfg.back_camera_color;
        auto old_back_camera_image = emuenv.cfg.back_camera_image;
        emuenv.cfg.back_camera_type = back_camera;
        if (back_camera >= 2) {
            // Save selected camera name
            emuenv.cfg.back_camera_id = cameras[back_camera];
            emuenv.cfg.back_camera_type = 2;
        } else if (back_camera == 0) {
            // Color input field
            static ImVec4 color = ImGui::ColorConvertU32ToFloat4(emuenv.cfg.back_camera_color);
            ImGui::ColorEdit3("##back_color", (float *)&color);
            emuenv.cfg.back_camera_color = ImGui::ColorConvertFloat4ToU32(color);
        } else if (back_camera == 1) {
            ImGui::Spacing();
            if (emuenv.cfg.back_camera_image.empty()) {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.camera["image_not_set"].c_str());
            } else {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.cfg.back_camera_image.c_str());
            }
            ImGui::Spacing();
            if (ImGui::Button((lang.camera["set_image"] + "##back_camera").c_str())) {
                fs::path camera_image_path = fs_utils::utf8_to_path(emuenv.cfg.back_camera_image);
                host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(camera_image_path);
                if (result == host::dialog::filesystem::SUCCESS && camera_image_path != fs_utils::utf8_to_path(emuenv.cfg.back_camera_image)) {
                    emuenv.cfg.back_camera_image = fs_utils::path_to_utf8(camera_image_path);
                }
                if (result == host::dialog::filesystem::ERROR) {
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                }
            }
        }
        if (old_back_camera_type != emuenv.cfg.back_camera_type || old_back_camera_id != emuenv.cfg.back_camera_id || old_back_camera_color != emuenv.cfg.back_camera_color || old_back_camera_image != emuenv.cfg.back_camera_image) {
            emuenv.camera.back()->update_config(emuenv.cfg.back_camera_type, emuenv.cfg.back_camera_id, emuenv.cfg.back_camera_image, emuenv.cfg.back_camera_color);
        }
        break;
    }

    case SettingsDialogSection::System: {
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.system["select_enter_button"].c_str());
        SetTooltipEx(lang.system["enter_button_description"].c_str());
        ImGui::RadioButton(lang.system["circle"].c_str(), &emuenv.cfg.sys_button, 0);
        ImGui::RadioButton(lang.system["cross"].c_str(), &emuenv.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox(lang.system["pstv_mode"].c_str(), &config.pstv_mode);
        SetTooltipEx(lang.system["pstv_mode_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.system["show_mode"].c_str(), &emuenv.cfg.show_mode);
        SetTooltipEx(lang.system["show_mode_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.system["demo_mode"].c_str(), &emuenv.cfg.demo_mode);
        SetTooltipEx(lang.system["demo_mode_description"].c_str());

        break;
    }

    case SettingsDialogSection::Emulator: {
#ifndef __ANDROID__
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["boot_apps_full_screen"].c_str(), &emuenv.cfg.boot_apps_full_screen);
#endif
        ImGui::Spacing();

        const char *LIST_LOG_LEVEL[] = { lang.emulator["trace"].c_str(), gui.lang.main_menubar.debug["title"].c_str(), lang.emulator["info"].c_str(), lang.emulator["warning"].c_str(), lang.emulator["error"].c_str(), lang.emulator["critical"].c_str(), lang.emulator["off"].c_str() };
        if (ImGui::Combo(lang.emulator["log_level"].c_str(), &emuenv.cfg.log_level, LIST_LOG_LEVEL, IM_ARRAYSIZE(LIST_LOG_LEVEL)))
            logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
        SetTooltipEx(lang.emulator["select_log_level"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["archive_log"].c_str(), &emuenv.cfg.archive_log);
        SetTooltipEx(lang.emulator["archive_log_description"].c_str());
        ImGui::SameLine();
#ifdef USE_DISCORD
        ImGui::Checkbox("Discord Rich Presence", &emuenv.cfg.discord_rich_presence);
        SetTooltipEx(lang.emulator["discord_rich_presence"].c_str());
#endif
        ImGui::Checkbox(lang.emulator["texture_cache"].c_str(), &emuenv.cfg.texture_cache);
        SetTooltipEx(lang.emulator["texture_cache_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.emulator["show_compile_shaders"].c_str(), &emuenv.cfg.show_compile_shaders);
        SetTooltipEx(lang.emulator["compile_shaders_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["show_touchpad_cursor"].c_str(), &config.show_touchpad_cursor);
        SetTooltipEx(lang.emulator["touchpad_cursor_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.emulator["log_compat_warn"].c_str(), &emuenv.cfg.log_compat_warn);
        SetTooltipEx(lang.emulator["log_compat_warn_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["check_for_updates"].c_str(), &emuenv.cfg.check_for_updates);
        SetTooltipEx(lang.emulator["check_for_updates_description"].c_str());
        ImGui::Spacing();
        ImGui::SliderInt(lang.emulator["file_loading_delay"].c_str(), &config.file_loading_delay, 0, 30, "%d ms", ImGuiSliderFlags_AlwaysClamp);
        SetTooltipEx(lang.emulator["file_loading_delay_description"].c_str());
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["performance_overlay"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["performance_overlay"].c_str(), &emuenv.cfg.performance_overlay);
        SetTooltipEx(lang.emulator["performance_overlay_description"].c_str());
        if (emuenv.cfg.performance_overlay) {
            const char *LIST_OVERLAY_DETAIL[] = { lang.emulator["minimum"].c_str(), lang.emulator["low"].c_str(), lang.emulator["medium"].c_str(), lang.emulator["maximum"].c_str() };
            ImGui::Combo(lang.emulator["detail"].c_str(), &emuenv.cfg.performance_overlay_detail, LIST_OVERLAY_DETAIL, IM_ARRAYSIZE(LIST_OVERLAY_DETAIL));
            SetTooltipEx(lang.emulator["select_detail"].c_str());
            const char *LIST_OVERLAY_POSITION[] = { lang.emulator["top_left"].c_str(), lang.emulator["top_center"].c_str(), lang.emulator["top_right"].c_str(), lang.emulator["bottom_left"].c_str(), lang.emulator["bottom_center"].c_str(), lang.emulator["bottom_right"].c_str() };
            ImGui::Combo(lang.emulator["position"].c_str(), &emuenv.cfg.performance_overlay_position, LIST_OVERLAY_POSITION, IM_ARRAYSIZE(LIST_OVERLAY_POSITION));
            SetTooltipEx(lang.emulator["select_position"].c_str());
        }
        ImGui::Spacing();
#ifndef _WIN32
        ImGui::Checkbox(lang.emulator["case_insensitive"].c_str(), &emuenv.io.case_isens_find_enabled);
        SetTooltipEx(lang.emulator["case_insensitive_description"].c_str());
#endif
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["emu_storage_folder"].c_str());
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.emulator["current_emu_path"].c_str(), emuenv.vita_fs_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button(lang.emulator["change_emu_path"].c_str()))
            change_emulator_path(gui, emuenv);
        SetTooltipEx(lang.emulator["change_emu_path_description"].c_str());
        if (emuenv.vita_fs_path != emuenv.default_path) {
            ImGui::SameLine();
            if (ImGui::Button(lang.emulator["reset_emu_path"].c_str())) {
                if (emuenv.default_path != emuenv.vita_fs_path) {
                    app::switch_emulator_path(emuenv, emuenv.default_path);

                    // Refresh the working paths
                    reset_emulator(gui, emuenv);
                    LOG_INFO("Successfully restored default path for Vita3K files to: {}", emuenv.vita_fs_path);
                }
            }
            SetTooltipEx(lang.emulator["reset_emu_path_description"].c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["custom_config_settings"].c_str());
        ImGui::Spacing();
        if (ImGui::Button(lang.emulator["clear_custom_config"].c_str())) {
            if (fs::remove_all(emuenv.config_path / "config")) {
                LOG_INFO("Clear all custom config settings successfully.");
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["screenshot_image_type"].c_str());
        ImGui::Spacing();
        const char *LIST_IMG_FORMAT[] = { lang.emulator["null"].c_str(), "JPEG", "PNG" };
        ImGui::Combo(lang.emulator["screenshot_format"].c_str(), &emuenv.cfg.screenshot_format, LIST_IMG_FORMAT, IM_ARRAYSIZE(LIST_IMG_FORMAT));
        break;
    }

    case SettingsDialogSection::Gui: {
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["show_gui"].c_str(), &emuenv.cfg.show_gui);
        SetTooltipEx(lang.gui["gui_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["show_info_bar"].c_str(), &emuenv.cfg.show_info_bar);
        SetTooltipEx(lang.gui["info_bar_description"].c_str());
#if defined(HAS_QT) || defined(__ANDROID__)
        if (!is_custom_config) {
            ImGui::Spacing();
#ifdef __ANDROID__
            const char *LIST_GUI_BACKEND[] = { "ImGui", "Compose" };
            int gui_backend = emuenv.cfg.gui_backend == "Compose" ? 1 : 0;
#else
            const char *LIST_GUI_BACKEND[] = { "ImGui", "Qt" };
            int gui_backend = emuenv.cfg.gui_backend == "Qt" ? 1 : 0;
#endif
            if (ImGui::Combo(lang.gui["gui_backend"].c_str(), &gui_backend, LIST_GUI_BACKEND, IM_ARRAYSIZE(LIST_GUI_BACKEND)))
                emuenv.cfg.gui_backend = LIST_GUI_BACKEND[gui_backend];
            SetTooltipEx(lang.gui["gui_backend_description"].c_str());
        }
#endif
        ImGui::Spacing();
        const std::string system_lang_name = fmt::format("{}: {}", lang.system["title"], get_sys_lang_name(emuenv.cfg.sys_lang));
        std::vector<const char *> list_user_lang_str{ system_lang_name.c_str() };
        static std::map<std::string, std::string> static_list_user_lang_names = {
            { "id", "Indonesia" },
            { "ms", "Malaysia" },
            { "ua", reinterpret_cast<const char *>(u8"Українська") },
        };
        for (const auto &l : list_user_lang)
            list_user_lang_str.push_back(static_list_user_lang_names.contains(l) ? static_list_user_lang_names[l].c_str() : l.c_str());
        if (ImGui::Combo(lang.gui["user_lang"].c_str(), &current_user_lang, list_user_lang_str.data(), static_cast<int>(list_user_lang_str.size()), 4)) {
            if (current_user_lang != 0)
                emuenv.cfg.user_lang = list_user_lang[current_user_lang - 1];
            else
                emuenv.cfg.user_lang.clear();

            lang::init_lang(gui.lang, emuenv);
        }
        SetTooltipEx(lang.gui["select_user_lang"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["display_info_message"].c_str(), &emuenv.cfg.display_info_message);
        SetTooltipEx(lang.gui["display_info_message_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["display_system_apps"].c_str(), &emuenv.cfg.display_system_apps);
        SetTooltipEx(lang.gui["display_system_apps_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["show_live_area_screen"].c_str(), &emuenv.cfg.show_live_area_screen);
        SetTooltipEx(lang.gui["live_area_screen_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["stretch_the_display_area"].c_str(), &config.stretch_the_display_area);
        SetTooltipEx(lang.gui["stretch_the_display_area_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["apps_list_grid"].c_str(), &emuenv.cfg.apps_list_grid);
        SetTooltipEx(lang.gui["apps_list_grid_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["fullscreen_hd_res_pixel_perfect"].c_str(), &config.fullscreen_hd_res_pixel_perfect);
        SetTooltipEx(lang.gui["fullscreen_hd_res_pixel_perfect_description"].c_str());
        if (!emuenv.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt(lang.gui["icon_size"].c_str(), &emuenv.cfg.icon_size, 64, 128);
            SetTooltipEx(lang.gui["select_icon_size"].c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang.gui["font_support"].c_str());
        ImGui::Spacing();
        if (gui.fw_font) {
            ImGui::Checkbox(lang.gui["asia_font_support"].c_str(), &emuenv.cfg.asia_font_support);
            SetTooltipEx(lang.gui["asia_font_support_description"].c_str());
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", firmware_font["no_font_exist"].c_str());
            if (ImGui::Button(gui.lang.welcome["download_firmware_font_package"].c_str()))
                open_path("https://bit.ly/2P2rb0r");
            SetTooltipEx(lang.gui["firmware_font_package_description"].c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang.gui["theme_background"].c_str());
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.gui["current_theme_content_id"].c_str(), user.theme_id.c_str());
        if (user.theme_id != "default") {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["reset_default_theme"].c_str())) {
                user.theme_id = "default";
                user.use_theme_bg = false;
                if (init_theme(gui, emuenv, "default"))
                    user.use_theme_bg = true;
                user.start_path.clear();
                user.start_type = "default";
                app::save_user(emuenv, emuenv.io.user_id);
                init_theme_start_background(gui, emuenv, "default");
                init_apps_icon(gui, emuenv, gui.app_selector.sys_apps);
            }
            ImGui::SameLine();
        }
        if (!gui.theme_backgrounds.empty())
            if (ImGui::Checkbox(lang.gui["using_theme_background"].c_str(), &user.use_theme_bg))
                app::save_user(emuenv, emuenv.io.user_id);

        if (!gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["clean_user_backgrounds"].c_str())) {
                gui.user_backgrounds[user.backgrounds[gui.current_user_bg]] = {};
                gui.user_backgrounds.clear();
                if (!gui.theme_backgrounds.empty())
                    user.use_theme_bg = true;
                user.backgrounds.clear();
                app::save_user(emuenv, emuenv.io.user_id);
            }
        }
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.gui["current_start_background"].c_str(), user.start_type.c_str());
        if (((user.theme_id == "default") && (user.start_type != "default")) || ((user.theme_id != "default") && (user.start_type != "theme"))) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["reset_start_background"].c_str())) {
                user.start_path.clear();
                init_theme_start_background(gui, emuenv, user.theme_id);
                user.start_type = (user.theme_id == "default") ? "default" : "theme";
                app::save_user(emuenv, emuenv.io.user_id);
            }
        }
        if (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            ImGui::SliderFloat(lang.gui["background_alpha"].c_str(), &emuenv.cfg.background_alpha, 0.999f, 0.000f);
            SetTooltipEx(lang.gui["select_background_alpha"].c_str());
        }
        if (!gui.theme_backgrounds.empty() || (gui.user_backgrounds.size() > 1)) {
            ImGui::Spacing();
            ImGui::SliderInt(lang.gui["delay_background"].c_str(), &emuenv.cfg.delay_background, 4, 60);
            SetTooltipEx(lang.gui["select_delay_background"].c_str());
        }
        ImGui::Spacing();
        ImGui::SliderInt(lang.gui["delay_start"].c_str(), &emuenv.cfg.delay_start, 30, 300);
        SetTooltipEx(lang.gui["select_delay_start"].c_str());
        break;
    }

    case SettingsDialogSection::Network: {
        ImGui::Spacing();

        // PSN
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "PlayStation Network");
        ImGui::Spacing();
        ImGui::Checkbox(lang.network["psn_signed_in"].c_str(), &config.psn_signed_in);
        SetTooltipEx(lang.network["psn_signed_in_description"].c_str());

        // Adhoc
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "Adhoc");
        ImGui::Spacing();

        const auto addrs = net_utils::get_all_assigned_addrs();
        std::vector<std::string> addrsStrings;
        std::vector<const char *> addrsSelect;
        std::vector<const char *> nMaskSelect;
        addrsStrings.reserve(addrs.size());
        addrsSelect.reserve(addrs.size());
        nMaskSelect.reserve(addrs.size());

        for (const auto &addr : addrs) {
            addrsStrings.emplace_back(fmt::format("{} ({})", addr.addr, addr.name));
            addrsSelect.emplace_back(addrsStrings.back().c_str());
            nMaskSelect.emplace_back(addr.netMask.c_str());
        }

        if (emuenv.cfg.adhoc_addr >= addrs.size()) {
            emuenv.cfg.adhoc_addr = 0;
            LOG_WARN("Invalid adhoc address index {}, resetting to 0", emuenv.cfg.adhoc_addr);
            save_config(gui, emuenv);
        }

        ImGui::PushItemWidth(ImGui::CalcTextSize(addrsStrings[emuenv.cfg.adhoc_addr].c_str()).x + (30.f * SCALE.x));
        ImGui::Combo(lang.network["ip_address"].c_str(), &emuenv.cfg.adhoc_addr, addrsSelect.data(), static_cast<int>(addrsSelect.size()));
        SetTooltipEx(lang.network["ip_address_description"].c_str());

        ImGui::BeginDisabled();
        ImGui::Combo(lang.network["subnet_mask"].c_str(), &emuenv.cfg.adhoc_addr, nMaskSelect.data(), static_cast<int>(nMaskSelect.size()));
        ImGui::EndDisabled();
        ImGui::PopItemWidth();

        // HTTP
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "HTTP");
        ImGui::Spacing();
        ImGui::Checkbox(lang.network["enable_http"].c_str(), &emuenv.cfg.http_enable);
        SetTooltipEx(lang.network["enable_http_description"].c_str());
        ImGui::Spacing();
        ImGui::SliderInt(lang.network["timeout_attempts"].c_str(), &emuenv.cfg.http_timeout_attempts, 0, 100);
        SetTooltipEx(lang.network["timeout_attempts_description"].c_str());
        ImGui::SliderInt(lang.network["timeout_sleep"].c_str(), &emuenv.cfg.http_timeout_sleep_ms, 50, 3000);
        SetTooltipEx(lang.network["timeout_sleep_description"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SliderInt(lang.network["read_end_attempts"].c_str(), &emuenv.cfg.http_read_end_attempts, 0, 100);
        SetTooltipEx(lang.network["read_end_attempts_description"].c_str());
        ImGui::SliderInt(lang.network["read_end_sleep"].c_str(), &emuenv.cfg.http_read_end_sleep_ms, 50, 3000);
        SetTooltipEx(lang.network["read_end_sleep_description"].c_str());
        break;
    }

    case SettingsDialogSection::Debug: {
        ImGui::Spacing();
        ImGui::Checkbox(lang.debug["log_imports"].c_str(), &emuenv.kernel.debugger.log_imports);
        ImGui::SameLine();
        SetTooltipEx(lang.debug["log_imports_description"].c_str());
        ImGui::Checkbox(lang.debug["log_exports"].c_str(), &emuenv.kernel.debugger.log_exports);
        SetTooltipEx(lang.debug["log_exports_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.debug["log_active_shaders"].c_str(), &emuenv.cfg.log_active_shaders);
        SetTooltipEx(lang.debug["log_active_shaders_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.debug["log_uniforms"].c_str(), &emuenv.cfg.log_uniforms);
        ImGui::SameLine();
        SetTooltipEx(lang.debug["log_uniforms_description"].c_str());
        ImGui::Checkbox(lang.debug["color_surface_debug"].c_str(), &emuenv.cfg.color_surface_debug);
        SetTooltipEx(lang.debug["color_surface_debug_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.debug["dump_elfs"].c_str(), &emuenv.kernel.debugger.dump_elfs);
        SetTooltipEx(lang.debug["dump_elfs_description"].c_str());
        if (emuenv.backend_renderer == renderer::Backend::Vulkan) {
            ImGui::Spacing();
            ImGui::Checkbox(lang.debug["validation_layer"].c_str(), &emuenv.cfg.validation_layer);
            ImGui::SameLine();
            SetTooltipEx(lang.debug["validation_layer_description"].c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button(emuenv.kernel.debugger.watch_code ? lang.debug["unwatch_code"].c_str() : lang.debug["watch_code"].c_str())) {
            emuenv.kernel.debugger.watch_code = !emuenv.kernel.debugger.watch_code;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(emuenv.kernel.debugger.watch_memory ? lang.debug["unwatch_memory"].c_str() : lang.debug["watch_memory"].c_str())) {
            emuenv.kernel.debugger.watch_memory = !emuenv.kernel.debugger.watch_memory;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(emuenv.kernel.debugger.watch_import_calls ? lang.debug["unwatch_import_calls"].c_str() : lang.debug["watch_import_calls"].c_str())) {
            emuenv.kernel.debugger.watch_import_calls = !emuenv.kernel.debugger.watch_import_calls;
            emuenv.kernel.debugger.update_watches();
        }

#ifdef TRACY_ENABLE
        // Tracy profiler settings
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Tracy Profiler");

        ImGui::Text("The Tracy profiler implementation in the emulator allows among other\n"
                    "things to track the functions that a game calls in real-time\n"
                    "and visualize them in a timeline with timings for every frame and audio buffer.");

        // Primitive Tracy implementation
        ImGui::Checkbox("Primitive implementation", &emuenv.cfg.tracy_primitive_impl);
        SetTooltipEx("The primitive Tracy implementation for HLE modules allows for\n"
                     "all HLE module calls to be logged without manual instrumentation needed.\n"
                     "However it is just a general workaround that doesn't count for statistic\n"
                     "analysis neither for trace searching on Tracy.\n\n"
                     "Due to the amount of functions being logged due to this implementation\n"
                     "Tracy logs can become gigabytes long in a matter of minutes. Because of this\n"
                     "it is only recommended to be used when the module(s) to debug aren't available for\n"
                     "advanced profiling or a more general overview of the function calls is needed and\n"
                     "in a PC with at least 12GB (Linux) or 16GB (Windows) of RAM.");

        // ImGui::Text("The Tracy profiler is not available in Release builds, please compile Vita3K\nfrom source using"
        // " either the RelWithDebInfo or Debug builds in order to use it.");

        // Text to display along the modules list
        const char *tracy_modules_list_label = "Available modules for advanced profiling\n\n"
                                               "Modules enabled for advanced profiling don't\n"
                                               "only provide function call timings but\n"
                                               "also log the arguments they were called\n"
                                               "with for every single function call\n"
                                               "except arguments driving a large amount\n"
                                               "of data such as large sized arrays.\n\n"
                                               "Advanced profiling requires functions to\n"
                                               "be manually instrumented in source code.";

        // Tracy modules list
        static std::vector<std::string> tracy_modules = tracy_module_utils::get_available_module_names();
        if (ImGui::BeginListBox(tracy_modules_list_label, { CONTENT_SIZE.x / 2.f, ImGui::GetTextLineHeightWithSpacing() * 8.25f + ImGui::GetStyle().FramePadding.y * 2.0f })) {
            // Get all HLE modules available for advanced profiling using Tracy. Do it only once.
            // For every HLE module available for advanced profiling using Tracy
            for (auto &module : tracy_modules) {
                bool activation_state = tracy_module_utils::is_tracy_active(module);
                // Create selectable item using module name and get activation state
                if (ImGui::Selectable(module.c_str(), activation_state)) {
                    // Change activation state if module is clicked/selected
                    if (activation_state) {
                        // Deactivate module
                        tracy_module_utils::set_tracy_active(module, false);
                        // Update config data by deleting the name of the module from the vector
                        std::erase(emuenv.cfg.tracy_advanced_profiling_modules, module);
                    } else {
                        // Activate module
                        tracy_module_utils::set_tracy_active(module, true);
                        // Update config data by appending the name of the module to the vector
                        emuenv.cfg.tracy_advanced_profiling_modules.push_back(module);
                    }
                }
            }
            ImGui::ScrollWhenDragging();
            ImGui::EndListBox();
        }
        // Calculate checkbox state based on Tracy modules selection
        int selected_count = 0;
        int visible_count = static_cast<int>(tracy_modules.size());
        for (const auto &module : tracy_modules) {
            if (tracy_module_utils::is_tracy_active(module))
                selected_count++;
        }

        bool all_selected = selected_count == visible_count;
        bool partial_selected = (selected_count > 0) && (selected_count < visible_count);

        // Set checkbox appearance based on state (indeterminate when partially selected)
        if (partial_selected) {
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        }

        static bool selection_state = all_selected || partial_selected;
        if (ImGui::Checkbox("Select all##tracy", &selection_state)) {
            // Toggle all tracy modules
            for (const auto &module : tracy_modules) {
                tracy_module_utils::set_tracy_active(module, selection_state);
                const bool module_existed = vector_utils::contains(emuenv.cfg.tracy_advanced_profiling_modules, module);
                if (selection_state && !module_existed) {
                    emuenv.cfg.tracy_advanced_profiling_modules.push_back(module);
                } else if (!selection_state && module_existed) {
                    std::erase(emuenv.cfg.tracy_advanced_profiling_modules, module);
                }
            }
        }

        if (partial_selected) {
            ImGui::PopItemFlag();
        }
#endif // TRACY_ENABLE

        break;
    }
    default:
        break;
    }

    ImGui::ScrollWhenDragging();
    ImGui::EndChild();
    ImGui::SetCursorPosY(WINDOW_SIZE.y - TITLE_BAR_HEIGHT);
    ImGui::Separator();
    const ImVec2 FOOTER_BG_MIN(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT + WINDOW_SIZE.y - TITLE_BAR_HEIGHT + 1.f);
    const ImVec2 FOOTER_BG_MAX(FOOTER_BG_MIN.x + WINDOW_SIZE.x, FOOTER_BG_MIN.y + TITLE_BAR_HEIGHT);
    draw_list->AddRectFilled(FOOTER_BG_MIN, FOOTER_BG_MAX, IM_COL32(11, 131, 0, 150.f));
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x), WINDOW_SIZE.y - ((TITLE_BAR_HEIGHT + BUTTON_SIZE.y) / 2.f)));
    ImGui::SetWindowFontScale(1.f * RES_SCALE.y);

    const auto close_settings_dialog = [&]() {
        const auto is_app_running = !emuenv.io.app_path.empty() && !emuenv.kernel.is_threads_paused();
        show_settings_dialog = false;
        if (!is_app_running)
            gui.vita_area.home_screen = true;
        if (!emuenv.cfg.show_info_bar || is_app_running)
            gui.vita_area.information_bar = false;
    };
    if (ImGui::Button(common.common["close"].c_str(), BUTTON_SIZE))
        close_settings_dialog();
    ImGui::SameLine(0, 40.f * SCALE.x);
    const auto is_apply = !emuenv.io.app_path.empty() && (!is_custom_config || (emuenv.app_path == emuenv.io.app_path));
    const auto settings_affect_running_game = is_apply
        && (is_custom_config || !config::has_custom_config(emuenv.config_path, emuenv.io.app_path));
    const auto restart_required_settings = config::get_restart_required_settings(emuenv.cfg.current_config, config);
    const auto emulator_restart_required_settings = get_emulator_restart_required_settings(emuenv.cfg.current_config, config);
    const auto is_emulator_restart = settings_affect_running_game && !emulator_restart_required_settings.empty();
    const auto is_game_restart = settings_affect_running_game
        && !restart_required_settings.empty()
        && !is_emulator_restart;
    const char *button_label = lang.main_window["save_close"].c_str();
    if (is_apply) {
        if (is_emulator_restart)
            button_label = lang.main_window["save_emulator_restart"].c_str();
        else if (is_game_restart)
            button_label = lang.main_window["save_reboot"].c_str();
        else
            button_label = lang.main_window["save_apply"].c_str();
    }
    if (ImGui::Button(button_label, BUTTON_SIZE)) {
        const auto result = save_config(gui, emuenv);
        if (result.affected_running_game && !result.restart_required_settings.empty())
            app::request_in_process_launch(emuenv, AppLaunchRequest{
                                                       .app_path = emuenv.io.app_path,
                                                       .self_path = emuenv.self_path,
                                                       .argv = emuenv.cfg.app_args.empty() ? std::vector<std::string>{} : std::vector<std::string>{ emuenv.cfg.app_args },
                                                       .reason = AppLaunchReason::LoadExec,
                                                   });

        close_settings_dialog();
    }
    SetTooltipEx(lang.main_window["keep_changes"].c_str());
    ImGui::PopStyleColor(8);
    ImGui::PopStyleVar(3);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
