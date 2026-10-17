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
#include <bgm_player/functions.h>

#include <config/functions.h>
#include <config/settings.h>
#include <config/state.h>

#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui-imgui/functions.h>
#include <host/dialog/filesystem.h>
#include <ime/functions.h>
#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <limits>
#include <numeric>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <vita_shell/functions.h>

#include <stb_image.h>
#include <util/vector_utils.h>

#undef ERROR

namespace gui {

static std::map<std::string, vita_shell::ThemeInfo> themes_info;
static std::vector<std::pair<std::string, time_t>> themes_list;
static std::string current_selected_theme, selected, first_visible_theme;
static float set_scroll_pos = -1.f, current_scroll_pos, max_scroll_pos;

enum Menu { UNDEFINED = -1,
    THEME,
    START,
    BACKGROUND,
    DATE_FORMAT,
    TIME_FORMAT,
    SELECT_INPUT_LANG };
enum SettingsMenu { SELECT = -1,
    SOUND_DISPLAY,
    THEME_BACKGROUND,
    DATE_TIME,
    LANGUAGE };
enum class Popup { UNDEFINED,
    DEL,
    INFORMATION,
    SELECT_SYS_LANG };
enum class SubMenu { UNDEFINED,
    THEME,
    IMAGE,
    DEFAULT,
    SELECT_KEYBOARDS };

static Menu menu = Menu::UNDEFINED;
static SettingsMenu settings_menu = SettingsMenu::SELECT;
static Popup popup = Popup::UNDEFINED;
static SubMenu sub_menu = SubMenu::UNDEFINED;
static int32_t current_menu = 0;
static uint32_t current_menu_size = 4;

struct ThemeWithPosition {
    std::string theme_id;
    uint32_t line;
    uint32_t column;
};
static std::vector<ThemeWithPosition> themes_list_filtered;

ThemePreviewLoader::ThemePreviewLoader(const fs::path &vita_fs_path, IOState &io, std::vector<ThemePreviewRequest> requests) {
    thread = std::thread([this, vita_fs_path, &io, requests = std::move(requests)] {
        try {
            for (const auto &request : requests) {
                if (quit)
                    return;
                const auto &name = request.path;
                if (name.empty())
                    continue;
                const auto directory = vita_fs_path / "ux0/theme" / fs_utils::utf8_to_path(request.content_id);
                vita_shell::ThemeReader reader(directory, request.content_id != "default", &io);
                vfs::FileBuffer buffer;
                const bool read = request.content_id == "default"
                    ? vfs::read_file(VitaIoDevice::vs0, buffer, vita_fs_path, name)
                    : reader.read(fs_utils::utf8_to_path(name), buffer);
                if (!read || buffer.empty() || buffer.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                    LOG_WARN("Cannot read theme thumbnail '{}' for '{}'.", name, request.content_id);
                    continue;
                }
                Result result{ request.content_id, {} };
                result.image.data.reset(stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()),
                    &result.image.width, &result.image.height, nullptr, STBI_rgb_alpha));
                if (!result.image.data) {
                    LOG_ERROR("Invalid theme thumbnail '{}' for '{}'.", name, request.content_id);
                    continue;
                }
                std::unique_lock lock(mutex);
                changed.wait(lock, [this] { return quit || ready.size() < 8; });
                if (quit)
                    return;
                ready.push_back(std::move(result));
            }
        } catch (const std::exception &error) {
            LOG_ERROR("Cannot load theme previews: {}", error.what());
        }
    });
}

ThemePreviewLoader::~ThemePreviewLoader() {
    {
        std::lock_guard lock(mutex);
        quit = true;
    }
    changed.notify_all();
    if (thread.joinable())
        thread.join();
}

void ThemePreviewLoader::commit(GuiState &gui) {
    std::vector<Result> results;
    {
        std::lock_guard lock(mutex);
        const auto count = std::min<size_t>(ready.size(), 2);
        for (size_t i = 0; i < count; ++i)
            results.push_back(std::move(ready[i]));
        ready.erase(ready.begin(), ready.begin() + count);
    }
    changed.notify_one();
    for (const auto &result : results)
        gui.themes_package[result.content_id] = ImGui_Texture(gui.imgui_state.get(), result.image.data.get(), result.image.width, result.image.height);
}

static void start_theme_previews(GuiState &gui, EmuEnvState &emuenv) {
    std::vector<ThemePreviewRequest> requests;
    requests.reserve(themes_list.size());
    for (const auto &[id, updated] : themes_list)
        requests.push_back({ id, themes_info.at(id).preview.at(vita_shell::ThemeImage::Package) });
    std::stable_partition(requests.begin(), requests.end(), [](const auto &request) { return request.content_id == "default"; });
    gui.theme_preview_loader.emplace(emuenv.vita_fs_path, emuenv.io, std::move(requests));
}

static void init_theme_preview(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    gui.theme_preview.clear();
    gui.theme_preview_id = id;
    const auto found = themes_info.find(id);
    if (found == themes_info.end())
        return;
    vita_shell::ThemeReader reader(emuenv.vita_fs_path / "ux0/theme" / fs_utils::utf8_to_path(id), id != "default", &emuenv.io);
    for (const auto &[image, type] : { std::pair{ vita_shell::ThemeImage::Home, HOME }, std::pair{ vita_shell::ThemeImage::Lock, LOCK } }) {
        const auto path = found->second.preview.find(image);
        if (path == found->second.preview.end() || path->second.empty())
            continue;
        const auto &name = path->second;
        vfs::FileBuffer buffer;
        const bool read = id == "default" ? vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.vita_fs_path, name)
                                          : reader.read(fs_utils::utf8_to_path(name), buffer);
        if (!read || buffer.empty() || buffer.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            LOG_WARN("Cannot read theme preview '{}' for '{}'.", name, id);
            continue;
        }
        IconData image_data;
        image_data.data.reset(stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &image_data.width, &image_data.height, nullptr, STBI_rgb_alpha));
        if (!image_data.data) {
            LOG_ERROR("Invalid theme preview '{}' for '{}'.", name, id);
            continue;
        }
        gui.theme_preview[type] = ImGui_Texture(gui.imgui_state.get(), image_data.data.get(), image_data.width, image_data.height);
    }
}

static void get_themes_list(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.theme_catalog_loader)
        return;
    gui.theme_catalog_error.clear();
    const auto path = emuenv.vita_fs_path;
    const auto language = emuenv.cfg.sys_lang;
    const auto default_title = gui.lang.settings.theme_background.main["default"];
    auto &io = emuenv.io;
    gui.theme_catalog_loader.emplace(std::async(std::launch::async, [path, language, default_title, &io] {
        return vita_shell::build_theme_catalog(path, language, default_title, io);
    }));
}

