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

#include <config/functions.h>
#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <ime/functions.h>
#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <lang/functions.h>
#include <numeric>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <pugixml.hpp>
#include <stb_image.h>
#include <util/vector_utils.h>

#undef ERROR

namespace gui {

struct Theme {
    std::string title;
    std::string provided;
    tm updated;
    size_t size;
    std::string version;
    std::string package;
    std::map<ThemePreviewType, std::string> preview;
};

static std::map<std::string, Theme> themes_info;
static std::vector<std::pair<std::string, time_t>> themes_list;
static time_t last_updated_themes_list = 0;
static uint32_t themes_cache_lang;

static bool get_themes_cache(EmuEnvState &emuenv) {
    const auto theme_path{ emuenv.pref_path / "ux0/theme" };
    const auto fw_theme_path{ emuenv.pref_path / "vs0/data/internal/theme" };

    // Check if the themes list has been updated recently to avoid unnecessary updates
    if ((fs::last_write_time(theme_path) <= last_updated_themes_list) && (fs::last_write_time(fw_theme_path) <= last_updated_themes_list))
        return true;
    else if (last_updated_themes_list != 0) {
        LOG_INFO("Found new update of themes list, updating...");
        return false;
    }

    const auto themes_cache_path{ emuenv.pref_path / "ux0/temp/themes.dat" };
    fs::ifstream themes_cache(themes_cache_path, std::ios::in | std::ios::binary);
    if (themes_cache.is_open()) {
        themes_info.clear();
        themes_list.clear();

        // Read size of themes list
        size_t size;
        themes_cache.read((char *)&size, sizeof(size));

        // Check version of cache
        uint32_t versionInFile;
        themes_cache.read((char *)&versionInFile, sizeof(uint32_t));
        if (versionInFile != 1) {
            LOG_WARN("Current version of cache: {}, is outdated, recreate it.", versionInFile);
            themes_cache.close();
            return false;
        }

        // Read language of cache
        themes_cache.read((char *)&themes_cache_lang, sizeof(uint32_t));
        if (themes_cache_lang != emuenv.cfg.sys_lang) {
            LOG_WARN("Current lang of cache: {}, is different configuration: {}, recreate it.", get_sys_lang_name(themes_cache_lang), get_sys_lang_name(emuenv.cfg.sys_lang));
            themes_cache.close();
            return false;
        }

        // Read updated time of themes list
        themes_cache.read((char *)&last_updated_themes_list, sizeof(time_t));

        // Read Theme info value
        for (size_t t = 0; t < size; t++) {
            auto read = [&themes_cache]() {
                size_t size;

                themes_cache.read((char *)&size, sizeof(size));

                std::vector<char> buffer(size); // dont trust std::string to hold buffer enough
                themes_cache.read(buffer.data(), size);

                return std::string(buffer.begin(), buffer.end());
            };

            Theme theme;

            const std::string content_id = read();
            theme.title = read();
            theme.provided = read();

            // Read updated time of theme, convert it to tm structure
            time_t updated;
            themes_cache.read(reinterpret_cast<char *>(&updated), sizeof(time_t));
            SAFE_LOCALTIME(&updated, &theme.updated);

            themes_cache.read(reinterpret_cast<char *>(&theme.size), sizeof(size_t));
            theme.version = read();
            theme.package = read();
            theme.preview[HOME] = read();
            theme.preview[LOCK] = read();

            themes_info.emplace(content_id, theme);
            themes_list.emplace_back(content_id, updated);
        }

        themes_cache.close();

        std::sort(themes_list.begin(), themes_list.end(), [&](const auto &ta, const auto &tb) {
            return ta.second > tb.second;
        });
    }

    return !themes_info.empty();
}

void init_theme_package(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    const auto theme = themes_info[content_id];
    if (!theme.package.empty() && !gui.themes_list.contains(content_id)) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        if (content_id == "default")
            vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, theme.package);
        else
            vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, fs::path("theme") / fs_utils::utf8_to_path(content_id) / theme.package);

        if (buffer.empty()) {
            LOG_WARN("Background, Name: '{}', Not found for title: {} [{}].", theme.package, content_id, theme.title);
            return;
        }
        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid Background for title: {} [{}].", content_id, theme.title);
            return;
        }

        gui.themes_list[content_id] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }
}

static void init_theme_preview(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    const auto &theme = themes_info[content_id];

    // Clear previous theme preview
    gui.theme_preview.clear();

    for (const auto &[type, name] : themes_info[content_id].preview) {
        if (!name.empty()) {
            int32_t width = 0;
            int32_t height = 0;
            vfs::FileBuffer buffer;

            if (content_id == "default")
                vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, name);
            else
                vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, fs::path("theme") / fs_utils::utf8_to_path(content_id) / name);

            if (buffer.empty()) {
                LOG_WARN("Background, Name: '{}', Not found for title: {} [{}].", name, content_id, theme.title);
                continue;
            }
            stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Invalid Background for title: {} [{}].", content_id, theme.title);
                continue;
            }

            gui.theme_preview[type] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
        }
    }
}

