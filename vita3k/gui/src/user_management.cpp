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

#include <ctrl/ctrl.h>

#include <config/functions.h>
#include <config/state.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <io/state.h>
#include <np/common.h>

#include <util/log.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <pugixml.hpp>
#include <stb_image.h>

#undef ERROR

namespace gui {

enum AvatarSize {
    SMALL = 34,
    MEDIUM = 130,
    LARGE = 160
};

static ImVec2 get_avatar_size(AvatarSize size, const ImVec2 scale = { 1, 1 }) {
    return ImVec2(static_cast<float>(size) * scale.x, static_cast<float>(size) * scale.y);
}

struct AvatarInfo {
    ImVec2 pos;
    ImVec2 size;
};

static std::map<std::string, std::map<AvatarSize, AvatarInfo>> users_avatar_infos;
static bool init_avatar(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id, const std::string &avatar_path) {
    const auto avatar_path_path = avatar_path == "default" ? emuenv.static_assets_path / "data/image/icon.png" : fs_utils::utf8_to_path(avatar_path);

    std::vector<uint8_t> avatar_data{};
    if (!fs_utils::read_data(avatar_path_path, avatar_data)) {
        LOG_WARN("Avatar image doesn't exist: {}.", avatar_path_path);
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;

    stbi_uc *data = stbi_load_from_memory(avatar_data.data(), avatar_data.size(), &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", avatar_path_path);
        return false;
    }

    gui.users_avatar[user_id] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    // Calculate avatar size and position based of aspect ratio
    // Resize for all size of avatar
    constexpr std::array<AvatarSize, 3> sizes = { SMALL, MEDIUM, LARGE };
    for (const auto size : sizes) {
        const auto avatar_size = get_avatar_size(size);
        const auto ratio = std::min(avatar_size.x / static_cast<float>(width), avatar_size.y / static_cast<float>(height));
        auto &avatar = users_avatar_infos[user_id][size];
        avatar.size = ImVec2(width * ratio, height * ratio);
        avatar.pos = ImVec2((avatar_size.x / 2.f) - (avatar.size.x / 2.f), (avatar_size.y / 2.f) - (avatar.size.y / 2.f));
    }

    return gui.users_avatar.contains(user_id);
}

void get_users_list(GuiState &gui, EmuEnvState &emuenv) {
    gui.users.clear();
    const auto user_path{ emuenv.pref_path / "ux0/user" };
    if (fs::exists(user_path) && !fs::is_empty(user_path)) {
        for (const auto &path : fs::directory_iterator(user_path)) {
            pugi::xml_document user_xml;
            if (fs::is_directory(path) && user_xml.load_file((path / "user.xml").c_str())) {
                const auto user_child = user_xml.child("user");

                // Load user id
                std::string user_id;
                if (!user_child.attribute("id").empty())
                    user_id = user_child.attribute("id").as_string();
                else
                    user_id = path.path().stem().string();

                // Load user name
                auto &user = gui.users[user_id];
                user.id = user_id;
                if (!user_child.attribute("name").empty())
                    user.name = user_child.attribute("name").as_string();

                // Load Avatar
                if (!user_child.child("avatar").text().empty())
                    user.avatar = user_child.child("avatar").text().as_string();
                init_avatar(gui, emuenv, user.id, user.avatar);

                // Load sort Apps list settings
                auto sort_apps_list = user_child.child("sort-apps-list");
                if (!sort_apps_list.empty()) {
                    user.sort_apps_type = static_cast<SortType>(sort_apps_list.attribute("type").as_uint());
                    user.sort_apps_state = static_cast<SortState>(sort_apps_list.attribute("state").as_uint());
                }

                // Load theme settings
                auto theme = user_child.child("theme");
                if (!theme.attribute("use-background").empty())
                    user.use_theme_bg = theme.attribute("use-background").as_bool();

                if (!theme.child("content-id").text().empty())
                    user.theme_id = theme.child("content-id").text().as_string();

                // Load start screen settings
                auto start = user_child.child("start-screen");
                if (!start.attribute("type").empty())
                    user.start_type = start.attribute("type").as_string();

                if (!start.child("path").text().empty())
                    user.start_path = start.child("path").text().as_string();

                // Load backgrounds path
                for (const auto &bg : user_child.child("backgrounds"))
                    user.backgrounds.emplace_back(bg.text().as_string());
            }
        }
    }
}

void save_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id) {
    const auto user_path{ emuenv.pref_path / "ux0/user" / user_id };
    fs::create_directories(user_path);

    const auto &user = gui.users[user_id];

    pugi::xml_document user_xml;
    auto declarationUser = user_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    // Save user settings
    auto user_child = user_xml.append_child("user");
    user_child.append_attribute("id") = user.id.c_str();
    user_child.append_attribute("name") = user.name.c_str();
    user_child.append_child("avatar").append_child(pugi::node_pcdata).set_value(user.avatar.c_str());

    // Save sort Apps list settings
    auto sort_apps_list = user_child.append_child("sort-apps-list");
    sort_apps_list.append_attribute("type") = user.sort_apps_type;
    sort_apps_list.append_attribute("state") = user.sort_apps_state;

    // Save theme settings
    auto theme = user_child.append_child("theme");
    theme.append_attribute("use-background") = user.use_theme_bg;
    theme.append_child("content-id").append_child(pugi::node_pcdata).set_value(user.theme_id.c_str());

    // Save start screen settings
    auto start_screen = user_child.append_child("start-screen");
    start_screen.append_attribute("type") = user.start_type.c_str();
    start_screen.append_child("path").append_child(pugi::node_pcdata).set_value(user.start_path.c_str());

    // Save backgrounds path
    auto bg_path = user_child.append_child("backgrounds");
    for (const auto &bg : user.backgrounds)
        bg_path.append_child("background").append_child(pugi::node_pcdata).set_value(bg.c_str());

    const auto save_xml = user_xml.save_file((user_path / "user.xml").c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml for user id: {}, name: {}, in path: {}", user.id, user.name, user_path);
}

enum UserMenu {
    CREATE,
    SELECT,
    EDIT,
    DELETE_USER,
    CONFIRM,
};

static uint32_t current_user_id_selected = 0;
static UserMenu menu_selected = SELECT, menu = SELECT;

void init_user_management(GuiState &gui, EmuEnvState &emuenv) {
    init_app_background(gui, emuenv, "NPXS10013");
    gui.vita_area.home_screen = false;
    gui.vita_area.information_bar = false;
    gui.vita_area.user_management = true;
    if (!emuenv.cfg.user_id.empty())
        current_user_id_selected = string_utils::stoi_def(emuenv.cfg.user_id, 0, "cfg user id");
    else if (!gui.users.empty())
        current_user_id_selected = string_utils::stoi_def(gui.users.begin()->first, 0, "gui user id");
    else
        menu_selected = CREATE;
}

void init_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id) {
    emuenv.io.user_id = user_id;
    emuenv.io.user_name = gui.users[user_id].name;
    init_user_backgrounds(gui, emuenv);
    init_theme(gui, emuenv, gui.users[user_id].theme_id);
    init_notice_info(gui, emuenv);
    init_last_time_apps(gui, emuenv);
}

void open_user(GuiState &gui, EmuEnvState &emuenv) {
    gui.vita_area.user_management = false;

    if (gui.users[emuenv.io.user_id].start_type == "image")
        init_user_start_background(gui, gui.users[emuenv.io.user_id].start_path);
    else
        init_theme_start_background(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);

    gui.vita_area.information_bar = true;
    gui.vita_area.start_screen = true;

#ifdef USE_VITA3K_UPDATE
    if (emuenv.cfg.check_for_updates) {
        std::thread update_vita3k_thread([&gui]() {
            if (init_vita3k_update(gui))
                gui.help_menu.vita3k_update = true;
        });
        update_vita3k_thread.detach();
    }
#endif
}

static auto get_users_index(GuiState &gui, const std::string &user_name) {
    const auto profils_index = std::find_if(gui.users.begin(), gui.users.end(), [&](const auto &u) {
        return u.second.name == user_name;
    });

    return profils_index;
}

static std::string del_menu, title, user_id_selected;
static User temp;
static std::vector<uint32_t> users_list_available;

static void create_temp_user(GuiState &gui, EmuEnvState &emuenv) {
    menu = CREATE;
    auto id = 0;
    for (const auto &user : gui.users) {
        if (id != string_utils::stoi_def(user.first, 0, "gui user id"))
            break;
        else
            ++id;
    }
    user_id_selected = fmt::format("{:0>2d}", id);
    auto i = 1;
    const auto USER_STR = gui.lang.user_management["user"];
    for (const auto &user : gui.users) {
        if (get_users_index(gui, USER_STR + std::to_string(i)) == gui.users.end())
            break;
        else
            ++i;
    }
    temp.id = user_id_selected;
    temp.name = USER_STR + std::to_string(i);
    temp.avatar = "default";
    temp.theme_id = "default";
    temp.use_theme_bg = true;
    temp.start_type = "default";
    init_avatar(gui, emuenv, "temp", "default");
}

static void clear_user_temp(GuiState &gui) {
    temp = {};
    gui.users_avatar["temp"] = {};
    gui.users_avatar.erase("temp");
    user_id_selected.clear();
}

static void create_and_save_user(GuiState &gui, EmuEnvState &emuenv) {
    // Assign temp user data
    gui.users[user_id_selected] = temp;

    // Move avatar texture + infos
    gui.users_avatar[user_id_selected] = std::move(gui.users_avatar["temp"]);
    users_avatar_infos[user_id_selected] = users_avatar_infos["temp"];
    current_user_id_selected = string_utils::stoi_def(user_id_selected, 0, "selected user id");
    save_user(gui, emuenv, user_id_selected);

    if (menu == CREATE)
        menu = CONFIRM;
    else {
        clear_user_temp(gui);
        menu = SELECT;
    }
}

static void select_and_open_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id) {
    if (emuenv.io.user_id != user_id)
        init_user(gui, emuenv, user_id);
    if (emuenv.cfg.user_id != user_id) {
        emuenv.cfg.user_id = user_id;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    open_user(gui, emuenv);
}

static void delete_user(GuiState &gui, EmuEnvState &emuenv) {
    const auto user_path{ emuenv.pref_path / "ux0/user" };
    fs::remove_all(user_path / user_id_selected);
    gui.users_avatar.erase(user_id_selected);
    gui.users.erase(get_users_index(gui, gui.users[user_id_selected].name));
    if (gui.users.empty() || (user_id_selected == emuenv.io.user_id)) {
        emuenv.cfg.user_id.clear();
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        emuenv.io.user_id.clear();
    }
    del_menu = "confirm";
}

void browse_users_management(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    const auto users_list_available_size = static_cast<int32_t>(users_list_available.size() - 1);

    // Find current selected user index in users list
    const int32_t available_index = vector_utils::find_index(users_list_available, current_user_id_selected);
    const auto is_empty = users_list_available.empty();
    const auto first_user_id_available = is_empty ? 0 : users_list_available.front();
    // When user press a button, enable navigation by buttons
    if (!gui.is_nav_button) {
        gui.is_nav_button = true;

        if (menu_selected == SELECT) {
            if (gui.users.empty())
                menu_selected = CREATE;
            else if (available_index == -1)
                // Set first index of users list if current index is invalid
                current_user_id_selected = first_user_id_available;
        }

        return;
    }

    const auto prev_available_index = is_empty ? 0 : users_list_available[std::max(available_index - 1, 0)];
    const auto next_available_index = is_empty ? 0 : users_list_available[std::min(available_index + 1, users_list_available_size)];

    const auto current_user_selected_str = fmt::format("{:0>2d}", current_user_id_selected);

    switch (button) {
    case SCE_CTRL_UP:
        if (menu == DELETE_USER)
            current_user_id_selected = prev_available_index;
        else if (menu_selected == SELECT)
            menu_selected = EDIT;
        break;
    case SCE_CTRL_RIGHT:
        if (menu == SELECT) {
            if ((menu_selected == CREATE) && !gui.users.empty())
                menu_selected = SELECT;
            else if ((menu_selected == SELECT) || (menu_selected == EDIT)) {
                if (current_user_id_selected == next_available_index)
                    menu_selected = DELETE_USER;
                else
                    current_user_id_selected = next_available_index;
            }
        }
        break;
    case SCE_CTRL_DOWN:
        if (menu == DELETE_USER)
            current_user_id_selected = next_available_index;
        else if (menu_selected == EDIT)
            menu_selected = SELECT;
        break;
    case SCE_CTRL_LEFT:
        if (menu == SELECT) {
            if ((menu_selected == SELECT) || (menu_selected == EDIT)) {
                if (current_user_id_selected == first_user_id_available)
                    menu_selected = CREATE;
                else
                    current_user_id_selected = prev_available_index;
            } else if (menu_selected == DELETE_USER)
                menu_selected = SELECT;
        }
        break;
    case SCE_CTRL_CIRCLE: {
        const auto USER_ALREADY_INIT = !gui.users.empty() && !emuenv.io.user_id.empty() && (emuenv.cfg.user_id == emuenv.io.user_id);
        if (menu != SELECT) {
            if ((menu == CREATE) || (menu == EDIT))
                clear_user_temp(gui);
            if ((menu == DELETE_USER) && !user_id_selected.empty()) {
                user_id_selected.clear();
                del_menu.clear();
            } else {
                menu = SELECT;
                if (menu_selected == CREATE)
                    current_user_id_selected = first_user_id_available;
                else if (menu_selected == DELETE_USER)
                    current_user_id_selected = users_list_available.back();
            }
        } else if (USER_ALREADY_INIT) {
            gui.vita_area.user_management = false;
            gui.vita_area.home_screen = true;
        }

        break;
    }
    case SCE_CTRL_CROSS:
        switch (menu) {
        case CREATE:
        case EDIT: {
            const auto free_name = get_users_index(gui, temp.name) == gui.users.end();
            const auto check_free_name = (menu == CREATE ? free_name : (temp.name == gui.users[user_id_selected].name) || free_name);
            if (check_free_name)
                create_and_save_user(gui, emuenv);
            break;
        }
        case SELECT:
            if (menu_selected == CREATE)
                create_temp_user(gui, emuenv);
            else if (menu_selected == SELECT)
                select_and_open_user(gui, emuenv, current_user_selected_str);
            else if (menu_selected == EDIT) {
                user_id_selected = current_user_selected_str;
                init_avatar(gui, emuenv, "temp", gui.users[current_user_selected_str].avatar);
                temp = gui.users[current_user_selected_str];
                menu = EDIT;
            } else if (menu_selected == DELETE_USER) {
                current_user_id_selected = first_user_id_available;
                menu = DELETE_USER;
            }
            break;
        case DELETE_USER:
            if (del_menu.empty()) {
                user_id_selected = current_user_selected_str;
                del_menu = "warn";
            } else if (del_menu == "warn")
                delete_user(gui, emuenv);
            else if (del_menu == "confirm") {
                del_menu.clear();
                user_id_selected.clear();
                current_user_id_selected = first_user_id_available;
                if (gui.users.empty()) {
                    menu = SELECT;
                    menu_selected = CREATE;
                }
            }
            break;
        case CONFIRM:
            clear_user_temp(gui);
            menu = menu_selected = SELECT;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void draw_user_management(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 WINDOW_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    // Clear users list available
    users_list_available.clear();

    const ImVec2 WINDOW_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##user_management", &gui.vita_area.user_management, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize, IM_COL32(0.f, 0.f, 0.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const ImVec2 WINDOW_POS_MAX(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y);
    if (gui.apps_background.contains("NPXS10013"))
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10013"], WINDOW_POS, WINDOW_POS_MAX);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(WINDOW_POS, WINDOW_POS_MAX, IM_COL32(10.f, 50.f, 140.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const auto user_path{ emuenv.pref_path / "ux0/user" };

    const auto SMALL_AVATAR_SIZE = get_avatar_size(SMALL, SCALE);
    const auto MED_AVATAR_SIZE = get_avatar_size(MEDIUM, SCALE);
    const auto LARGE_AVATAR_SIZE = get_avatar_size(LARGE, SCALE);
    const auto USER_NAME_BG_SIZE = ImVec2(MED_AVATAR_SIZE.x, 60.f * SCALE.y);

    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    const auto calc_title = ImGui::CalcTextSize(title.c_str()).y / 2.f;
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, (32.f * SCALE.y) - calc_title));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());

    const auto SIZE_USER = ImVec2(WINDOW_SIZE.x, 410.f * SCALE.y);
    const auto POS_SEPARATOR = 66.f * SCALE.y;
    const auto SPACE_AVATAR = MED_AVATAR_SIZE.x + (20.f * SCALE.x);

    ImGui::SetCursorPosY(POS_SEPARATOR);
    ImGui::Separator();

    if (menu == SELECT)
        ImGui::SetNextWindowContentSize(ImVec2(SIZE_USER.x + (!gui.users.empty() ? ((gui.users.size() + 1) * SPACE_AVATAR) : 0.f), 0.0f));
    ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x, WINDOW_POS.y + (68.f * SCALE.y)), ImGuiCond_Always);
    auto flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::BeginChild("##user_child", SIZE_USER, ImGuiChildFlags_None, flags);

    // Draw user background
    const auto draw_user_bg = [&](const AvatarSize size, const ImVec2 origin_pos) {
        ImGui::SetCursorPos(origin_pos);
        const auto SCREEN_POS = ImGui::GetCursorScreenPos();
        const auto BG_AVATAR_SIZE = get_avatar_size(size, SCALE);
        const auto &DRAW_LIST = ImGui::GetWindowDrawList();
        DRAW_LIST->AddRectFilled(SCREEN_POS, ImVec2(SCREEN_POS.x + BG_AVATAR_SIZE.x, SCREEN_POS.y + BG_AVATAR_SIZE.y), IM_COL32(19.f, 69.f, 167.f, 150.f));
        if (size == MEDIUM) {
            ImGui::SetCursorPos(ImVec2(origin_pos.x, origin_pos.y + MED_AVATAR_SIZE.y));
            DRAW_LIST->AddRectFilled(SCREEN_POS, ImVec2(SCREEN_POS.x + BG_AVATAR_SIZE.x, SCREEN_POS.y + USER_NAME_BG_SIZE.y), IM_COL32(19.f, 69.f, 167.f, 255.f));
        }
    };

    // Draw user avatar
    const auto draw_avatar = [&](const std::string &user_id, const AvatarSize size, const ImVec2 origin_pos) {
        draw_user_bg(size, origin_pos);
        if (gui.users_avatar.contains(user_id)) {
            const auto &user_avatar_infos = users_avatar_infos[user_id][size];
            ImVec2 AVATAR_POS = ImVec2(origin_pos.x + (user_avatar_infos.pos.x * SCALE.x), origin_pos.y + (user_avatar_infos.pos.y * SCALE.y));
            ImVec2 AVATAR_SIZE = ImVec2(user_avatar_infos.size.x * SCALE.x, user_avatar_infos.size.y * SCALE.y);
            ImGui::SetCursorPos(AVATAR_POS);
            ImGui::Image(gui.users_avatar[user_id], AVATAR_SIZE);
        }
    };

    const auto SCREEN_POS = ImGui::GetCursorScreenPos();

    // Draw frame around the selected user
    const auto draw_frame = [&](const ImVec2 &pos, const ImVec2 &size) {
        const auto thickness = 3.f * SCALE.x;
        const auto half_thickness = thickness / 2.f;
        const auto USER_POS_MIN = ImVec2(SCREEN_POS.x + pos.x - half_thickness, SCREEN_POS.y + pos.y - half_thickness);
        const auto USER_POS_MAX = ImVec2(USER_POS_MIN.x + size.x + thickness, USER_POS_MIN.y + size.y + thickness);
        ImGui::GetWindowDrawList()->AddRect(USER_POS_MIN, USER_POS_MAX, IM_COL32(255.f, 255.f, 255.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll, thickness);
    };

    const auto AVATAR_POS = ImVec2((SIZE_USER.x / 2) - (((menu == CREATE) || (menu == EDIT) ? LARGE_AVATAR_SIZE.x : MED_AVATAR_SIZE.x) / 2.f), ((menu == CREATE) || (menu == EDIT)) ? 46.f * SCALE.y : (SIZE_USER.y / 2) - (MED_AVATAR_SIZE.y / 2.f));
    const ImVec2 CREATE_USER_POS((SIZE_USER.x / 2) - (MED_AVATAR_SIZE.x / 2.f), (SIZE_USER.y / 2) - (MED_AVATAR_SIZE.y / 2.f));
    const ImVec2 DELETE_USER_POS(AVATAR_POS.x + ((gui.users.size() + 1) * SPACE_AVATAR), AVATAR_POS.y);
    const ImVec2 SELECTABLE_USER_SIZE(MED_AVATAR_SIZE.x, MED_AVATAR_SIZE.y + USER_NAME_BG_SIZE.y);
    const auto HALF_SIZE_USER = SIZE_USER.x / 2.f;

    const auto BUTTON_SIZE = ImVec2(220 * SCALE.x, 36 * SCALE.y);
    const auto BUTTON_POS = ImVec2((SIZE_USER.x / 2.f) - (BUTTON_SIZE.x / 2.f), SIZE_USER.y - BUTTON_SIZE.y - (28.f * SCALE.y));
    const auto TEXT_USER_PADDING = MED_AVATAR_SIZE.x + (5.f * SCALE.x);
    const auto USER_NAME_PADDING = 10.f * SCALE.x;
    auto &lang = gui.lang.user_management;
    auto &common = emuenv.common_dialog.lang.common;

    static bool is_scroll_animating = false; // Flag to indicate if the scroll animation is in progress
    static float target_scroll_x = 0.f; // Target scroll position
    static float scroll_x = 0.f; // Initialize the scroll position

    if (is_scroll_animating) {
        std::string current_id;
        switch (menu_selected) {
        case CREATE:
            current_id = "create";
            break;
        case SELECT:
        case EDIT:
            current_id = std::to_string(current_user_id_selected);
            break;
        case DELETE_USER:
            current_id = "delete";
            break;
        default:
            break;
        }

        // Update the scroll position towards the target position
        is_scroll_animating = set_scroll_animation(scroll_x, target_scroll_x, current_id, ImGui::SetScrollX);
    }

    const auto trigger_scroll_to_item = [&](bool should_scroll) {
        if (should_scroll) {
            const auto user_item_rect_half = ((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) / 2.f) - WINDOW_POS.x;
            if (std::abs(user_item_rect_half - HALF_SIZE_USER) > 1.0f) {
                target_scroll_x = ImGui::GetScrollX() + (user_item_rect_half - HALF_SIZE_USER);
                is_scroll_animating = true;
            }
        }
    };

    const auto draw_centered_label = [&](const std::string &str, const ImVec2 &pos, const ImVec2 &size) {
        const auto STR_SIZE = ImGui::CalcTextSize(str.c_str());
        ImGui::SetCursorPos(ImVec2(pos.x + (size.x / 2.f) - (STR_SIZE.x / 2.f), pos.y + (size.y / 2.f) - (STR_SIZE.y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", str.c_str());
    };

    switch (menu) {
    case SELECT: {
        // Users list
        title = lang["select_user"];
        ImGui::SetWindowFontScale(1.6f);
        draw_user_bg(MEDIUM, CREATE_USER_POS);
        ImGui::SetCursorPos(CREATE_USER_POS);
        if (ImGui::InvisibleButton("+", SELECTABLE_USER_SIZE)) {
            if (menu_selected == CREATE)
                create_temp_user(gui, emuenv);
            else
                menu_selected = CREATE;
        }
        draw_centered_label("+", CREATE_USER_POS, MED_AVATAR_SIZE);
        if (menu_selected == CREATE)
            draw_frame(CREATE_USER_POS, SELECTABLE_USER_SIZE);
        trigger_scroll_to_item(menu_selected == CREATE);
        ImGui::SetWindowFontScale(0.7f);
        const auto CREATE_USER_SIZE_STR = (MED_AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["create_user"].c_str(), 0, false, TEXT_USER_PADDING).x / 2.f);
        ImGui::PushTextWrapPos(CREATE_USER_POS.x + TEXT_USER_PADDING);
        ImGui::SetCursorPos(ImVec2(CREATE_USER_POS.x + CREATE_USER_SIZE_STR, AVATAR_POS.y + MED_AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["create_user"].c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + SPACE_AVATAR, AVATAR_POS.y));
        for (const auto &user : gui.users) {
            ImGui::PushID(user.first.c_str());
            const auto user_id = string_utils::stoi_def(user.first, 0, "gui user id");
            const auto is_current_user_id_selected = (user_id == current_user_id_selected) && ((menu_selected == SELECT) || (menu_selected == EDIT));
            users_list_available.push_back(user_id);
            const auto USER_POS = ImGui::GetCursorPos();
            const auto EDIT_USER_STR_SIZE = ImGui::CalcTextSize(lang["edit_user"].c_str(), 0, false, MED_AVATAR_SIZE.x - (ImGui::GetStyle().FramePadding.x * 2.f));
            const auto EDIT_USER_STR_POS = ImVec2(USER_POS.x + (MED_AVATAR_SIZE.x / 2.f) - (EDIT_USER_STR_SIZE.x / 2.f), USER_POS.y - ImGui::GetStyle().ItemSpacing.y - ImGui::GetStyle().FramePadding.y - EDIT_USER_STR_SIZE.y);
            ImGui::PushTextWrapPos(USER_POS.x + MED_AVATAR_SIZE.x);
            ImGui::SetCursorPos(EDIT_USER_STR_POS);
            ImGui::Text("%s", lang["edit_user"].c_str());
            ImGui::PopTextWrapPos();
            const auto EDIT_USER_POS_SEL = ImVec2(USER_POS.x, USER_POS.y - EDIT_USER_STR_SIZE.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetStyle().FramePadding.y * 2.f));
            ImGui::SetCursorPos(EDIT_USER_POS_SEL);
            const auto is_edit_user_selected = (menu_selected == EDIT) && is_current_user_id_selected;
            const auto EDIT_USER_SIZE = ImVec2(MED_AVATAR_SIZE.x, EDIT_USER_STR_SIZE.y + (ImGui::GetStyle().FramePadding.y * 2.f));
            if (ImGui::InvisibleButton("##edit_user", EDIT_USER_SIZE)) {
                current_user_id_selected = user_id;
                menu_selected = EDIT;
                if (is_current_user_id_selected) {
                    user_id_selected = user.first;
                    init_avatar(gui, emuenv, "temp", gui.users[user.first].avatar);
                    temp = gui.users[user.first];
                    menu = EDIT;
                }
            }
            if (is_edit_user_selected)
                draw_frame(EDIT_USER_POS_SEL, EDIT_USER_SIZE);
            trigger_scroll_to_item(is_current_user_id_selected);
            draw_avatar(user.first, MEDIUM, USER_POS);
            ImGui::SetCursorPos(USER_POS);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
            if (ImGui::InvisibleButton("##avatar", SELECTABLE_USER_SIZE)) {
                current_user_id_selected = user_id;
                menu_selected = SELECT;
                if (is_current_user_id_selected)
                    select_and_open_user(gui, emuenv, user.first);
            }
            if ((menu_selected == SELECT) && is_current_user_id_selected)
                draw_frame(USER_POS, SELECTABLE_USER_SIZE);
            ImGui::PopStyleColor();
#ifndef __ANDROID__
            if (ImGui::BeginPopupContextItem("##user_context_menu")) {
                if (ImGui::MenuItem(lang["open_user_folder"].c_str()))
                    open_path((user_path / user.first).string());
                ImGui::EndPopup();
            }
#endif
            ImGui::SetCursorPos(ImVec2(USER_POS.x + USER_NAME_PADDING, USER_POS.y + MED_AVATAR_SIZE.y + (5.f * SCALE.y)));
            ImGui::PushTextWrapPos(USER_POS.x + MED_AVATAR_SIZE.x - USER_NAME_PADDING);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", user.second.name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopID();
            ImGui::SetCursorPos(ImVec2(USER_POS.x + SPACE_AVATAR, USER_POS.y));
        }
        if (!gui.users.empty()) {
            draw_user_bg(MEDIUM, DELETE_USER_POS);
            ImGui::SetCursorPos(DELETE_USER_POS);
            ImGui::SetWindowFontScale(1.6f);
            if (ImGui::InvisibleButton("-", SELECTABLE_USER_SIZE)) {
                if (menu_selected == DELETE_USER)
                    menu = DELETE_USER;
                else
                    menu_selected = DELETE_USER;
            }
            draw_centered_label("-", DELETE_USER_POS, MED_AVATAR_SIZE);
            if (menu_selected == DELETE_USER)
                draw_frame(DELETE_USER_POS, SELECTABLE_USER_SIZE);
            trigger_scroll_to_item(menu_selected == DELETE_USER);
            const auto delete_user_item_rect_half = ImGui::GetItemRectMax().x - (MED_AVATAR_SIZE.x / 2.f);
            ImGui::SetWindowFontScale(0.7f);
            ImGui::PushTextWrapPos(DELETE_USER_POS.x + TEXT_USER_PADDING);
            const auto DEL_USER_POS_STR = DELETE_USER_POS.x + (MED_AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["delete_user"].c_str(), 0, false, TEXT_USER_PADDING).x / 2.f);
            ImGui::SetCursorPos(ImVec2(DEL_USER_POS_STR, AVATAR_POS.y + TEXT_USER_PADDING));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["delete_user"].c_str());
            ImGui::PopTextWrapPos();
        }
        break;
    }
    case CREATE:
    case EDIT: {
        title = menu == CREATE ? lang["create_user"].c_str() : lang["edit_user"].c_str();
        ImGui::SetWindowFontScale(0.6f);
        const auto RESET_AVATAR_BTN_SIZE = ImGui::CalcTextSize(lang["reset_avatar"].c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.f);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + (LARGE_AVATAR_SIZE.x / 2.f) - (RESET_AVATAR_BTN_SIZE / 2.f), AVATAR_POS.y - (5.f * SCALE.y) - BUTTON_SIZE.y));
        if ((temp.avatar != "default") && ImGui::Button(lang["reset_avatar"].c_str(), ImVec2(RESET_AVATAR_BTN_SIZE, BUTTON_SIZE.y))) {
            temp.avatar = "default";
            init_avatar(gui, emuenv, "temp", "default");
        }
        draw_avatar("temp", LARGE, AVATAR_POS);
        const ImVec2 CHANGE_AVATAR_BTN_SIZE(LARGE_AVATAR_SIZE.x, 36.f * SCALE.y);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + (LARGE_AVATAR_SIZE.x / 2.f) - (CHANGE_AVATAR_BTN_SIZE.x / 2.f), AVATAR_POS.y + LARGE_AVATAR_SIZE.y + (5.f * SCALE.y)));
        if (ImGui::Button(lang["choose_avatar"].c_str(), CHANGE_AVATAR_BTN_SIZE)) {
            fs::path avatar_path{};
            host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(avatar_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });
            if (result == host::dialog::filesystem::Result::SUCCESS) {
                if (fs::exists(avatar_path) && init_avatar(gui, emuenv, "temp", avatar_path.string()))
                    temp.avatar = avatar_path.string();
            } else if (result == host::dialog::filesystem::Result::ERROR)
                LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
        }
        ImGui::SetWindowFontScale(0.8f);
        const auto INPUT_NAME_SIZE = 330.f * SCALE.x;
        const auto INPUT_NAME_POS = ImVec2((SIZE_USER.x / 2.f) - (INPUT_NAME_SIZE / 2.f), SIZE_USER.y - (118.f * SCALE.y));
        ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x, INPUT_NAME_POS.y - (30.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["name"].c_str());
        ImGui::SetCursorPos(INPUT_NAME_POS);
        ImGui::PushItemWidth(INPUT_NAME_SIZE);
        // It's correct to use std::string this way because of small string optimization (string is small enough to be stored in the string object itself)
        if (ImGui::InputText("##user_name", temp.name.data(), SCE_NP_ONLINEID_MAX_LENGTH))
            temp.name = temp.name.data();
        ImGui::PopItemWidth();
        const auto free_name = get_users_index(gui, temp.name) == gui.users.end();
        const auto check_free_name = (menu == CREATE ? free_name : (temp.name == gui.users[user_id_selected].name) || free_name);
        if (!check_free_name) {
            ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x + INPUT_NAME_SIZE + (10.f * SCALE.x), INPUT_NAME_POS.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "! %s", lang["user_name_used"].c_str());
        }
        ImGui::SetWindowFontScale(0.6f);
        ImGui::SetCursorPos(BUTTON_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        const auto confirm = lang["confirm"].c_str();
        if (!temp.name.empty() && check_free_name ? ImGui::Button(confirm, BUTTON_SIZE) : ImGui::Selectable(confirm, false, ImGuiSelectableFlags_Disabled, BUTTON_SIZE))
            create_and_save_user(gui, emuenv);
        ImGui::PopStyleVar();
        break;
    }
    case CONFIRM: {
        ImGui::SetWindowFontScale(0.8f);
        ImGui::SetCursorPosY(44.f * SCALE.y);
        TextColoredCentered(GUI_COLOR_TEXT, lang["user_created"].c_str());
        const auto AVATAR_CONFIRM_POS = ImVec2((SIZE_USER.x / 2) - (MED_AVATAR_SIZE.x / 2.f), 96.f * SCALE.y);
        draw_avatar(user_id_selected, MEDIUM, AVATAR_CONFIRM_POS);
        ImGui::SetWindowFontScale(0.7f);
        ImGui::SetCursorPos(ImVec2(AVATAR_CONFIRM_POS.x + USER_NAME_PADDING, AVATAR_CONFIRM_POS.y + MED_AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::PushTextWrapPos(AVATAR_CONFIRM_POS.x + MED_AVATAR_SIZE.x - USER_NAME_PADDING);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[user_id_selected].name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorPos(BUTTON_POS);
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            clear_user_temp(gui);
            menu = menu_selected = SELECT;
        }
        break;
    }
    case DELETE_USER: {
        title = lang["delete_user"];
        if (user_id_selected.empty()) {
            ImGui::SetWindowFontScale(1.f);
            const auto CHILD_DELETE_USER_SIZE = ImVec2(674 * SCALE.x, 308.f * SCALE.y);
            const auto SELECT_SIZE = ImVec2(674.f * SCALE.x, 46.f * SCALE.y);
            ImGui::SetCursorPosY(5.f * SCALE.y);
            TextColoredCentered(GUI_COLOR_TEXT, lang["user_delete"].c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            const auto CHILD_DELETE_USER_POS = ImVec2(WINDOW_SIZE.x / 2.f, (168.f * SCALE.y));
            ImGui::SetNextWindowPos(CHILD_DELETE_USER_POS, ImGuiCond_Always, ImVec2(0.5f, 0.f));
            auto delete_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
            if (gui.is_nav_button)
                delete_flags |= ImGuiWindowFlags_NoMouseInputs;
            ImGui::BeginChild("##delete_user_child", CHILD_DELETE_USER_SIZE, ImGuiChildFlags_None, delete_flags);
            ImGui::SetWindowFontScale(RES_SCALE.x);
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, SMALL_AVATAR_SIZE.x + (10.f * SCALE.x));
            ImGui::Separator();
            for (const auto &user : gui.users) {
                users_list_available.push_back(string_utils::stoi_def(user.first, 0, "gui user id"));
                draw_avatar(user.first, SMALL, ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + (SELECT_SIZE.y / 2.f) - (SMALL_AVATAR_SIZE.y / 2.f)));
                ImGui::NextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                if (ImGui::Selectable(user.second.name.c_str(), gui.is_nav_button && (current_user_id_selected == string_utils::stoi_def(user.first, 0, "gui user id")), ImGuiSelectableFlags_SpanAllColumns, SELECT_SIZE))
                    user_id_selected = user.first;
                if (gui.is_nav_button && (current_user_id_selected == string_utils::stoi_def(user.first, 0, "gui user id"))) {
                    if (ImGui::GetItemRectMin().y < CHILD_DELETE_USER_POS.y)
                        ImGui::SetScrollHereY(0.f);
                    else if (ImGui::GetItemRectMax().y > (CHILD_DELETE_USER_POS.y + CHILD_DELETE_USER_SIZE.y))
                        ImGui::SetScrollHereY(1.f);
                }
                ImGui::Separator();
                ImGui::NextColumn();
                ImGui::PopStyleVar();
                ImGui::ScrollWhenDragging();
            }
            ImGui::Columns(1);
            ImGui::EndChild();
            ImGui::PopStyleVar();
        } else {
            ImGui::SetWindowFontScale(0.8f);
            if (del_menu.empty()) {
                ImGui::SetCursorPos(ImVec2(148.f * SCALE.x, 100.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["user_delete_msg"].c_str());
                ImGui::SetCursorPos(ImVec2(194.f * SCALE.x, 148.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[user_id_selected].name.c_str());
                ImGui::SetCursorPos(ImVec2(148.f * SCALE.x, 194.f * SCALE.y));
                ImGui::TextWrapped("%s", lang["user_delete_message"].c_str());
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button(common["delete"].c_str(), BUTTON_SIZE))
                    del_menu = "warn";
            } else if (del_menu == "warn") {
                ImGui::SetCursorPosY(146.f * SCALE.y);
                TextColoredCentered(GUI_COLOR_TEXT, lang["user_delete_warn"].c_str());
                ImGui::SetCursorPos(BUTTON_POS);
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - BUTTON_SIZE.x - 20.f, BUTTON_POS.y));
                if (ImGui::Button(common["no"].c_str(), BUTTON_SIZE)) {
                    user_id_selected.clear();
                    del_menu.clear();
                }
                ImGui::SameLine(0, 40.f * SCALE.x);
                if (ImGui::Button(common["yes"].c_str(), BUTTON_SIZE))
                    delete_user(gui, emuenv);
            } else if (del_menu == "confirm") {
                ImGui::SetCursorPosY(146.f * SCALE.y);
                TextColoredCentered(GUI_COLOR_TEXT, lang["user_deleted"].c_str());
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                    del_menu.clear();
                    user_id_selected.clear();
                    if (gui.users.empty()) {
                        menu = SELECT;
                        menu_selected = CREATE;
                    }
                }
            }
        }
        break;
    }
    }
    ImGui::EndChild();

    ImGui::SetCursorPosY(WINDOW_SIZE.y - POS_SEPARATOR);
    ImGui::Separator();
    ImGui::SetWindowFontScale(RES_SCALE.x);
    const auto USER_ALREADY_INIT = !gui.users.empty() && !emuenv.io.user_id.empty() && (emuenv.cfg.user_id == emuenv.io.user_id);
    if ((menu == SELECT && USER_ALREADY_INIT) || ((menu != SELECT) && (menu != CONFIRM) && del_menu.empty())) {
        ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, ImGui::GetCursorPosY() + (10.f * SCALE.y)));
        if (ImGui::Button(common["cancel"].c_str(), ImVec2(110.f * SCALE.x, 40.f * SCALE.y))) {
            if (menu != SELECT) {
                if ((menu == CREATE) || (menu == EDIT))
                    clear_user_temp(gui);
                if ((menu == DELETE_USER) && !user_id_selected.empty())
                    user_id_selected.clear();
                else
                    menu = SELECT;
            } else if (USER_ALREADY_INIT) {
                gui.vita_area.user_management = false;
                gui.vita_area.home_screen = true;
            }
        }
    }
    if (menu == SELECT && !gui.users.empty()) {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["automatic_user_login"].c_str()).x / 2.f), WINDOW_SIZE.y - 50.f * SCALE.y));
        if (ImGui::Checkbox(lang["automatic_user_login"].c_str(), &emuenv.cfg.auto_user_login))
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    if (!gui.users.empty() && gui.users.contains(emuenv.cfg.user_id)) {
        draw_avatar(emuenv.cfg.user_id, SMALL, ImVec2(WINDOW_SIZE.x - (220.f * SCALE.x), WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (SMALL_AVATAR_SIZE.y / 2.f)));
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 180.f * SCALE.x, WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (ImGui::CalcTextSize(gui.users[emuenv.cfg.user_id].name.c_str()).y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[emuenv.cfg.user_id].name.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace gui