static void commit_themes_list(GuiState &gui, EmuEnvState &emuenv) {
    if (!gui.theme_catalog_loader || gui.theme_catalog_loader->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        auto catalog = gui.theme_catalog_loader->get();
        if (!catalog) {
            gui.theme_preview_loader.reset();
            gui.themes_package.clear();
            gui.theme_preview.clear();
            gui.theme_preview_id.clear();
            themes_info.clear();
            themes_list.clear();
            gui.theme_catalog_error = "Cannot load theme list; see log for details.";
            gui.theme_catalog_loader.reset();
            return;
        }
        gui.theme_preview_loader.reset();
        gui.themes_package.clear();
        gui.theme_preview.clear();
        gui.theme_preview_id.clear();
        themes_info = std::move(catalog->info);
        themes_list = std::move(catalog->list);
        start_theme_previews(gui, emuenv);
    } catch (const std::exception &error) {
        gui.theme_preview_loader.reset();
        gui.themes_package.clear();
        gui.theme_preview.clear();
        gui.theme_preview_id.clear();
        themes_info.clear();
        themes_list.clear();
        gui.theme_catalog_error = error.what();
        LOG_ERROR("Cannot load theme list: {}", error.what());
    }
    gui.theme_catalog_loader.reset();
}

static void set_theme(GuiState &gui, EmuEnvState &emuenv) {
    auto &user = emuenv.app.user_list.users[emuenv.io.user_id];
    if (!apply_theme(gui, emuenv, selected))
        return;
    user.start_path.clear();
    user.theme_id = selected;
    user.use_theme_bg = !gui.theme_backgrounds.empty();
    init_theme_start_background(gui, emuenv, selected);
    user.start_type = (selected == "default") ? "default" : "theme";
    app::save_user(emuenv, emuenv.io.user_id);
    selected.clear();
    gui.theme_preview.clear();
    gui.theme_preview_id.clear();
    set_scroll_pos = current_scroll_pos;
}

static void handle_seting_menu_selection(GuiState &gui, EmuEnvState &emuenv) {
    switch (settings_menu) {
    case SettingsMenu::SELECT:
        if (current_menu == SettingsMenu::THEME_BACKGROUND)
            get_themes_list(gui, emuenv);
        settings_menu = static_cast<SettingsMenu>(current_menu);
        current_menu_size = 3;
        break;
    case SettingsMenu::THEME_BACKGROUND:
        menu = static_cast<Menu>(current_menu);
        if (menu == Menu::THEME && current_selected_theme.empty() && !themes_list.empty())
            current_selected_theme = themes_list.front().first;
        break;
    default:
        break;
    }
    current_menu = 0;
}

static void back_settings(GuiState &gui, EmuEnvState &emuenv) {
    if (settings_menu == SettingsMenu::SELECT) {
        close_system_app(gui, emuenv);
    } else if (popup != Popup::UNDEFINED) {
        popup = Popup::UNDEFINED;
    } else if (!selected.empty()) {
        selected.clear();
        gui.theme_preview.clear();
        gui.theme_preview_id.clear();
        set_scroll_pos = current_scroll_pos;
    } else if (sub_menu != SubMenu::UNDEFINED) {
        sub_menu = SubMenu::UNDEFINED;
        gui.theme_preview.clear();
        gui.theme_preview_id.clear();
    } else if (menu != Menu::UNDEFINED) {
        menu = Menu::UNDEFINED;
        current_menu = 0;
        current_menu_size = 3;
    } else {
        settings_menu = SettingsMenu::SELECT;
        current_menu = 0;
        current_menu_size = 4;
    }
}

void browse_settings(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    if (!gui.is_nav_button) {
        gui.is_nav_button = true;
        return;
    }

    if (button == (emuenv.cfg.sys_button == 1 ? SCE_CTRL_CIRCLE : SCE_CTRL_CROSS)) {
        back_settings(gui, emuenv);
        return;
    }

    const auto browse_menu = [&]() {
        if (current_menu == -1) {
            current_menu = 0;
            return;
        }
        switch (button) {
        case SCE_CTRL_UP:
            if (current_menu > 0)
                current_menu = (current_menu - 1) % current_menu_size;
            break;
        case SCE_CTRL_DOWN:
            if (current_menu < static_cast<int32_t>(current_menu_size) - 1)
                current_menu = (current_menu + 1) % current_menu_size;
            break;
        case SCE_CTRL_CIRCLE:
        case SCE_CTRL_CROSS:
            if (button == (emuenv.cfg.sys_button == 1 ? SCE_CTRL_CROSS : SCE_CTRL_CIRCLE))
                handle_seting_menu_selection(gui, emuenv);
            break;
        default: break;
        }
    };

    if (settings_menu == SettingsMenu::SELECT || (settings_menu == SettingsMenu::THEME_BACKGROUND && menu == Menu::UNDEFINED)) {
        browse_menu();
    } else if (settings_menu == SettingsMenu::THEME_BACKGROUND && menu == Menu::THEME && popup == Popup::UNDEFINED) {
        if (gui.theme_catalog_loader || !gui.theme_catalog_error.empty() || themes_list_filtered.empty())
            return;
        if (current_selected_theme.empty()) {
            current_selected_theme = first_visible_theme;
            return;
        }
        const auto it = std::find_if(themes_list_filtered.begin(), themes_list_filtered.end(), [&](const ThemeWithPosition &theme) {
            return theme.theme_id == current_selected_theme;
        });
        const int32_t index = it != themes_list_filtered.end() ? static_cast<int32_t>(std::distance(themes_list_filtered.begin(), it)) : -1;
        if (index == -1) {
            current_selected_theme = first_visible_theme;
            return;
        }
        const int32_t last = static_cast<int32_t>(themes_list_filtered.size() - 1);
        const auto column = themes_list_filtered[index].column;
        const auto confirm = [&]() {
            if (selected.empty()) {
                selected = current_selected_theme;
                set_scroll_pos = current_scroll_pos;
                init_theme_preview(gui, emuenv, selected);
            } else if (selected != emuenv.app.user_list.users[emuenv.io.user_id].theme_id || !vita_shell::get_theme_state().loaded || vita_shell::get_theme_state().content_id != selected)
                set_theme(gui, emuenv);
        };
        switch (button) {
        case SCE_CTRL_UP:
            if (selected.empty() && index >= 3)
                current_selected_theme = themes_list_filtered[index - 3].theme_id;
            break;
        case SCE_CTRL_RIGHT:
            if (selected.empty() && column < 2)
                current_selected_theme = themes_list_filtered[std::min(index + 1, last)].theme_id;
            break;
        case SCE_CTRL_DOWN:
            if (selected.empty() && index + 3 <= last)
                current_selected_theme = themes_list_filtered[index + 3].theme_id;
            break;
        case SCE_CTRL_LEFT:
            if (selected.empty() && index > 0 && column > 0)
                current_selected_theme = themes_list_filtered[index - 1].theme_id;
            break;
        case SCE_CTRL_CIRCLE:
            if (emuenv.cfg.sys_button == 0)
                confirm();
            break;
        case SCE_CTRL_CROSS:
            if (emuenv.cfg.sys_button == 1)
                confirm();
            break;
        default: break;
        }
    }
}