static void set_theme(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    std::string user_lang;

    const auto sys_lang = static_cast<SceSystemParamLang>(themes_cache_lang);
    switch (sys_lang) {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE: user_lang = "m_ja"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: user_lang = "m_en"; break;
    case SCE_SYSTEM_PARAM_LANG_FRENCH: user_lang = "m_fr"; break;
    case SCE_SYSTEM_PARAM_LANG_SPANISH: user_lang = "m_es"; break;
    case SCE_SYSTEM_PARAM_LANG_GERMAN: user_lang = "m_de"; break;
    case SCE_SYSTEM_PARAM_LANG_ITALIAN: user_lang = "m_it"; break;
    case SCE_SYSTEM_PARAM_LANG_DUTCH: user_lang = "m_nl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: user_lang = "m_pt"; break;
    case SCE_SYSTEM_PARAM_LANG_RUSSIAN: user_lang = "m_ru"; break;
    case SCE_SYSTEM_PARAM_LANG_KOREAN: user_lang = "m_ko"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_T: user_lang = "m_ch"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S: user_lang = "m_zh"; break;
    case SCE_SYSTEM_PARAM_LANG_FINNISH: user_lang = "m_fi"; break;
    case SCE_SYSTEM_PARAM_LANG_SWEDISH: user_lang = "m_sv"; break;
    case SCE_SYSTEM_PARAM_LANG_DANISH: user_lang = "m_da"; break;
    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: user_lang = "m_no"; break;
    case SCE_SYSTEM_PARAM_LANG_POLISH: user_lang = "m_pl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: user_lang = "m_pt-br"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: user_lang = "m_en-gb"; break;
    case SCE_SYSTEM_PARAM_LANG_TURKISH: user_lang = "m_tr"; break;
    default: break;
    }

    const auto theme_path{ emuenv.pref_path / "ux0/theme" };
    const auto theme_path_xml{ theme_path / content_id / "theme.xml" };
    pugi::xml_document doc;

    if (doc.load_file(theme_path_xml.c_str())) {
        const auto infomation = doc.child("theme").child("InfomationProperty");

        // Thumbnail theme
        themes_info[content_id].package = infomation.child("m_packageImageFilePath").text().as_string();

        // Preview theme
        themes_info[content_id].preview[HOME] = infomation.child("m_homePreviewFilePath").text().as_string();
        themes_info[content_id].preview[LOCK] = infomation.child("m_startPreviewFilePath").text().as_string();

        // Title
        const auto title = infomation.child("m_title");
        if (!title.child("m_param").child(user_lang.c_str()).text().empty())
            themes_info[content_id].title = title.child("m_param").child(user_lang.c_str()).text().as_string();
        else
            themes_info[content_id].title = title.child("m_default").text().as_string();

        // Provider
        const auto provider = infomation.child("m_provider");
        if (!provider.child("m_param").child(user_lang.c_str()).text().empty())
            themes_info[content_id].provided = provider.child("m_param").child(user_lang.c_str()).text().as_string();
        else
            themes_info[content_id].provided = provider.child("m_default").text().as_string();

        const auto pred = [](const auto acc, const auto &theme) {
            if (fs::is_regular_file(theme.path()))
                return acc + fs::file_size(theme.path());
            return acc;
        };
        const auto theme_content_ids = fs::recursive_directory_iterator(theme_path / content_id);
        const auto theme_size = std::accumulate(fs::begin(theme_content_ids), fs::end(theme_content_ids), boost::uintmax_t{}, pred);

        const auto updated = fs::last_write_time(theme_path / content_id);
        SAFE_LOCALTIME(&updated, &themes_info[content_id].updated);

        themes_info[content_id].size = theme_size / KiB(1);
        themes_info[content_id].version = infomation.child("m_contentVer").text().as_string();

        themes_list.emplace_back(content_id, updated);

        // Update last updated themes list
        last_updated_themes_list = std::time(0);
    } else
        LOG_ERROR("theme not found for title: {}", content_id);
}

void save_themes_cache(EmuEnvState &emuenv) {
    const auto temp_path{ emuenv.pref_path / "ux0/temp" };
    fs::create_directories(temp_path);

    fs::ofstream themes_cache(temp_path / "themes.dat", std::ios::out | std::ios::binary);
    if (themes_cache.is_open()) {
        // Write Size of apps list
        const auto size = themes_info.size();
        themes_cache.write(reinterpret_cast<const char *>(&size), sizeof(size));

        // Write version of cache
        const uint32_t versionInFile = 1;
        themes_cache.write(reinterpret_cast<const char *>(&versionInFile), sizeof(uint32_t));

        // Write language of cache
        themes_cache.write(reinterpret_cast<const char *>(&themes_cache_lang), sizeof(uint32_t));

        // Write last updated of cache
        themes_cache.write(reinterpret_cast<const char *>(&last_updated_themes_list), sizeof(time_t));

        // Write Apps list
        for (auto &[content_id, theme] : themes_info) {
            auto write = [&themes_cache](const std::string &i) {
                const size_t size = i.length();

                themes_cache.write(reinterpret_cast<const char *>(&size), sizeof(size));
                themes_cache.write(i.c_str(), size);
            };

            write(content_id);
            write(theme.title);
            write(theme.provided);
            const time_t updated = std::mktime(&theme.updated);
            themes_cache.write(reinterpret_cast<const char *>(&updated), sizeof(time_t));
            themes_cache.write(reinterpret_cast<const char *>(&theme.size), sizeof(size_t));
            write(theme.version);
            write(theme.package);
            for (const auto &[type, preview] : theme.preview)
                write(preview);
        }
        themes_cache.close();
    }
}

static void init_themes(GuiState &gui, EmuEnvState &emuenv) {
    const auto theme_path{ emuenv.pref_path / "ux0/theme" };
    const auto fw_theme_path{ emuenv.pref_path / "vs0/data/internal/theme" };
    if ((!fs::exists(fw_theme_path) || fs::is_empty(fw_theme_path)) && (!fs::exists(theme_path) || fs::is_empty(theme_path))) {
        LOG_WARN("Theme path is empty");
        return;
    }

    // Clear all themes list
    themes_info.clear();
    themes_list.clear();

    themes_cache_lang = emuenv.cfg.sys_lang;

    for (const auto &themes : fs::directory_iterator(theme_path)) {
        if (!themes.path().empty() && fs::is_directory(themes.path())) {
            const auto content_id_path = themes.path().filename();
            const auto content_id = fs_utils::path_to_utf8(content_id_path);
            set_theme(gui, emuenv, content_id);
        }
    }

    std::sort(themes_list.begin(), themes_list.end(), [&](const auto &ta, const auto &tb) {
        return ta.second > tb.second;
    });

    if (fs::exists(fw_theme_path) && !fs::is_empty(fw_theme_path)) {
        themes_info["default"].package = "data/internal/theme/theme_defaultImage.png";

        themes_info["default"].preview[HOME] = "data/internal/theme/defaultTheme_homeScreen.png";
        themes_info["default"].preview[LOCK] = "data/internal/theme/defaultTheme_startScreen.png";

        themes_info["default"].title = gui.lang.settings.theme_background.main["default"];
        themes_list.emplace_back("default", time_t{});
    } else
        LOG_WARN("Default theme not found, install firmware fix this!");

    // Save themes cache
    save_themes_cache(emuenv);
}

void update_themes(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id) {
    if (content_id.empty())
        return;

    if ((themes_info.empty() || themes_list.empty()) && !get_themes_cache(emuenv))
        return;

    if (themes_info.contains(content_id))
        themes_info.erase(content_id);

    // Remove from themes list
    themes_list.erase(std::remove_if(themes_list.begin(), themes_list.end(), [&](const auto &theme) {
        return theme.first == content_id;
    }),
        themes_list.end());

    // Rmove package icon when it exists
    if (gui.themes_list.contains(content_id))
        gui.themes_list.erase(content_id);

    // Set new theme info
    set_theme(gui, emuenv, content_id);

    // Sort themes list by updated time
    std::sort(themes_list.begin(), themes_list.end(), [&](const auto &ta, const auto &tb) {
        return ta.second > tb.second;
    });

    save_themes_cache(emuenv);
}

static std::string current_selected_theme, selected, first_visible_theme;
static void set_theme(GuiState &gui, EmuEnvState &emuenv) {
    gui.users[emuenv.io.user_id].start_path.clear();
    if (init_theme(gui, emuenv, selected)) {
        gui.users[emuenv.io.user_id].theme_id = selected;
        gui.users[emuenv.io.user_id].use_theme_bg = true;
    } else
        gui.users[emuenv.io.user_id].use_theme_bg = false;
    init_theme_start_background(gui, emuenv, selected);
    gui.users[emuenv.io.user_id].start_type = (selected == "default") ? "default" : "theme";
    save_user(gui, emuenv, emuenv.io.user_id);
    selected.clear();
}

enum Menu {
    UNDEFINED = -1,
    THEME,
    START,
    BACKGROUND,
    DATE_FORMAT,
    TIME_FORMAT,
    SELECT_INPUT_LANG
};

enum SettingsMenu {
    SELECT = -1,
    THEME_BACKGROUND,
    DATE_TIME,
    LANGUAGE
};

static Menu menu = Menu::UNDEFINED;
static SettingsMenu settings_menu = SettingsMenu::SELECT;