void draw_settings(GuiState &gui, EmuEnvState &emuenv) {
    commit_themes_list(gui, emuenv);
    if (gui.theme_preview_loader)
        gui.theme_preview_loader->commit(gui);
    auto &user = emuenv.app.user_list.users[emuenv.io.user_id];
    static std::string title, delete_user_background, delete_theme;
    static ImGuiTextFilter search_bar;

    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);
    const auto SIZE_PREVIEW = ImVec2(360.f * SCALE.x, 204.f * SCALE.y);
    const auto SIZE_MINI_PREVIEW = ImVec2(256.f * SCALE.x, 145.f * SCALE.y);
    const auto SIZE_LIST = ImVec2(780 * SCALE.x, 448.f * SCALE.y);
    const auto SIZE_PACKAGE = ImVec2(226.f * SCALE.x, 128.f * SCALE.y);
    const auto SIZE_MINI_PACKAGE = ImVec2(170.f * SCALE.x, 96.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto is_background = gui.apps_background.contains("NPXS10015");
    auto &common = gui.lang.common.main;

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    auto flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::Begin("##settings", &gui.vita_area.settings, flags);
    ImGui::PopStyleVar();

    const auto draw_list = ImGui::GetWindowDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (is_background)
        draw_list->AddImage(gui.apps_background["NPXS10015"], VIEWPORT_POS, BG_POS_MAX);
    else
        draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    const auto title_size_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
    ImGui::PushTextWrapPos(((WINDOW_SIZE.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (title_size_str.x / 2.f), (35.f * SCALE.y) - (title_size_str.y / 2.f)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
    ImGui::PopTextWrapPos();

    auto &lang = gui.lang.settings;
    auto &theme_background = lang.theme_background;
    auto &theme = theme_background.theme;
    auto &sound_display = lang.sound_display;
    auto &date_time = lang.date_time;
    auto &language = lang.language;

    static bool is_scroll_animating = false;
    static float scroll_y, target_scroll_y;

    if (settings_menu == SettingsMenu::THEME_BACKGROUND) {
        // Search Bar
        if ((menu == Menu::THEME) && selected.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto search_size = ImGui::CalcTextSize(common["search"].c_str());
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (220.f * SCALE.x) - search_size.x, (35.f * SCALE.y) - (search_size.y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", common["search"].c_str());
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200 * SCALE.x);
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);

            // Draw Scroll Arrow
            const auto ARROW_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);
            const ImU32 ARROW_COLOR = 0xFFFFFFFF; // White

            // Set arrow position of width
            const auto ARROW_WIDTH_POS = VIEWPORT_SIZE.x - (45.f * SCALE.x);
            const auto ARROW_DRAW_WIDTH_POS = VIEWPORT_POS.x + ARROW_WIDTH_POS;
            const auto ARROW_SELECT_WIDTH_POS = ARROW_WIDTH_POS - (ARROW_SIZE.x / 2.f);

            if (current_scroll_pos != 0.f) {
                const auto ARROW_UP_HEIGHT_POS = 135.f * SCALE.y;
                const ImVec2 ARROW_UPP_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_UP_HEIGHT_POS);
                draw_list->AddTriangleFilled(
                    ImVec2(ARROW_UPP_CENTER.x - (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x, ARROW_UPP_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x + (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_UP_HEIGHT_POS - ARROW_SIZE.y));
                if ((ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || (!ImGui::GetIO().WantTextInput && ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_leftstick_up))) {
                    target_scroll_y = current_scroll_pos - (340 * SCALE.y);
                    is_scroll_animating = true;
                    current_selected_theme.clear();
                }
            }
            if (current_scroll_pos < max_scroll_pos) {
                const auto ARROW_DOWN_HEIGHT_POS = VIEWPORT_SIZE.y - (30.f * SCALE.y);
                const ImVec2 ARROW_DOWN_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_DOWN_HEIGHT_POS);
                draw_list->AddTriangleFilled(
                    ImVec2(ARROW_DOWN_CENTER.x + (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x - (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_DOWN_HEIGHT_POS - ARROW_SIZE.y));
                if ((ImGui::Selectable("##down", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || (!ImGui::GetIO().WantTextInput && ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_leftstick_down))) {
                    target_scroll_y = current_scroll_pos + (340 * SCALE.y);
                    is_scroll_animating = true;
                    current_selected_theme.clear();
                }
            }
        }
    }

    ImGui::SetCursorPosY(64.0f * SCALE.y);
    ImGui::Separator();
    const auto MARGIN_HEADER = ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 POS_LIST(WINDOW_POS.x + (WINDOW_SIZE.x / 2.f) - (SIZE_LIST.x / 2.f), WINDOW_POS.y + MARGIN_HEADER);
    ImGui::SetNextWindowPos(POS_LIST, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    auto child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        child_flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::BeginChild("##settings_child", SIZE_LIST, ImGuiChildFlags_None, child_flags);

    const auto SIZE_SELECT = 80.f * SCALE.y;
    const auto SIZE_PUPUP_SELECT = 70.f * SCALE.y;
    const ImVec2 MENU_SIZE(SIZE_LIST.x, SIZE_SELECT);
    const auto SCREEN_POS = ImGui::GetCursorScreenPos();
    const auto DRAW_LIST = ImGui::GetWindowDrawList();
    const float rect_size = 6.f * SCALE.y;
    const float half_rect_size = rect_size / 2.f;
    const auto rounding = 15.f * SCALE.x;
    const ImU32 COLOR_PULSE_BORDER = get_selectable_color_pulse();
    const ImU32 COLOR_PULSE_MID = get_selectable_color_pulse(40.f);
    const ImU32 COLOR_PULSE_FILL = get_selectable_color_pulse(120.f);
    static ImVec2 animated_rectangle_pos{};
    const auto ImLerp = [](const ImVec2 &a, const ImVec2 &b, float t) {
        return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    };
    const auto draw_menu = [&](const std::map<uint32_t, std::string> &menu_list) {
        for (uint32_t i = 0; i < menu_list.size(); ++i) {
            const auto &item_title = menu_list.at(i);
            const float INIT_MENU_POS = i * MENU_SIZE.y;
            const ImVec2 MENU_POS_MIN(SCREEN_POS.x, SCREEN_POS.y + INIT_MENU_POS);
            if (current_menu == static_cast<int32_t>(i)) {
                const float speed = 1.f - std::exp(-10.f * ImGui::GetIO().DeltaTime);
                animated_rectangle_pos = ImLerp(animated_rectangle_pos, MENU_POS_MIN, speed);
                const ImVec2 pos_min = animated_rectangle_pos;
                const ImVec2 pos_max(pos_min.x + MENU_SIZE.x, pos_min.y + MENU_SIZE.y);
                const ImVec2 pos_mid(pos_min.x, pos_min.y + MENU_SIZE.y * 0.5f);
                const ImVec2 pos_mid_max(pos_max.x, pos_mid.y);
                DRAW_LIST->AddRectFilledMultiColor(pos_min, pos_mid_max, COLOR_PULSE_BORDER, COLOR_PULSE_BORDER, COLOR_PULSE_MID, COLOR_PULSE_MID);
                DRAW_LIST->AddRectFilledMultiColor(pos_mid, pos_max, COLOR_PULSE_MID, COLOR_PULSE_MID, COLOR_PULSE_BORDER, COLOR_PULSE_BORDER);
            }
            const bool clicked = ImGui::InvisibleButton(item_title.c_str(), MENU_SIZE);
            if (ImGui::IsItemHovered())
                current_menu = i;
            if (clicked) {
                current_menu = i;
                handle_seting_menu_selection(gui, emuenv);
            }
            ImGui::SetCursorPos(ImVec2(100.f * SCALE.x, INIT_MENU_POS + (MENU_SIZE.y / 2.f) - (ImGui::GetFontSize() / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", item_title.c_str());
            ImGui::SetCursorPos(ImVec2(0.f, INIT_MENU_POS + MENU_SIZE.y));
            ImGui::Separator();
        }
    };

    // Settings
    switch (settings_menu) {
    case SettingsMenu::SELECT:
        title = lang.main["title"];
        ImGui::SetWindowFontScale(1.2f);
        draw_menu({ { SettingsMenu::SOUND_DISPLAY, sound_display["title"] }, { SettingsMenu::THEME_BACKGROUND, theme_background.main["title"] }, { SettingsMenu::DATE_TIME, date_time.main["title"] }, { SettingsMenu::LANGUAGE, language.main["title"] } });
        break;
    case SettingsMenu::SOUND_DISPLAY: {
        title = sound_display["title"];
        ImGui::SetWindowFontScale(1.02f);
        const auto system_music_size = ImGui::CalcTextSize(sound_display["system_music"].c_str());
        ImGui::SetCursorPos(ImVec2(116.f * SCALE.x, (SIZE_SELECT - system_music_size.y) / 2.f));
        ImGui::Text("%s", sound_display["system_music"].c_str());
        const ImVec2 padding_checkbox(8.6f * SCALE.y, 8.6f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding_checkbox);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(227.f / 255.f, 237.f / 255.f, 225.f / 255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.62f, 0.75f, 0.62f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.62f, 0.75f, 0.62f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.62f, 0.75f, 0.62f, 0.8f));
        ImGui::SetCursorPos(ImVec2(SIZE_LIST.x - (106.f * SCALE.x), (SIZE_SELECT - (42.f * SCALE.y)) / 2.f));
        if (ImGui::Checkbox("##system_music", &user.system_music)) {
            if (bgm_player::init_bgm(emuenv.vita_fs_path, user.system_music))
                bgm_player::switch_bgm_state(false);
            app::save_user(emuenv, emuenv.io.user_id);
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        break;
    }
    case SettingsMenu::THEME_BACKGROUND: {
        // Themes & Backgrounds
        const auto select = common["select"].c_str();
        switch (menu) {
        case Menu::UNDEFINED: {
            title = theme_background.main["title"];
            ImGui::SetWindowFontScale(1.2f);
            draw_menu({ { Menu::THEME, theme_background.theme.main["title"] }, { Menu::START, theme_background.start_screen["title"] }, { Menu::BACKGROUND, theme_background.home_screen_backgrounds["title"] } });
            const auto current_theme = themes_info.find(user.theme_id);
            const std::string current_title = gui.theme_catalog_loader ? "..." : current_theme != themes_info.end() ? current_theme->second.title
                                                                                                                    : std::string{};
            ImGui::SetWindowFontScale(0.66f);
            draw_ellipsis_text(current_title, 258.f * SCALE.x, ImVec2(SIZE_LIST.x - (60.f * SCALE.x), MENU_SIZE.y / 2.f), ImVec2(1.f, 0.f), GUI_COLOR_TEXT);
            break;
        }
        case Menu::THEME: {
            if (gui.theme_catalog_loader) {
                ImGui::TextUnformatted("...");
                break;
            }
            if (!gui.theme_catalog_error.empty()) {
                ImGui::TextWrapped("%s", gui.theme_catalog_error.c_str());
                break;
            }
            // Theme List
            if (selected.empty()) {
                title = theme.main["title"];

                // Delete Theme
                if (!delete_theme.empty()) {
                    gui.themes_package.erase(delete_theme);
                    themes_info.erase(delete_theme);
                    const auto theme_list_index = std::ranges::find_if(themes_list, [&](const auto &t) {
                        return t.first == delete_theme;
                    });
                    if (theme_list_index != themes_list.end())
                        themes_list.erase(theme_list_index);
                    delete_theme.clear();
                    get_themes_list(gui, emuenv);
                }

                // Set Scroll Pos
                if (set_scroll_pos != -1) {
                    ImGui::SetScrollY(set_scroll_pos);
                    set_scroll_pos = -1;
                }
                current_scroll_pos = ImGui::GetScrollY();
                max_scroll_pos = ImGui::GetScrollMaxY();
                if (is_scroll_animating)
                    is_scroll_animating = set_scroll_animation(scroll_y, target_scroll_y, current_selected_theme, ImGui::SetScrollY);
                themes_list_filtered.clear();
                const auto COLUMN_WIDTH = SIZE_LIST.x / 3.f;
                const auto MARGIN_BEGIN = 20.f * SCALE.x;
                const auto MARGIN_HEIGHT = 80.f * SCALE.y;
                const auto filtered_count = std::count_if(themes_list.begin(), themes_list.end(), [&](const auto &item) {
                    const auto &info = themes_info.at(item.first);
                    return search_bar.PassFilter(info.title.c_str()) || search_bar.PassFilter(item.first.c_str());
                });
                const auto last_line = filtered_count > 0 ? (filtered_count - 1) / 3 : 0;
                uint32_t current_col = 0, current_line = 0;
                static ImVec2 animated_frame_pos{}, frame_target_pos{};
                const ImVec2 FRAME_SIZE(SIZE_PACKAGE.x + rect_size, SIZE_PACKAGE.y + rect_size);
                const auto set_scroll = [&](float desired_scroll) {
                    if (std::abs(desired_scroll - current_scroll_pos) > 1.f) {
                        target_scroll_y = desired_scroll;
                        is_scroll_animating = true;
                    }
                };
                std::vector<std::string> visible_themes;
                for (const auto &[id, updated] : themes_list) {
                    const auto &info = themes_info.at(id);
                    if (!search_bar.PassFilter(info.title.c_str()) && !search_bar.PassFilter(id.c_str()))
                        continue;
                    ImGui::PushID(id.c_str());
                    themes_list_filtered.push_back({ id, current_line, current_col });
                    const bool is_current_theme = current_selected_theme == id;
                    const ImVec2 GRID_THEME_POS(rect_size + current_col * COLUMN_WIDTH, MARGIN_BEGIN + rect_size + current_line * (SIZE_PACKAGE.y + MARGIN_HEIGHT));
                    ImGui::SetCursorPos(GRID_THEME_POS);
                    if (ImGui::InvisibleButton("##preview", SIZE_PACKAGE)) {
                        selected = id;
                        set_scroll_pos = current_scroll_pos;
                        init_theme_preview(gui, emuenv, selected);
                    }
                    if (ImGui::IsItemHovered())
                        current_selected_theme = id;
                    const auto item_max = ImGui::GetItemRectMax().y;
                    const auto item_min = item_max - FRAME_SIZE.y;
                    const auto list_max = POS_LIST.y + SIZE_LIST.y;
                    const bool visible = item_max >= POS_LIST.y && item_min <= list_max;
                    if (visible && item_min >= POS_LIST.y && item_max <= list_max)
                        visible_themes.emplace_back(id);
                    const ImVec2 IMAGE_POS_MIN(SCREEN_POS.x + GRID_THEME_POS.x, SCREEN_POS.y + GRID_THEME_POS.y);
                    const ImVec2 IMAGE_POS_MAX(IMAGE_POS_MIN.x + SIZE_PACKAGE.x, IMAGE_POS_MIN.y + SIZE_PACKAGE.y);
                    const ImVec2 FRAME_POS_MIN(IMAGE_POS_MIN.x - half_rect_size, IMAGE_POS_MIN.y - half_rect_size);
                    const ImVec2 FRAME_POS_MAX(FRAME_POS_MIN.x + FRAME_SIZE.x, FRAME_POS_MIN.y + FRAME_SIZE.y);
                    if (is_current_theme && gui.is_nav_button) {
                        const float item_center_y = item_min + FRAME_SIZE.y * 0.5f;
                        const float list_center_y = POS_LIST.y + SIZE_LIST.y * 0.5f;
                        if (current_line == 0)
                            set_scroll(current_scroll_pos + item_min - MARGIN_BEGIN - POS_LIST.y);
                        else if (current_line == last_line)
                            set_scroll(max_scroll_pos);
                        else
                            set_scroll(current_scroll_pos + item_center_y - list_center_y);
                    }
                    if (is_current_theme && !is_scroll_animating)
                        frame_target_pos = FRAME_POS_MIN;
                    if (visible) {
                        const auto image = gui.themes_package.find(id);
                        if (image != gui.themes_package.end())
                            DRAW_LIST->AddImageRounded(image->second, IMAGE_POS_MIN, IMAGE_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, rounding);
                        DRAW_LIST->AddRect(FRAME_POS_MIN, FRAME_POS_MAX, IM_COL32(200, 200, 200, 80), rounding, ImDrawFlags_None, rect_size);
                        ImGui::SetWindowFontScale(0.6f);
                        draw_ellipsis_text(info.title, FRAME_SIZE.x + rect_size, ImVec2(GRID_THEME_POS.x - rect_size, GRID_THEME_POS.y + FRAME_SIZE.y + ImGui::GetStyle().ItemSpacing.y), ImVec2(-1, 1), GUI_COLOR_TEXT);
                    }
                    ImGui::PopID();
                    if (++current_col >= 3) {
                        current_col = 0;
                        ++current_line;
                    }
                }
                const float speed = 1.f - std::exp(-10.f * ImGui::GetIO().DeltaTime);
                animated_frame_pos = ImLerp(animated_frame_pos, frame_target_pos, speed);
                const ImVec2 frame_max(animated_frame_pos.x + FRAME_SIZE.x, animated_frame_pos.y + FRAME_SIZE.y);
                DRAW_LIST->AddRectFilled(animated_frame_pos, frame_max, COLOR_PULSE_FILL, rounding);
                DRAW_LIST->AddRect(animated_frame_pos, frame_max, COLOR_PULSE_BORDER, rounding, ImDrawFlags_None, rect_size);
                first_visible_theme = visible_themes.empty() ? std::string{} : visible_themes.front();
                const ImVec2 download_pos(rect_size + current_col * COLUMN_WIDTH, MARGIN_BEGIN + rect_size + current_line * (SIZE_PACKAGE.y + MARGIN_HEIGHT));
                ImGui::SetCursorPos(download_pos);
                if (ImGui::Selectable("##download_theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    open_path("https://psvt.ovh");
                ImGui::SetWindowFontScale(0.6f);
                ImGui::SetCursorPos(ImVec2(download_pos.x, download_pos.y + SIZE_PACKAGE.y + ImGui::GetStyle().ItemSpacing.y));
                ImGui::PushTextWrapPos(download_pos.x + SIZE_PACKAGE.x + rect_size);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme.main["find_a_psvita_custom_themes"].c_str());
                ImGui::PopTextWrapPos();
                ImGui::ScrollWhenDragging();
            } else {
                // Theme Select
                if (gui.theme_preview_id != selected)
                    init_theme_preview(gui, emuenv, selected);
                title = themes_info[selected].title;
                switch (popup) {
                case Popup::UNDEFINED: {
                    if (gui.theme_preview.contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(15.f * SCALE.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.theme_preview.at(HOME), SIZE_PREVIEW);
                    }
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCALE.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.theme_preview.at(LOCK), SIZE_PREVIEW);
                    }
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y));
                    if ((selected != user.theme_id || !vita_shell::get_theme_state().loaded || vita_shell::get_theme_state().content_id != selected) && ImGui::Button(select, BUTTON_SIZE))
                        set_theme(gui, emuenv);
                    break;
                }
                case Popup::DEL: {
                    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
                    ImGui::Begin("##delete_theme", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
                    ImGui::BeginChild("##delete_theme_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetCursorPos(ImVec2(48.f * SCALE.x, 28.f * SCALE.y));
                    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
                    if (gui.themes_package.contains(selected))
                        ImGui::Image(gui.themes_package.at(selected), SIZE_MINI_PACKAGE);
                    else
                        ImGui::Dummy(SIZE_MINI_PACKAGE);
                    ImGui::SameLine(0, 22.f * SCALE.x);
                    const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[selected].title.c_str(), nullptr, false, POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - (70.f * SCALE.x)).y / 2.f;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_MINI_PACKAGE.y / 2.f) - CALC_TITLE);
                    ImGui::TextWrapped("%s", themes_info[selected].title.c_str());
                    ImGui::SetCursorPosY(POPUP_SIZE.y / 2.f);
                    TextColoredCentered(GUI_COLOR_TEXT, theme.main["delete"].c_str());
                    ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
                    if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), BUTTON_SIZE))
                        popup = Popup::UNDEFINED;
                    ImGui::SameLine(0, 40.f * SCALE.x);
                    if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE)) {
                        gui.theme_preview_loader.reset();
                        const bool can_delete = selected != user.theme_id || apply_theme(gui, emuenv, "default");
                        if (!can_delete) {
                            popup = Popup::UNDEFINED;
                            start_theme_previews(gui, emuenv);
                        } else if (selected == user.theme_id) {
                            user.theme_id = "default";
                            user.start_path.clear();
                            user.start_type = "default";
                            user.use_theme_bg = !gui.theme_backgrounds.empty();
                            app::save_user(emuenv, emuenv.io.user_id);
                            init_theme_start_background(gui, emuenv, "default");
                        }
                        if (can_delete) {
                            std::error_code error;
                            fs::remove_all(emuenv.vita_fs_path / "ux0/theme" / fs_utils::utf8_to_path(selected), error);
                            if (error) {
                                LOG_ERROR("Cannot delete theme '{}': {}", selected, error.message());
                                start_theme_previews(gui, emuenv);
                            } else {
                                if (emuenv.app_path == "emu:app/NPXS10026")
                                    init_content_manager(gui, emuenv);
                                delete_theme = selected;
                                popup = Popup::UNDEFINED;
                                selected.clear();
                                gui.theme_preview.clear();
                                gui.theme_preview_id.clear();
                                set_scroll_pos = current_scroll_pos;
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::End();
                    break;
                }
                case Popup::INFORMATION: {
                    if (gui.theme_preview.contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(119.f * SCALE.x, 4.f * SCALE.y));
                        ImGui::Image(gui.theme_preview.at(HOME), SIZE_MINI_PREVIEW);
                    }
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2(SIZE_LIST.x / 2.f + (15.f * SCALE.y), 4.f * SCALE.y));
                        ImGui::Image(gui.theme_preview.at(LOCK), SIZE_MINI_PREVIEW);
                    }
                    const auto INFO_POS = ImVec2(280.f * SCALE.x, 30.f * SCALE.y);
                    auto &info = theme.information;
                    ImGui::SetWindowFontScale(0.94f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["name"].c_str());
                    ImGui::SameLine();
                    ImGui::PushTextWrapPos(SIZE_LIST.x - (30.f * SCALE.x));
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].title.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["provider"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].provided.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["updated"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    auto DATE_TIME = get_date_time(gui, emuenv, themes_info[selected].updated);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                    if (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR) {
                        ImGui::SameLine();
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                    }
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["size"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%zu KB", themes_info[selected].size);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["version"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].version.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["content_id"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextWrapped("%s", selected.c_str());
                    ImGui::ScrollWhenDragging();
                    break;
                }
                }
            }
            break;
        }
        case Menu::START: {
            if (sub_menu == SubMenu::UNDEFINED) {
                title = theme_background.start_screen["title"];
                ImGui::SetWindowFontScale(0.72f);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                const auto PACKAGE_POS_Y = (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (72.f * SCALE.y);
                const auto is_not_default = user.theme_id != "default";
                if (is_not_default) {
                    const auto THEME_POS = ImVec2(15.f * SCALE.x, PACKAGE_POS_Y);
                    if (gui.themes_package.contains(user.theme_id)) {
                        ImGui::SetCursorPos(THEME_POS);
                        ImGui::Image(gui.themes_package.at(user.theme_id), SIZE_PACKAGE);
                    }
                    ImGui::SetCursorPos(THEME_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(user.start_type == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                        sub_menu = SubMenu::THEME;
                        init_theme_preview(gui, emuenv, user.theme_id);
                    }
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(15.f * SCALE.x);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[user.theme_id].title.c_str());
                    ImGui::PopTextWrapPos();
                }
                const auto IMAGE_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f) : 15.f * SCALE.x, PACKAGE_POS_Y);
                if ((user.start_type == "image") && gui.start_background) {
                    ImGui::SetCursorPos(IMAGE_POS);
                    ImGui::Image(gui.start_background, SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(IMAGE_POS);
                if (user.start_type == "image")
                    ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(user.start_type == "image" ? "V" : theme_background.start_screen["add_image"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    sub_menu = SubMenu::IMAGE;
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(IMAGE_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.start_screen["image"].c_str());
                const auto DEFAULT_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCALE.y) : (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), PACKAGE_POS_Y);
                if (gui.themes_package.contains("default")) {
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::Image(gui.themes_package.at("default"), SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(DEFAULT_POS);
                ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(user.start_type == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                    sub_menu = SubMenu::DEFAULT;
                    init_theme_preview(gui, emuenv, "default");
                }
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(DEFAULT_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.main["default"].c_str());
                ImGui::PopStyleVar();
                ImGui::SetWindowFontScale(0.90f);
            } else {
                const auto START_PREVIEW_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y));
                const auto SELECT_BUTTON_POS = ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y);
                if (sub_menu == SubMenu::THEME) {
                    if (gui.theme_preview_id != user.theme_id)
                        init_theme_preview(gui, emuenv, user.theme_id);
                    title = themes_info[user.theme_id].title;
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.theme_preview.at(LOCK), SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((user.start_type != "theme") && ImGui::Button(select, BUTTON_SIZE)) {
                        user.start_path.clear();
                        user.start_type = "theme";
                        init_theme_start_background(gui, emuenv, user.theme_id);
                        app::save_user(emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                        gui.theme_preview.clear();
                        gui.theme_preview_id.clear();
                    }
                } else if (sub_menu == SubMenu::IMAGE) {
                    fs::path image_path{};
                    host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(image_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });

                    if ((result == host::dialog::filesystem::Result::SUCCESS) && fs::exists(image_path)) {
                        if (init_user_start_background(gui, fs_utils::path_to_utf8(image_path))) {
                            user.start_path = fs_utils::path_to_utf8(image_path);
                            user.start_type = "image";
                            app::save_user(emuenv, emuenv.io.user_id);
                        }
                    } else if (result == host::dialog::filesystem::Result::ERROR) {
                        LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                    }
                    sub_menu = SubMenu::UNDEFINED;
                } else if (sub_menu == SubMenu::DEFAULT) {
                    if (gui.theme_preview_id != "default")
                        init_theme_preview(gui, emuenv, "default");
                    title = theme_background.main["default"];
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.theme_preview.at(LOCK), SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((user.start_type != "default") && ImGui::Button(select, BUTTON_SIZE)) {
                        user.start_path.clear();
                        init_theme_start_background(gui, emuenv, "default");
                        user.start_type = "default";
                        app::save_user(emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                        gui.theme_preview.clear();
                        gui.theme_preview_id.clear();
                    }
                }
            }
            break;
        }
        case Menu::BACKGROUND: {
            title = theme_background.home_screen_backgrounds["title"];

            // Delete user background
            if (!delete_user_background.empty()) {
                vector_utils::erase_first(user.backgrounds, delete_user_background);
                gui.user_backgrounds.erase(delete_user_background);
                gui.user_backgrounds_infos.erase(delete_user_background);
                if (!user.backgrounds.empty())
                    gui.current_user_bg = gui.current_user_bg % gui.user_backgrounds.size();
                else if (!gui.theme_backgrounds.empty())
                    user.use_theme_bg = true;
                app::save_user(emuenv, emuenv.io.user_id);
                delete_user_background.clear();
            }

            ImGui::SetWindowFontScale(0.80f);
            ImGui::Columns(3, nullptr, false);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
            for (const auto &background : user.backgrounds) {
                const auto IMAGE_POS = ImGui::GetCursorPos();
                const auto PREVIEW_POS = ImVec2(IMAGE_POS.x + (gui.user_backgrounds_infos[background].prev_pos.x * SCALE.x), IMAGE_POS.y + (gui.user_backgrounds_infos[background].prev_pos.y * SCALE.y));
                const auto PREVIEW_SIZE = ImVec2(gui.user_backgrounds_infos[background].prev_size.x * SCALE.x, gui.user_backgrounds_infos[background].prev_size.y * SCALE.y);
                ImGui::SetCursorPos(PREVIEW_POS);
                ImGui::Image(gui.user_backgrounds[background], PREVIEW_SIZE);
                ImGui::SetCursorPosY(IMAGE_POS.y);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                ImGui::PushID(background.c_str());
                if (ImGui::Selectable(theme_background.home_screen_backgrounds["delete_background"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    delete_user_background = background;
                ImGui::PopID();
                ImGui::PopStyleColor();
                ImGui::NextColumn();
            }
            if (ImGui::Selectable(theme_background.home_screen_backgrounds["add_background"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                fs::path background_path{};
                host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(background_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });
                if (result == host::dialog::filesystem::Result::SUCCESS) {
                    auto background_path_string = fs_utils::path_to_utf8(background_path);
                    if (!gui.user_backgrounds.contains(background_path_string) && fs::exists(background_path) && init_user_background(gui, emuenv, background_path_string)) {
                        user.backgrounds.push_back(background_path_string);
                        user.use_theme_bg = false;
                        app::save_user(emuenv, emuenv.io.user_id);
                    }
                } else if (result == host::dialog::filesystem::Result::ERROR) {
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                }
            }
            ImGui::PopStyleVar();
            ImGui::Columns(1);
            break;
        }
        }
        break;
    }
    case SettingsMenu::DATE_TIME:
        // Date & Time
        title = date_time.main["title"];
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (ImGui::Selectable(date_time.date_format["title"].c_str(), menu == Menu::DATE_FORMAT, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            if (menu == Menu::UNDEFINED)
                menu = Menu::DATE_FORMAT;
            else
                menu = Menu::UNDEFINED;
        }
        ImGui::Separator();
        if (ImGui::Selectable(date_time.time_format["title"].c_str(), menu == Menu::TIME_FORMAT, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            if (menu == Menu::UNDEFINED)
                menu = Menu::TIME_FORMAT;
            else
                menu = Menu::UNDEFINED;
        }
        ImGui::Separator();
        ImGui::PopStyleVar();
        if (menu != Menu::UNDEFINED) {
            const auto TIME_SELECT_SIZE = 336.f * SCALE.x;
            ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - TIME_SELECT_SIZE, WINDOW_POS.y), ImGuiCond_Always, ImVec2(0.f, 0.f));
            ImGui::SetNextWindowSize(ImVec2(TIME_SELECT_SIZE, WINDOW_SIZE.y), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.16f, 0.18f, 1.f));
            ImGui::Begin("##time", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 30.f * SCALE.x);
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            if (menu == Menu::DATE_FORMAT) {
                const auto get_date_format_sting = [&](SceSystemParamDateFormat date_format) {
                    std::string date_format_str;
                    auto &lang = gui.lang.settings.date_time.date_format;
                    switch (date_format) {
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD: date_format_str = lang["yyyy_mm_dd"]; break;
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY: date_format_str = lang["dd_mm_yyyy"]; break;
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY: date_format_str = lang["mm_dd_yyyy"]; break;
                    default: break;
                    }

                    return date_format_str;
                };

                for (auto f = 0; f < 3; f++) {
                    auto date_format_value = static_cast<SceSystemParamDateFormat>(f);
                    const auto date_format_str = get_date_format_sting(date_format_value);
                    ImGui::PushID(date_format_str.c_str());
                    ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    if (ImGui::Selectable(emuenv.cfg.sys_date_format == date_format_value ? "V" : "##date_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                        if (emuenv.cfg.sys_date_format != date_format_value) {
                            emuenv.cfg.sys_date_format = date_format_value;
                            config::serialize_config(emuenv.cfg, emuenv.config_path);
                        }
                        menu = Menu::UNDEFINED;
                    }
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    ImGui::Selectable(date_format_str.c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
            } else if (menu == Menu::TIME_FORMAT) {
                ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
                if (ImGui::Selectable(is_12_hour_format ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (!is_12_hour_format) {
                        emuenv.cfg.sys_time_format = SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    menu = Menu::UNDEFINED;
                }
                ImGui::NextColumn();
                ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                ImGui::Selectable(date_time.time_format["clock_12_hour"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
                if (ImGui::Selectable(!is_12_hour_format ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (is_12_hour_format) {
                        emuenv.cfg.sys_time_format = SCE_SYSTEM_PARAM_TIME_FORMAT_24HOUR;
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    menu = Menu::UNDEFINED;
                }
                ImGui::NextColumn();
                ImGui::Selectable(date_time.time_format["clock_24_hour"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::PopStyleVar();
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                menu = Menu::UNDEFINED;
        }
        break;
    case SettingsMenu::LANGUAGE:
        // Language
        if (menu == Menu::UNDEFINED) {
            title = language.main["title"];
            ImGui::Columns(2, nullptr, false);
            ImGui::SetWindowFontScale(1.2f);
            const auto sys_lang_str = language.main["system_language"].c_str();
            const auto sys_lang_str_size = ImGui::CalcTextSize(sys_lang_str).x;
            ImGui::SetColumnWidth(0, sys_lang_str_size + 40.f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(sys_lang_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT))) {
                if (popup == Popup::UNDEFINED)
                    popup = Popup::SELECT_SYS_LANG;
                else
                    popup = Popup::UNDEFINED;
            }
            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
            ImGui::Selectable(get_sys_lang_name(emuenv.cfg.sys_lang).c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(language.input_language["title"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                menu = Menu::SELECT_INPUT_LANG;
            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
            ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
            ImGui::PopStyleVar();
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::Columns(1);
            ImGui::PopStyleVar();
            if (popup == Popup::SELECT_SYS_LANG) {
                const auto SYS_LANG_SIZE = WINDOW_SIZE.x / 2.f;
                ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - SYS_LANG_SIZE, WINDOW_POS.y), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(SYS_LANG_SIZE, WINDOW_SIZE.y), ImGuiCond_Always);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.16f, 0.18f, 1.f));
                ImGui::Begin("##system_language", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                ImGui::BeginDisabled(gui.apps_list_scan_running.load());
                for (const auto &sys_lang : LIST_SYS_LANG) {
                    ImGui::PushID(sys_lang.first);
                    const auto is_current_lang = emuenv.cfg.sys_lang == sys_lang.first;
                    if (ImGui::Selectable(is_current_lang ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT))) {
                        if (!is_current_lang) {
                            const auto previous_sys_lang = emuenv.cfg.sys_lang;
                            emuenv.cfg.sys_lang = sys_lang.first;
                            config::serialize_config(emuenv.cfg, emuenv.config_path);
                            gui::init_lang(gui.lang, emuenv);
                            const auto live_area_state = get_live_area_current_open_apps_list_index(gui, "emu:app/NPXS10015") != gui.live_area_current_open_apps_list.end();
                            if (previous_sys_lang != emuenv.cfg.sys_lang) {
                                get_sys_apps_title(gui, emuenv);
                                init_user_apps(gui, emuenv);
                            }
                            gui.live_area_current_open_apps_list.clear();
                            gui.live_area_contents.clear();
                            gui.live_items.clear();
                            init_notice_info(gui, emuenv);
                            if (live_area_state) {
                                update_live_area_current_open_apps_list(gui, emuenv, "emu:app/NPXS10015");
                                init_live_area(gui, emuenv, "emu:app/NPXS10015");
                            }
                        }
                        popup = Popup::UNDEFINED;
                    }
                    ImGui::NextColumn();
                    ImGui::Selectable(sys_lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                    ImGui::ScrollWhenDragging();
                }
                ImGui::EndDisabled();
                ImGui::Columns(1);
                ImGui::PopStyleVar();
                ImGui::End();
                ImGui::PopStyleColor();
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    popup = Popup::UNDEFINED;
            }
        } else {
            // Input Languages
            const auto keyboards_str = language.keyboards["title"].c_str();
            if (sub_menu == SubMenu::UNDEFINED) {
                title = language.input_language["title"];
                ImGui::SetWindowFontScale(1.2f);
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 600.f * sdl::get_viewport().manual_dpi_scale);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(keyboards_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                    sub_menu = SubMenu::SELECT_KEYBOARDS;
                ImGui::PopStyleVar();
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.f);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
                ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                ImGui::PopStyleVar();
                ImGui::Separator();
                ImGui::NextColumn();
                ImGui::Columns(1);
            } else {
                if (selected.empty()) {
                    title = keyboards_str;
                    ImGui::Columns(3, nullptr, false);
                    ImGui::SetColumnWidth(0, 40.f * sdl::get_viewport().manual_dpi_scale);
                    ImGui::SetColumnWidth(1, 560.f * sdl::get_viewport().manual_dpi_scale);
                    for (const auto &lang : emuenv.ime.lang.ime_keyboards) {
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                        ImGui::PushID(lang.first);
                        ImGui::SetWindowFontScale(1.f);
                        const auto is_lang_enable = vector_utils::contains(emuenv.cfg.ime_langs, static_cast<uint64_t>(lang.first));
                        if (ImGui::Selectable(is_lang_enable ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                            selected = std::to_string(lang.first);
                        ImGui::NextColumn();
                        ImGui::SetWindowFontScale(1.2f);
                        ImGui::Selectable(lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                        ImGui::PopStyleVar();
                        ImGui::NextColumn();
                        ImGui::SetWindowFontScale(1.f);
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
                        ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                        ImGui::Separator();
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                        ImGui::NextColumn();
                    }
                    ImGui::Columns(1);
                    ImGui::ScrollWhenDragging();
                } else {
                    SceImeLanguage lang_select = static_cast<SceImeLanguage>(string_utils::stoi_def(selected, 0, "language"));
                    title = get_ime_lang_index(emuenv.ime, lang_select)->second;
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 650.f * SCALE.x);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::Selectable(title.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                    ImGui::PopStyleVar();
                    ImGui::NextColumn();
                    auto ime_lang_cfg_index = std::find(emuenv.cfg.ime_langs.begin(), emuenv.cfg.ime_langs.end(), lang_select);
                    auto is_ime_lang_enable = ime_lang_cfg_index != emuenv.cfg.ime_langs.end();
                    ImGui::SetWindowFontScale(1.4f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (16.f * SCALE.y));
                    if (ImGui::Checkbox("##lang", &is_ime_lang_enable)) {
                        if (ime_lang_cfg_index != emuenv.cfg.ime_langs.end()) {
                            emuenv.cfg.ime_langs.erase(ime_lang_cfg_index);
                            if (emuenv.cfg.ime_langs.empty())
                                emuenv.cfg.current_ime_lang = SCE_IME_LANGUAGE_ENGLISH_US;
                            else if (emuenv.cfg.current_ime_lang == lang_select)
                                emuenv.cfg.current_ime_lang = emuenv.cfg.ime_langs.back();
                        } else {
                            emuenv.cfg.ime_langs.push_back(lang_select);

                            // Sort Ime lang in good order
                            if (emuenv.cfg.ime_langs.size() > 1) {
                                std::vector<uint64_t> cfg_ime_langs_temp;
                                for (const auto &[lang_id, _] : emuenv.ime.lang.ime_keyboards) {
                                    if (vector_utils::contains(emuenv.cfg.ime_langs, (uint64_t)lang_id))
                                        cfg_ime_langs_temp.push_back(lang_id);
                                }
                                emuenv.cfg.ime_langs = std::move(cfg_ime_langs_temp);
                            }
                        }
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    ImGui::NextColumn();
                    ImGui::Columns(1);
                }
            }
        }
        break;
    default:
        break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Back
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(6.f * SCALE.x, WINDOW_SIZE.y - (52.f * SCALE.y)));
    if (ImGui::Button("<<", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)))
        back_settings(gui, emuenv);

    if ((settings_menu == SettingsMenu::THEME_BACKGROUND) && !selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (70.f * SCALE.x), WINDOW_SIZE.y - (52.f * SCALE.y)));
        if (popup == Popup::UNDEFINED && ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...")) {
            if (ImGui::MenuItem(theme.information["title"].c_str()))
                popup = Popup::INFORMATION;
            if (ImGui::MenuItem(::lang::get(::lang::str::delete_).c_str()))
                popup = Popup::DEL;
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