static int32_t current_menu;
static uint32_t current_menu_size = 3;
static void handle_seting_menu_selection(GuiState &gui, EmuEnvState &emuenv) {
    switch (settings_menu) {
    case SettingsMenu::SELECT:
        if (current_menu == SettingsMenu::THEME_BACKGROUND) {
            if (!get_themes_cache(emuenv))
                init_themes(gui, emuenv);
        }
        settings_menu = static_cast<SettingsMenu>(current_menu);
        current_menu_size = 3;
        break;
    case SettingsMenu::THEME_BACKGROUND:
        menu = static_cast<Menu>(current_menu);
        switch (menu) {
        case Menu::THEME:
            if (current_selected_theme.empty())
                current_selected_theme = themes_list.empty() ? "" : themes_list.front().first;
            break;
        case Menu::START:
            init_theme_package(gui, emuenv, "default");
            init_theme_package(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
            break;
        default:
            break;
        }
        break;
    default: break;
    }

    // Reset after select
    current_menu = 0;
}

struct ThemeWithPosition {
    std::string theme_id;
    uint32_t line;
    uint32_t column;
};
std::vector<ThemeWithPosition> themes_list_filtered;
void browse_settings(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    // When user press a button, enable navigation by buttons
    if (!gui.is_nav_button) {
        gui.is_nav_button = true;

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
            if (current_menu < current_menu_size)
                current_menu = (current_menu + 1) % current_menu_size;
            break;
        case SCE_CTRL_CIRCLE:
            break;
        case SCE_CTRL_CROSS:
            handle_seting_menu_selection(gui, emuenv);
            break;
        }
    };

    switch (settings_menu) {
    case SettingsMenu::SELECT:
        browse_menu();
        break;
    case SettingsMenu::THEME_BACKGROUND:
        switch (menu) {
        case Menu::UNDEFINED:
            browse_menu();
            break;
        case Menu::THEME: {
            if (themes_list_filtered.empty())
                return;

            // When the current selected theme index have not any selected, set it to the first visible theme index
            if (current_selected_theme.empty()) {
                current_selected_theme = first_visible_theme;

                return;
            }

            const auto themes_list_filtered_size = static_cast<int32_t>(themes_list_filtered.size() - 1);
            // Find current selected app index in apps list filtered
            const auto current_selected_theme_it = std::find_if(themes_list_filtered.begin(), themes_list_filtered.end(), [&](const ThemeWithPosition &theme) {
                return theme.theme_id == current_selected_theme;
            });
            const int32_t current_selected_theme_index = (current_selected_theme_it != themes_list_filtered.end()) ? std::distance(themes_list_filtered.begin(), current_selected_theme_it) : -1;
            if (current_selected_theme_index == -1) {
                current_selected_theme = first_visible_theme;
                return;
            }

            const auto confirm = [&gui, &emuenv]() {
                if (selected.empty()) {
                    selected = current_selected_theme;
                    init_theme_preview(gui, emuenv, selected);
                } else if (selected != gui.users[emuenv.io.user_id].theme_id)
                    set_theme(gui, emuenv);
            };

            const auto prev_filtered_theme = themes_list_filtered[std::max(current_selected_theme_index - 1, 0)];
            const auto next_filtered_theme = themes_list_filtered[std::min(current_selected_theme_index + 1, themes_list_filtered_size)];

            const auto theme_info = themes_list_filtered[current_selected_theme_index];
            const auto column = theme_info.column;

            switch (button) {
            case SCE_CTRL_UP:
                if (current_selected_theme_index >= 3)
                    current_selected_theme = themes_list_filtered[current_selected_theme_index - 3].theme_id;
                break;
            case SCE_CTRL_RIGHT:
                if (column < 2)
                    current_selected_theme = next_filtered_theme.theme_id;
                break;
            case SCE_CTRL_DOWN:
                if ((current_selected_theme_index + 3) <= themes_list_filtered_size)
                    current_selected_theme = themes_list_filtered[current_selected_theme_index + 3].theme_id;
                break;
            case SCE_CTRL_LEFT:
                if ((current_selected_theme_index > 0) && (column > 0))
                    current_selected_theme = prev_filtered_theme.theme_id;
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
            break;
        }
        case Menu::START:
            break;
        case Menu::BACKGROUND:
            break;
        case Menu::DATE_FORMAT:
            break;
        case Menu::TIME_FORMAT:
            break;
        case Menu::SELECT_INPUT_LANG:
            break;
        default: break;
        }
        break;
    case SettingsMenu::DATE_TIME:
        break;
    case SettingsMenu::LANGUAGE:
        break;
    default: break;
    }
}

#ifdef __ANDROID__
bool copy_file_from_host(const fs::path &src, const fs::path &dst) {
    fs::create_directories(dst.parent_path());

    FILE *infile = host::dialog::filesystem::resolve_host_handle(src);
    if (!infile) {
        LOG_ERROR("Failed to open source file: {}", src.string());
        return false;
    }

    FILE *outfile = fopen(dst.string().c_str(), "wb");
    if (!outfile) {
        LOG_ERROR("Failed to create destination file: {}", dst.string());
        fclose(infile);
        return false;
    }

    constexpr size_t BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    size_t bytes_read = 0;

    while ((bytes_read = fread(buffer.data(), 1, BUFFER_SIZE, infile)) > 0) {
        fwrite(buffer.data(), 1, bytes_read, outfile);
    }

    fclose(infile);
    fclose(outfile);

    LOG_INFO("Successfully copied file from {} to {}", src.string(), dst.string());
    return true;
}
#endif

void draw_settings(GuiState &gui, EmuEnvState &emuenv) {
    static std::string title, delete_user_background, delete_theme;
    static ImGuiTextFilter search_bar;

    static float set_scroll_pos, current_scroll_pos, max_scroll_pos;

    enum class Popup {
        UNDEFINED,
        DEL,
        INFORMATION,
        SELECT_SYS_LANG
    };

    static Popup popup = Popup::UNDEFINED;

    enum class SubMenu {
        UNDEFINED,
        THEME,
        IMAGE,
        DEFAULT,
        SELECT_KEYBOARDS
    };
    static SubMenu sub_menu = SubMenu::UNDEFINED;

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

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

    auto &common = emuenv.common_dialog.lang.common;
    const auto BG_PATH = "vs0:app/NPXS10015";

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

    const auto draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (gui.apps_background.contains(BG_PATH))
        draw_list->AddImage(gui.apps_background[BG_PATH], VIEWPORT_POS, BG_POS_MAX);
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
    auto &date_time = lang.date_time;
    auto &language = lang.language;

    // Variables to manage the scroll animation
    static bool is_scroll_animating = false;
    static float scroll_y; // Initialize the scroll position
    static float target_scroll_y = 0.f; // Target scroll position

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
                    || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_up)))) {
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
                    || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_down)))) {
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

    const std::map<uint32_t, std::string> settings_list{
        { SettingsMenu::THEME_BACKGROUND, theme_background.main["title"] },
        { SettingsMenu::DATE_TIME, date_time.main["title"] },
        { SettingsMenu::LANGUAGE, language.main["title"] }
    };

    const ImVec2 MENU_SIZE(SIZE_LIST.x, SIZE_SELECT);
    const auto SCREEN_POS = ImGui::GetCursorScreenPos();

    const auto &DRAW_LIST = ImGui::GetWindowDrawList();
    const float rect_size = (6.f * SCALE.y);
    const float half_rect_size = rect_size / 2.f;
    const auto rounding = 15.f * SCALE.x;

    const ImU32 COLOR_PULSE_BORDER = get_selectable_color_pulse();
    const ImU32 COLOR_PULSE_MID = get_selectable_color_pulse(40.f);
    const ImU32 COLOR_PULSE_FILL = get_selectable_color_pulse(120.f);
    static ImVec2 animated_rectangle_pos{};

    const auto ImLerp = [](const ImVec2 &a, const ImVec2 &b, float t) {
        return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    };

    const auto draw_menu = [&](const std::map<uint32_t, std::string> menu_list) {
        for (auto i = 0; i < menu_list.size(); i++) {
            const auto menu = i;
            const auto title = menu_list.at(menu);
            const auto is_current_menu = current_menu == menu;
            const float INIT_MENU_POS(i * MENU_SIZE.y);
            const auto MENU_POS_MIN = ImVec2(SCREEN_POS.x, SCREEN_POS.y + INIT_MENU_POS);
            const auto MENU_POS_MAX = ImVec2(MENU_POS_MIN.x + SIZE_LIST.x, MENU_POS_MIN.y + MENU_SIZE.y);
            if (is_current_menu) {
                float speed = 1.f - std::exp(-10.f * ImGui::GetIO().DeltaTime);
                animated_rectangle_pos = ImLerp(animated_rectangle_pos, MENU_POS_MIN, speed);

                const ImVec2 rect_max(animated_rectangle_pos.x + MENU_SIZE.x,
                    animated_rectangle_pos.y + MENU_SIZE.y);

                const ImVec2 pos_min = animated_rectangle_pos;
                const ImVec2 pos_max = ImVec2(pos_min.x + MENU_SIZE.x, pos_min.y + MENU_SIZE.y);

                const float half_height = MENU_SIZE.y * 0.5f;
                const ImVec2 pos_mid = ImVec2(pos_min.x, pos_min.y + half_height);
                const ImVec2 pos_mid_max = ImVec2(pos_max.x, pos_min.y + half_height);

                DRAW_LIST->AddRectFilledMultiColor(
                    pos_min, pos_mid_max,
                    COLOR_PULSE_BORDER, COLOR_PULSE_BORDER,
                    COLOR_PULSE_MID, COLOR_PULSE_MID);

                DRAW_LIST->AddRectFilledMultiColor(
                    pos_mid, pos_max,
                    COLOR_PULSE_MID, COLOR_PULSE_MID,
                    COLOR_PULSE_BORDER, COLOR_PULSE_BORDER);
            }
            if (ImGui::InvisibleButton(title.c_str(), MENU_SIZE))
                handle_seting_menu_selection(gui, emuenv);
            if (ImGui::IsItemHovered())
                current_menu = menu;
            ImGui::SetCursorPos(ImVec2(100.f * SCALE.x, INIT_MENU_POS + (MENU_SIZE.y / 2.f) - (ImGui::GetFontSize() / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
            ImGui::SetCursorPos(ImVec2(0.f, INIT_MENU_POS + MENU_SIZE.y));
            ImGui::Separator();
        }
    };

    // Settings
    switch (settings_menu) {
    case SettingsMenu::SELECT:
        title = lang.main["title"];
        ImGui::SetWindowFontScale(1.2f);
        draw_menu(settings_list);
        break;
    case SettingsMenu::THEME_BACKGROUND: {
#ifdef __ANDROID__
        const auto remove_image = [&](const std::string &path) {
            if (!path.empty()) {
                const fs::path image_path = fs_utils::utf8_to_path(path);

                if (fs::exists(image_path)) {
                    fs::remove(image_path);
                    LOG_INFO("image removed: {}", path);
                }
            }
        };
#endif
        // Themes & Backgrounds
        const auto select = common["select"].c_str();
        switch (menu) {
        case Menu::UNDEFINED: {
            title = theme_background.main["title"];
            if (!themes_info.empty()) {
                ImGui::SetWindowFontScale(1.2f);

                const std::map<uint32_t, std::string> theme_background_list{
                    { Menu::THEME, theme_background.theme.main["title"] },
                    { Menu::START, theme_background.start_screen["title"] },
                    { Menu::BACKGROUND, theme_background.home_screen_backgrounds["title"] }
                };
                draw_menu(theme_background_list);
                ImGui::SetWindowFontScale(0.66f);
                // Draw the current selected theme title
                draw_ellipsis_text(themes_info[gui.users[emuenv.io.user_id].theme_id].title, 258.f * SCALE.x, ImVec2(SIZE_LIST.x - (60.f * SCALE.x), (MENU_SIZE.y / 2.f)), ImVec2(1.f, 0.f), GUI_COLOR_TEXT);
            }
            break;
        }
        case Menu::THEME: {
            // Theme List
            if (selected.empty()) {
                title = theme.main["title"];

                // Delete Theme
                if (!delete_theme.empty()) {
                    gui.themes_list.erase(delete_theme);
                    themes_info.erase(delete_theme);
                    const auto theme_list_index = std::find_if(themes_list.begin(), themes_list.end(), [&](const auto &t) {
                        return t.first == delete_theme;
                    });
                    themes_list.erase(theme_list_index);

                    // Update last updated themes list
                    last_updated_themes_list = std::time(0);

                    save_themes_cache(emuenv);
                    delete_theme.clear();
                }

                // Clear apps list filtered
                themes_list_filtered.clear();

                current_scroll_pos = ImGui::GetScrollY();
                max_scroll_pos = ImGui::GetScrollMaxY();

                // Apply the scroll animation
                if (is_scroll_animating)
                    is_scroll_animating = set_scroll_animation(scroll_y, target_scroll_y, current_selected_theme, ImGui::SetScrollY);

                const auto COLUMN_WIDTH = SIZE_LIST.x / 3.f;
                const auto MARGIN_BEGIN = 20.f * SCALE.x;
                const auto MARGIN_HEIGHT = 80.f * SCALE.y;
                uint32_t current_col = 0;
                uint32_t current_line = 0;
                const uint32_t max_cols = 3;
                const uint32_t max_lines = static_cast<uint32_t>(std::ceil(themes_list.size() / static_cast<float>(max_cols)) - 1);

                static ImVec2 animated_frame_pos{};
                static ImVec2 frame_target_pos{};
                const auto FRAME_SIZE = ImVec2(SIZE_PACKAGE.x + rect_size, SIZE_PACKAGE.y + rect_size);
                const auto set_scroll = [&](float desired_scroll) {
                    if (std::abs(desired_scroll - current_scroll_pos) > 1.f) {
                        target_scroll_y = desired_scroll;
                        is_scroll_animating = true;
                    }
                };

                std::vector<std::string> visible_themes{};
                for (const auto &[content_id, _] : themes_list) {
                    if (!search_bar.PassFilter(themes_info[content_id].title.c_str()) && !search_bar.PassFilter(content_id.c_str()))
                        continue;
                    ImGui::PushID(content_id.c_str());

                    themes_list_filtered.push_back({ content_id, current_line, current_col });

                    const auto is_current_theme = current_selected_theme == content_id;
                    const auto is_current_theme_selected = is_current_theme && gui.is_nav_button;

                    const ImVec2 INIT_THEME_POS(current_col * COLUMN_WIDTH, current_line * (SIZE_PACKAGE.y + MARGIN_HEIGHT));
                    const ImVec2 GRID_THEME_POS(rect_size + INIT_THEME_POS.x, MARGIN_BEGIN + rect_size + INIT_THEME_POS.y);

                    ImGui::SetCursorPos(GRID_THEME_POS);
                    if (ImGui::InvisibleButton("##preview", SIZE_PACKAGE)) {
                        selected = content_id;
                        init_theme_preview(gui, emuenv, selected);
                    }
                    if (ImGui::IsItemHovered())
                        current_selected_theme = content_id;

                    const auto ITEM_RECT_MAX = ImGui::GetItemRectMax().y;

                    const auto item_rect_min = ITEM_RECT_MAX - FRAME_SIZE.y;
                    const auto MAX_LIST_POS = POS_LIST.y + SIZE_LIST.y;

                    // Determine if the element is within the visible area of the window.
                    bool element_is_drawn = (ITEM_RECT_MAX >= POS_LIST.y) && (item_rect_min <= MAX_LIST_POS);

                    // Add the current app index to the visible apps list.
                    if (element_is_drawn && (item_rect_min >= POS_LIST.y) && (ITEM_RECT_MAX <= MAX_LIST_POS))
                        visible_themes.emplace_back(content_id);

                    const ImVec2 IMAGE_POS_MIN(SCREEN_POS.x + GRID_THEME_POS.x, SCREEN_POS.y + GRID_THEME_POS.y);
                    const ImVec2 IMAGE_POS_MAX(IMAGE_POS_MIN.x + SIZE_PACKAGE.x, IMAGE_POS_MIN.y + SIZE_PACKAGE.y);

                    const ImVec2 FRAME_POS_MIN(IMAGE_POS_MIN.x - half_rect_size, IMAGE_POS_MIN.y - half_rect_size);
                    const ImVec2 FRAME_POS_MAX(FRAME_POS_MIN.x + FRAME_SIZE.x, FRAME_POS_MIN.y + FRAME_SIZE.y);

                    if (is_current_theme_selected) {
                        const float item_center_y = item_rect_min + FRAME_SIZE.y * 0.5f;
                        const float list_center_y = POS_LIST.y + SIZE_LIST.y * 0.5f;

                        if (current_line == 0)
                            set_scroll(current_scroll_pos + item_rect_min - MARGIN_BEGIN - POS_LIST.y);
                        else if (current_line == max_lines)
                            set_scroll(max_scroll_pos);
                        else
                            set_scroll(current_scroll_pos + item_center_y - list_center_y);
                    }

                    if (is_current_theme && !is_scroll_animating)
                        frame_target_pos = FRAME_POS_MIN;

                    if (element_is_drawn) {
                        if (gui.themes_list.contains(content_id)) {
                            DRAW_LIST->AddImageRounded(gui.themes_list[content_id], IMAGE_POS_MIN, IMAGE_POS_MAX,
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), rounding);
                        } else if (std::find(gui.themes_package_async.begin(), gui.themes_package_async.end(), content_id) == gui.themes_package_async.end())
                            gui.themes_package_async.emplace_back(content_id);

                        // Draw the frame
                        DRAW_LIST->AddRect(FRAME_POS_MIN, FRAME_POS_MAX, IM_COL32(200.f, 200.f, 200.f, 80.f), rounding, ImDrawFlags_None, rect_size);

                        ImGui::SetWindowFontScale(0.6f);
                        draw_ellipsis_text(themes_info[content_id].title, FRAME_SIZE.x + rect_size, ImVec2(GRID_THEME_POS.x - rect_size, GRID_THEME_POS.y + FRAME_SIZE.y + ImGui::GetStyle().ItemSpacing.y), ImVec2(-1, 1), GUI_COLOR_TEXT);
                    }

                    ImGui::PopID();
                    ++current_col;
                    if (current_col >= max_cols) {
                        current_col = 0;
                        ++current_line;
                    }
                }

                const float speed = 1.f - std::exp(-10.f * ImGui::GetIO().DeltaTime);
                animated_frame_pos = ImLerp(animated_frame_pos, frame_target_pos, speed);

                ImVec2 rect_max(animated_frame_pos.x + FRAME_SIZE.x, animated_frame_pos.y + FRAME_SIZE.y);
                DRAW_LIST->AddRectFilled(animated_frame_pos, rect_max, COLOR_PULSE_FILL, rounding);
                DRAW_LIST->AddRect(animated_frame_pos, rect_max, COLOR_PULSE_BORDER, rounding, ImDrawFlags_None, rect_size);

                // When visible apps list is not empty, set first visible app
                if (!visible_themes.empty())
                    first_visible_theme = visible_themes.front();

                const ImVec2 DOWN_THEME_POS(rect_size + (current_col * COLUMN_WIDTH), MARGIN_BEGIN + rect_size + (current_line * (SIZE_PACKAGE.y + MARGIN_HEIGHT)));
                ImGui::SetCursorPos(DOWN_THEME_POS);
                if (ImGui::Selectable("##download_theme", true, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    open_path("https://psvt.ovh");
                ImGui::PushTextWrapPos(DOWN_THEME_POS.x + SIZE_PACKAGE.x + rect_size);
                ImGui::SetCursorPos(ImVec2(DOWN_THEME_POS.x, ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme.main["find_a_psvita_custom_themes"].c_str());
                ImGui::PopTextWrapPos();
                ImGui::ScrollWhenDragging();
            } else {
                // Theme Select
                title = themes_info[selected].title;
                switch (popup) {
                case Popup::UNDEFINED: {
                    if (gui.theme_preview.contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(15.f * SCALE.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.theme_preview[HOME], SIZE_PREVIEW);
                    }
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCALE.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.theme_preview[LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y));
                    if ((selected != gui.users[emuenv.io.user_id].theme_id) && ImGui::Button(select, BUTTON_SIZE)) {
#ifdef __ANDROID__
                        remove_image(gui.users[emuenv.io.user_id].start_path);
#endif
                        set_theme(gui, emuenv);
                        set_scroll_pos = current_scroll_pos;
                    }
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
                    ImGui::Image(gui.themes_list[selected], SIZE_MINI_PACKAGE);
                    ImGui::SameLine(0, 22.f * SCALE.x);
                    const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[selected].title.c_str(), nullptr, false, POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - (70.f * SCALE.x)).y / 2.f;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_MINI_PACKAGE.y / 2.f) - CALC_TITLE);
                    ImGui::TextWrapped("%s", themes_info[selected].title.c_str());
                    ImGui::SetCursorPosY(POPUP_SIZE.y / 2.f);
                    TextColoredCentered(GUI_COLOR_TEXT, theme.main["delete"].c_str());
                    ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
                    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
                        popup = Popup::UNDEFINED;
                    ImGui::SameLine(0, 40.f * SCALE.x);
                    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                        if (selected == gui.users[emuenv.io.user_id].theme_id) {
                            gui.users[emuenv.io.user_id].theme_id = "default";
#ifdef __ANDROID__
                            remove_image(gui.users[emuenv.io.user_id].start_path);
#endif
                            gui.users[emuenv.io.user_id].start_path.clear();
                            if (init_theme(gui, emuenv, "default"))
                                gui.users[emuenv.io.user_id].use_theme_bg = true;
                            else
                                gui.users[emuenv.io.user_id].use_theme_bg = false;
                            save_user(gui, emuenv, emuenv.io.user_id);
                            init_theme_start_background(gui, emuenv, "default");
                        }
                        fs::remove_all(emuenv.pref_path / "ux0/theme" / fs_utils::utf8_to_path(selected));
                        last_updated_themes_list = time(0);
                        if (emuenv.app_path == "NPXS10026")
                            init_content_manager(gui, emuenv);
                        delete_theme = selected;
                        popup = Popup::UNDEFINED;
                        selected.clear();
                        set_scroll_pos = current_scroll_pos;
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::End();
                    break;
                }
                case Popup::INFORMATION: {
                    if (gui.theme_preview.contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(119.f * SCALE.x, 4.f * SCALE.y));
                        ImGui::Image(gui.theme_preview[HOME], SIZE_MINI_PREVIEW);
                    }
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2(SIZE_LIST.x / 2.f + (15.f * SCALE.y), 4.f * SCALE.y));
                        ImGui::Image(gui.theme_preview[LOCK], SIZE_MINI_PREVIEW);
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
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                const auto PACKAGE_POS_Y = (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (72.f * SCALE.y);
                const auto is_not_default = gui.users[emuenv.io.user_id].theme_id != "default";
                if (is_not_default) {
                    const auto THEME_POS = ImVec2(15.f * SCALE.x, PACKAGE_POS_Y);
                    if (gui.themes_list.contains(gui.users[emuenv.io.user_id].theme_id)) {
                        ImGui::SetCursorPos(THEME_POS);
                        ImGui::Image(gui.themes_list[gui.users[emuenv.io.user_id].theme_id], SIZE_PACKAGE);
                    }
                    ImGui::SetCursorPos(THEME_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                        sub_menu = SubMenu::THEME;
                        init_theme_preview(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
                    }
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(15.f * SCALE.x);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[gui.users[emuenv.io.user_id].theme_id].title.c_str());
                    ImGui::PopTextWrapPos();
                }
                const auto IMAGE_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f) : 15.f * SCALE.x, PACKAGE_POS_Y);
                if ((gui.users[emuenv.io.user_id].start_type == "image") && gui.start_background) {
                    ImGui::SetCursorPos(IMAGE_POS);
                    ImGui::Image(gui.start_background, SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(IMAGE_POS);
                if (gui.users[emuenv.io.user_id].start_type == "image")
                    ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "image" ? "V" : theme_background.start_screen["add_image"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    sub_menu = SubMenu::IMAGE;
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(IMAGE_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.start_screen["image"].c_str());
                const auto DEFAULT_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCALE.y) : (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), PACKAGE_POS_Y);
                if (gui.themes_list.contains("default")) {
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::Image(gui.themes_list["default"], SIZE_PACKAGE);
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                        sub_menu = SubMenu::DEFAULT;
                        init_theme_preview(gui, emuenv, "default");
                    }
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(DEFAULT_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.main["default"].c_str());
                }
                ImGui::PopStyleVar();
                ImGui::SetWindowFontScale(0.90f);
            } else {
                const auto START_PREVIEW_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y));
                const auto SELECT_BUTTON_POS = ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y);
                if (sub_menu == SubMenu::THEME) {
                    title = themes_info[gui.users[emuenv.io.user_id].theme_id].title;
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.theme_preview[LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((gui.users[emuenv.io.user_id].start_type != "theme") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))) {
#ifdef __ANDROID__
                        remove_image(gui.users[emuenv.io.user_id].start_path);
#endif
                        gui.users[emuenv.io.user_id].start_path.clear();
                        gui.users[emuenv.io.user_id].start_type = "theme";
                        init_theme_start_background(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
                        save_user(gui, emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                    }
                } else if (sub_menu == SubMenu::IMAGE) {
                    fs::path image_path{};
                    host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(image_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });

                    if (result == host::dialog::filesystem::Result::SUCCESS) {
                        const fs::path old_path = !gui.users[emuenv.io.user_id].start_path.empty()
                            ? fs_utils::utf8_to_path(gui.users[emuenv.io.user_id].start_path)
                            : fs::path{};
#ifdef __ANDROID__
                        const fs::path image_dst_path = emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "image" / image_path.filename();
                        if (copy_file_from_host(image_path, image_dst_path))
                            image_path = image_dst_path;
#endif
                        if (fs::exists(image_path) && init_user_start_background(gui, fs_utils::path_to_utf8(image_path))) {
                            gui.users[emuenv.io.user_id].start_path = fs_utils::path_to_utf8(image_path);
                            gui.users[emuenv.io.user_id].start_type = "image";
                            save_user(gui, emuenv, emuenv.io.user_id);
                        }
#ifdef __ANDROID__
                        // Remove old image if different
                        if (!old_path.empty() && (old_path != image_path)) {
                            fs::remove(old_path);
                            LOG_INFO("Old start image removed: {}", old_path);
                        }
#endif
                    } else if (result == host::dialog::filesystem::Result::ERROR) {
                        LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                    }
                    sub_menu = SubMenu::UNDEFINED;
                } else if (sub_menu == SubMenu::DEFAULT) {
                    title = theme_background.main["default"];
                    if (gui.theme_preview.contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.theme_preview[LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((gui.users[emuenv.io.user_id].start_type != "default") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))) {
#ifdef __ANDROID__
                        remove_image(gui.users[emuenv.io.user_id].start_path);
#endif
                        gui.users[emuenv.io.user_id].start_path.clear();
                        init_theme_start_background(gui, emuenv, "default");
                        gui.users[emuenv.io.user_id].start_type = "default";
                        save_user(gui, emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                    }
                }
            }
            break;
        }
        case Menu::BACKGROUND: {
            title = theme_background.home_screen_backgrounds["title"];

            // Delete user background
            if (!delete_user_background.empty()) {
#ifdef __ANDROID__
                // Remove background image file if it exists
                remove_image(delete_user_background);
#endif
                vector_utils::erase_first(gui.users[emuenv.io.user_id].backgrounds, delete_user_background);
                gui.user_backgrounds.erase(delete_user_background);
                gui.user_backgrounds_infos.erase(delete_user_background);
                if (!gui.users[emuenv.io.user_id].backgrounds.empty())
                    gui.current_user_bg = gui.current_user_bg % gui.user_backgrounds.size();
                else if (!gui.theme_backgrounds.empty())
                    gui.users[emuenv.io.user_id].use_theme_bg = true;
                save_user(gui, emuenv, emuenv.io.user_id);
                delete_user_background.clear();
            }

            ImGui::SetWindowFontScale(0.80f);
            ImGui::Columns(3, nullptr, false);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
            for (const auto &background : gui.users[emuenv.io.user_id].backgrounds) {
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
#ifdef __ANDROID__
                    const fs::path bg_dst_path = emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "background" / background_path.filename();
                    if (copy_file_from_host(background_path, bg_dst_path))
                        background_path = bg_dst_path;
#endif
                    auto background_path_string = fs_utils::path_to_utf8(background_path);

                    if (!gui.user_backgrounds.contains(background_path_string) && fs::exists(background_path) && init_user_background(gui, emuenv, background_path_string)) {
                        gui.users[emuenv.io.user_id].backgrounds.push_back(background_path_string);
                        gui.users[emuenv.io.user_id].use_theme_bg = false;
                        save_user(gui, emuenv, emuenv.io.user_id);
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
            ImGui::SetWindowFontScale(1.2f);
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
                for (const auto &sys_lang : LIST_SYS_LANG) {
                    ImGui::PushID(sys_lang.first);
                    const auto is_current_lang = emuenv.cfg.sys_lang == sys_lang.first;
                    if (ImGui::Selectable(is_current_lang ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT))) {
                        if (!is_current_lang) {
                            emuenv.cfg.sys_lang = sys_lang.first;
                            config::serialize_config(emuenv.cfg, emuenv.config_path);
                            lang::init_lang(gui.lang, emuenv);
                            if (sys_lang.first != gui.app_selector.apps_cache_lang) {
                                std::thread init_app([&gui, &emuenv]() {
                                    get_sys_apps_title(gui, emuenv);
                                    get_user_apps_title(gui, emuenv);
                                    init_last_time_apps(gui, emuenv);
                                });
                                init_app.detach();
                            }
                            const auto live_area_state = get_live_area_current_open_apps_list_index(gui, "emu0:app/NPXS10015") != gui.live_area_current_open_apps_list.end();
                            gui.live_area_current_open_apps_list.clear();
                            gui.live_area_contents.clear();
                            gui.live_items.clear();
                            destroy_download_manager();
                            init_notice_info(gui, emuenv);
                            if (live_area_state) {
                                update_live_area_current_open_apps_list(gui, emuenv, "emu0:app/NPXS10015");
                                init_live_area(gui, emuenv, "emu0:app/NPXS10015");
                            }
                        }
                        popup = Popup::UNDEFINED;
                    }
                    ImGui::NextColumn();
                    ImGui::Selectable(sys_lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
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
                ImGui::SetColumnWidth(0, 600.f * emuenv.manual_dpi_scale);
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
                    ImGui::SetColumnWidth(0, 40.f * emuenv.manual_dpi_scale);
                    ImGui::SetColumnWidth(1, 560.f * emuenv.manual_dpi_scale);
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
    if (ImGui::Button("<<", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
        if (settings_menu != SettingsMenu::SELECT) {
            if (menu != Menu::UNDEFINED) {
                if (!selected.empty()) {
                    if (popup != Popup::UNDEFINED)
                        popup = Popup::UNDEFINED;
                    else {
                        selected.clear();
                        set_scroll_pos = current_scroll_pos;
                    }
                } else if (sub_menu != SubMenu::UNDEFINED)
                    sub_menu = SubMenu::UNDEFINED;
                else
                    menu = Menu::UNDEFINED;
            } else if (popup != Popup::UNDEFINED)
                popup = Popup::UNDEFINED;
            else
                settings_menu = SettingsMenu::SELECT;
        } else
            close_system_app(gui, emuenv);
    }

    if ((settings_menu == SettingsMenu::THEME_BACKGROUND) && !selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (70.f * SCALE.x), WINDOW_SIZE.y - (52.f * SCALE.y)));
        if (((popup != Popup::INFORMATION) && ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y))) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...")) {
            if (ImGui::MenuItem(theme.information["title"].c_str()))
                popup = Popup::INFORMATION;
            if (ImGui::MenuItem(common["delete"].c_str()))
                popup = Popup::DEL;
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
