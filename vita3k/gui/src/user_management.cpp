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

#include <config/functions.h>
#include <gui/functions.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <nfd.h>
#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

static bool init_avatar(GuiState &gui, HostState &host, const std::string &user_id, const std::string &avatar) {
    gui.users_avatar[user_id] = {};
    const auto avatar_path = avatar == "default" ? host.base_path + "/data/image/icon.png" : avatar;
    const std::wstring avatar_path_wstr = string_utils::utf_to_wide(avatar_path);

    if (!fs::exists(avatar_path_wstr)) {
        LOG_WARN("Avatar image doesn't exist: {}.", avatar_path);
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;

#ifdef _WIN32
    FILE *f = _wfopen(avatar_path_wstr.c_str(), L"rb");
#else
    FILE *f = fopen(avatar_path.c_str(), "rb");
#endif

    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", avatar_path);
        return false;
    }

    gui.users_avatar[user_id].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
    fclose(f);

    return gui.users_avatar.find(user_id) != gui.users_avatar.end();
}

void get_users_list(GuiState &gui, HostState &host) {
    gui.users.clear();
    const auto user_path{ fs::path(host.pref_path) / "ux0/user" };
    if (fs::exists(user_path) && !fs::is_empty(user_path)) {
        for (const auto &path : fs::directory_iterator(user_path)) {
            pugi::xml_document user_xml;
            if (fs::is_directory(path) && user_xml.load_file(((path / "user.xml").c_str()))) {
                const auto user_child = user_xml.child("user");

                // Load user settings
                std::string user_id;
                if (!user_child.attribute("id").empty())
                    user_id = user_child.attribute("id").as_string();
                else
                    user_id = path.path().stem().string();

                auto &user = gui.users[user_id];
                user.id = user_id;
                if (!user_child.attribute("name").empty())
                    user.name = user_child.attribute("name").as_string();
                else
                    user.name = "vita3K";

                if (!user_child.attribute("date-format").empty())
                    user.date_format = DateFormat(user_child.attribute("date-format").as_int());
                else
                    user.date_format = DateFormat::MM_DD_YYYY;

                if (!user_child.attribute("clock-12-hour").empty())
                    user.clock_12_hour = user_child.attribute("clock-12-hour").as_bool();
                else
                    user.clock_12_hour = true;

                if (!user_child.child("avatar").text().empty())
                    user.avatar = user_child.child("avatar").text().as_string();
                else
                    user.avatar = "default";
                init_avatar(gui, host, user.id, user.avatar);

                // Load theme settings
                auto theme = user_child.child("theme");
                if (!theme.attribute("use-background").empty())
                    user.use_theme_bg = theme.attribute("use-background").as_bool();
                else
                    user.use_theme_bg = true;

                if (!theme.child("content-id").text().empty())
                    user.theme_id = theme.child("content-id").text().as_string();
                else
                    user.theme_id = "default";

                // Load start screen settings
                auto start = user_child.child("start-screen");
                if (!start.attribute("type").empty())
                    user.start_type = start.attribute("type").as_string();
                else
                    user.start_type = "default";

                if (!start.child("path").text().empty())
                    user.start_path = start.child("path").text().as_string();

                // Load backgrounds path
                for (const auto &bg : user_child.child("backgrounds"))
                    user.backgrounds.push_back(bg.text().as_string());
            }
        }
    }
}

void save_user(GuiState &gui, HostState &host, const std::string &user_id) {
    const auto user_path{ fs::path(host.pref_path) / "ux0/user" / user_id };
    if (!fs::exists(user_path))
        fs::create_directory(user_path);

    const auto &user = gui.users[user_id];

    pugi::xml_document user_xml;
    auto declarationUser = user_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    // Save user settings
    auto user_child = user_xml.append_child("user");
    user_child.append_attribute("id") = user.id.c_str();
    user_child.append_attribute("name") = user.name.c_str();
    user_child.append_attribute("date-format") = user.date_format;
    user_child.append_attribute("clock-12-hour") = user.clock_12_hour;
    user_child.append_child("avatar").append_child(pugi::node_pcdata).set_value(user.avatar.c_str());

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
        LOG_ERROR("Fail save xml for user id: {}, name: {}, in path: {}", user.id, user.name, user_path.string());
}

void init_user(GuiState &gui, HostState &host, const std::string &user_id) {
    host.io.user_id = user_id;
    host.io.user_name = gui.users[user_id].name;
    if (!gui.users[user_id].backgrounds.empty()) {
        for (const auto &bg : gui.users[user_id].backgrounds)
            init_user_background(gui, host, user_id, bg);
    }
    init_theme(gui, host, gui.users[user_id].theme_id);
    init_notice_info(gui, host);
}

void open_user(GuiState &gui, HostState &host) {
    gui.live_area.user_management = false;

    if (gui.users[host.io.user_id].start_type == "image")
        init_user_start_background(gui, gui.users[host.io.user_id].start_path);
    else
        init_theme_start_background(gui, host, gui.users[host.io.user_id].theme_id);

    gui.live_area.start_screen = true;
}

static auto get_users_index(GuiState &gui, const std::string &user_name) {
    const auto profils_index = std::find_if(gui.users.begin(), gui.users.end(), [&](const auto &u) {
        return u.second.name == user_name;
    });

    return profils_index;
}

static std::string menu, del_menu, title, user_id;
static User temp;

void clear_temp(GuiState &gui) {
    temp = {};
    gui.users_avatar["temp"] = {};
    gui.users_avatar.erase("temp");
    user_id.clear();
}

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

void draw_user_management(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT);

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##user_management", &gui.live_area.user_management, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (!host.display.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.live_area.information_bar = true;

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size, IM_COL32(10.f, 50.f, 140.f, 255.f), 0.f, ImDrawCornerFlags_All);

    const auto user_path{ fs::path(host.pref_path) / "ux0/user" };
    const auto AVATAR_SIZE = ImVec2(128 * SCALE.x, 128 * SCALE.y);
    const auto SMALL_AVATAR_SIZE = (ImVec2(34.f * SCALE.x, 34.f * SCALE.y));
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    const auto calc_title = ImGui::CalcTextSize(title.c_str()).y / 2.f;
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, (32.f * SCALE.y) - calc_title));
    ImGui::TextColored(GUI_COLOR_TEXT, title.c_str());

    const auto SIZE_USER = ImVec2(960.f * SCALE.x, 376.f * SCALE.y);
    const auto POS_SEPARATOR = 68.f * SCALE.y;
    const auto SPACE_AVATAR = AVATAR_SIZE.x + (20.f * SCALE.x);

    ImGui::SetCursorPosY(POS_SEPARATOR);
    ImGui::Separator();

    if (menu.empty())
        ImGui::SetNextWindowContentSize(ImVec2(SIZE_USER.x + ((gui.users.size() - 2.5f) * SPACE_AVATAR), 0.0f));
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (102.f * SCALE.y)), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::BeginChild("##user_child", SIZE_USER, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    const auto AVATAR_POS = ImVec2((SIZE_USER.x / 2) - (AVATAR_SIZE.x / 2.f), (((menu == "create" || menu == "edit") ? 48.f : 122.f) * SCALE.y));
    const auto NEW_USER_POS = AVATAR_POS.x - (gui.users.size() ? SPACE_AVATAR : 0.f);
    const auto DELETE_USER_POS = AVATAR_POS.x + (SPACE_AVATAR * (gui.users.size()));
    const auto BUTTON_SIZE = ImVec2(220 * SCALE.x, 36 * SCALE.y);
    const auto BUTTON_POS = ImVec2((SIZE_USER.x / 2.f) - (BUTTON_SIZE.x / 2.f), 314.f * SCALE.y);
    const auto is_lang = !gui.lang.user_management.empty();
    const auto NEW_USER_STR = is_lang ? gui.lang.user_management["create_user"] : "Create User";
    const auto EDIT_USER_STR = is_lang ? gui.lang.user_management["edit_user"] : "Edit User";
    const auto DELETE_USER_STR = is_lang ? gui.lang.user_management["delete_user"] : "Delete User";

    if (menu.empty()) {
        // Users list
        title = "Select your user";
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::SetWindowFontScale(1.6f);
        ImGui::SetCursorPos(ImVec2(NEW_USER_POS, AVATAR_POS.y));
        if (ImGui::Selectable("+", false, ImGuiSelectableFlags_None, AVATAR_SIZE)) {
            menu = "create";
            auto id = 0;
            for (const auto &user : gui.users) {
                if (id != std::stoi(user.first))
                    break;
                else
                    ++id;
            }
            user_id = fmt::format("{:0>2d}", id);
            auto i = 1;
            const auto user = is_lang ? gui.lang.user_management["user"] : "User";
            for (i; i < gui.users.size(); i++) {
                if (get_users_index(gui, user + std::to_string(i)) == gui.users.end())
                    break;
            }
            temp.id = user_id;
            temp.name = user + std::to_string(i);
            temp.date_format = DateFormat::MM_DD_YYYY;
            temp.clock_12_hour = true;
            temp.avatar = "default";
            temp.theme_id = "default";
            temp.use_theme_bg = true;
            temp.start_type = "default";
            init_avatar(gui, host, "temp", "default");
        }
        ImGui::SetWindowFontScale(0.9f);
        const auto calc_text = (AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(NEW_USER_STR.c_str()).x / 2.f);
        ImGui::SetCursorPos(ImVec2(NEW_USER_POS + calc_text, AVATAR_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, NEW_USER_STR.c_str());
        ImGui::SetCursorPos(AVATAR_POS);
        for (const auto &user : gui.users) {
            ImGui::PushID(user.first.c_str());
            const auto USER_POS = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(USER_POS.x, USER_POS.y - BUTTON_SIZE.y));
            if (ImGui::Button(EDIT_USER_STR.c_str(), ImVec2(AVATAR_SIZE.x, BUTTON_SIZE.y))) {
                user_id = user.first;
                gui.users_avatar["temp"] = std::move(gui.users_avatar[user.first]);
                temp = gui.users[user.first];
                menu = "edit";
            }
            if (gui.users_avatar.find(user.first) != gui.users_avatar.end()) {
                ImGui::SetCursorPos(USER_POS);
                ImGui::Image(gui.users_avatar[user.first], AVATAR_SIZE);
            }
            ImGui::SetCursorPos(USER_POS);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
            if (ImGui::Selectable("##avatar", false, ImGuiSelectableFlags_None, AVATAR_SIZE)) {
                if (host.io.user_id != user.first) {
                    init_user(gui, host, user.first);
                    init_notice_info(gui, host);
                }
                if (host.cfg.user_id != user.first) {
                    host.cfg.user_id = user.first;
                    config::serialize_config(host.cfg, host.cfg.config_path);
                }
                open_user(gui, host);
            }
            ImGui::SetWindowFontScale(0.9f);
            ImGui::PopStyleColor();
            if (ImGui::BeginPopupContextItem("##user_context_menu")) {
                if (ImGui::MenuItem("Open User Folder"))
                    system((OS_PREFIX + (user_path / user.first).string()).c_str());
                ImGui::EndPopup();
            }
            ImGui::PopID();
            const auto POS_NAME = ImVec2(USER_POS.x + AVATAR_SIZE.x / 2 - ImGui::CalcTextSize(user.second.name.c_str()).x / 2, USER_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y));
            ImGui::SetCursorPos(POS_NAME);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", user.second.name.c_str());
            ImGui::SetCursorPos(ImVec2(USER_POS.x + SPACE_AVATAR, USER_POS.y));
        }
        if (gui.users.size()) {
            ImGui::SetCursorPos(ImVec2(DELETE_USER_POS, AVATAR_POS.y));
            ImGui::SetWindowFontScale(1.6f);
            if (ImGui::Selectable("-", false, ImGuiSelectableFlags_None, AVATAR_SIZE))
                menu = "delete";
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(0.9f);
            const auto calc_del_text = (AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(DELETE_USER_STR.c_str()).x / 2.f);
            ImGui::SetCursorPos(ImVec2(DELETE_USER_POS + calc_del_text, AVATAR_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, DELETE_USER_STR.c_str());
        } else {
            ImGui::PopStyleVar();
        }
    } else if ((menu == "create") || (menu == "edit")) {
        title = menu == "create" ? NEW_USER_STR : EDIT_USER_STR;
        ImGui::SetWindowFontScale(0.6f);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x, AVATAR_POS.y - BUTTON_SIZE.y));
        if ((temp.avatar != "default") && ImGui::Button("Reset Avatar", ImVec2(AVATAR_SIZE.x, BUTTON_SIZE.y))) {
            temp.avatar = "default";
            init_avatar(gui, host, "temp", "default");
        }
        if (gui.users_avatar.find("temp") != gui.users_avatar.end()) {
            ImGui::SetCursorPos(ImVec2(AVATAR_POS));
            ImGui::Image(gui.users_avatar["temp"], AVATAR_SIZE);
        }
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x, AVATAR_POS.y + AVATAR_SIZE.y));
        if (ImGui::Button(is_lang ? gui.lang.user_management["change_avatar"].c_str() : "Change Avatar", ImVec2(AVATAR_SIZE.x, BUTTON_SIZE.y))) {
            nfdchar_t *avatar_path;
            nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &avatar_path);

            if ((result == NFD_OKAY) && init_avatar(gui, host, "temp", avatar_path))
                temp.avatar = avatar_path;
        }
        ImGui::SetWindowFontScale(0.8f);
        const auto INPUT_NAME_SIZE = 330.f * SCALE.x;
        const auto INPUT_NAME_POS = ImVec2((SIZE_USER.x / 2.f) - (INPUT_NAME_SIZE / 2.f), 240.f * SCALE.y);
        ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x, INPUT_NAME_POS.y - (30.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? gui.lang.user_management["name"].c_str() : "Name");
        ImGui::SetCursorPos(INPUT_NAME_POS);
        ImGui::PushItemWidth(INPUT_NAME_SIZE);
        ImGui::InputText("##user_name", &temp.name);
        ImGui::PopItemWidth();
        const auto free_name = get_users_index(gui, temp.name) == gui.users.end();
        const auto check_free_name = (menu == "create" ? free_name : (temp.name == gui.users[user_id].name) || free_name);
        if (!check_free_name) {
            ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x + INPUT_NAME_SIZE + 10.f, INPUT_NAME_POS.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "! This name is already in use.");
        }
        ImGui::SetCursorPos(BUTTON_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        const auto confirm = is_lang ? gui.lang.user_management["confirm"] : "Confirm";
        if (!temp.name.empty() && check_free_name ? ImGui::Button(confirm.c_str(), BUTTON_SIZE) : ImGui::Selectable(confirm.c_str(), false, ImGuiSelectableFlags_Disabled, BUTTON_SIZE)) {
            gui.users_avatar[user_id] = std::move(gui.users_avatar["temp"]);
            gui.users[user_id] = temp;
            save_user(gui, host, user_id);
            if (menu == "create")
                menu = "confirm";
            else {
                clear_temp(gui);
                menu.clear();
            }
        }
        ImGui::PopStyleVar();
    } else if (menu == "confirm") {
        ImGui::SetWindowFontScale(0.8f);
        const std::string msg = "The following user has been created";
        const auto calc_text = (SIZE_USER.x / 2.f) - (ImGui::CalcTextSize(msg.c_str()).x / 2.f);
        ImGui::SetCursorPos(ImVec2(calc_text, (44.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, msg.c_str());
        ImGui::SetCursorPos(AVATAR_POS);
        if (gui.users_avatar.find(user_id) != gui.users_avatar.end())
            ImGui::Image(gui.users_avatar[user_id], AVATAR_SIZE);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + AVATAR_SIZE.x / 2 - ImGui::CalcTextSize(gui.users[user_id].name.c_str()).x / 2, AVATAR_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, gui.users[user_id].name.c_str());
        ImGui::SetCursorPos(BUTTON_POS);
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            clear_temp(gui);
            menu.clear();
        }
    } else if (menu == "delete") {
        title = DELETE_USER_STR;
        if (user_id.empty()) {
            ImGui::SetWindowFontScale(1.f);
            ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - (ImGui::CalcTextSize("Select the user you want to delete.").x / 2.f), 5.f * SCALE.y));
            const auto CHILD_DELETE_USER_SIZE = ImVec2(674 * SCALE.x, 308.f * SCALE.y);
            const auto SELECT_SIZE = ImVec2(674.f * SCALE.x, 46.f * SCALE.y);
            ImGui::TextColored(GUI_COLOR_TEXT, "Select the user you want to delete.");
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (168.f * SCALE.y)), ImGuiCond_Always, ImVec2(0.5f, 0.f));
            ImGui::BeginChild("##delete_user_child", CHILD_DELETE_USER_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetWindowFontScale(1.6f);
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, SMALL_AVATAR_SIZE.x + (10.f * SCALE.x));
            for (const auto &user : gui.users) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SELECT_SIZE.y / 2.f) - (SMALL_AVATAR_SIZE.y / 2.f));
                ImGui::Image(gui.users_avatar[user.first], SMALL_AVATAR_SIZE);
                ImGui::NextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                if (ImGui::Selectable(user.second.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, SELECT_SIZE))
                    user_id = user.first;
                ImGui::NextColumn();
                ImGui::PopStyleVar();
            }
            ImGui::Columns(1);
            ImGui::EndChild();
            ImGui::PopStyleVar();
        } else {
            ImGui::SetWindowFontScale(0.8f);
            if (del_menu.empty()) {
                ImGui::SetCursorPos(ImVec2(148.f * SCALE.x, 100.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "The following user will be deleted.");
                ImGui::SetCursorPos(ImVec2(194.f * SCALE.x, 148.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[user_id].name.c_str());
                ImGui::SetCursorPos(ImVec2(148.f * SCALE.x, 194.f * SCALE.y));
                ImGui::TextWrapped("If you delete the user, that user's saved data, trophies will be deleted.");
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button("Delete", BUTTON_SIZE))
                    del_menu = "warn";
            } else if (del_menu == "warn") {
                const auto calc_text = (SIZE_USER.x / 2.f) - (ImGui::CalcTextSize("Are you sure you want to continue?").x / 2.f);
                ImGui::SetCursorPos(ImVec2(calc_text, 146.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "The user will be deleted.");
                ImGui::SetCursorPosX(calc_text);
                ImGui::TextColored(GUI_COLOR_TEXT, "Are you sure you want to continue?");
                ImGui::SetCursorPos(BUTTON_POS);
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - BUTTON_SIZE.x - 20.f, BUTTON_POS.y));
                if (ImGui::Button("No", BUTTON_SIZE)) {
                    user_id.clear();
                    del_menu.clear();
                }
                ImGui::SameLine(0, 40.f * SCALE.x);
                if (ImGui::Button("Yes", BUTTON_SIZE)) {
                    fs::remove_all(user_path / user_id);
                    gui.users_avatar.erase(user_id);
                    gui.users.erase(get_users_index(gui, gui.users[user_id].name));
                    if (user_id == host.io.user_id)
                        host.io.user_id.clear();
                    del_menu = "confirm";
                }
            } else if (del_menu == "confirm") {
                ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - (ImGui::CalcTextSize("User Deleted.").x / 2.f), 146.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "User Deleted.");
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button("OK", BUTTON_SIZE)) {
                    del_menu.clear();
                    user_id.clear();
                    if (!gui.users.size())
                        menu.clear();
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::SetCursorPosY(WINDOW_SIZE.y - POS_SEPARATOR);
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.f);
    const auto USER_ALREADY_INIT = host.cfg.user_id == host.io.user_id;
    if ((menu.empty() && USER_ALREADY_INIT) || (!menu.empty() && (menu != "confirm") && del_menu.empty())) {
        ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, ImGui::GetCursorPosY() + (10.f * SCALE.y)));
        if (ImGui::Button("Cancel", ImVec2(80.f * SCALE.x, 40.f * SCALE.y))) {
            if (!menu.empty()) {
                if ((menu == "create") || (menu == "edit"))
                    clear_temp(gui);
                if ((menu == "delete") && !user_id.empty())
                    user_id.clear();
                else
                    menu.clear();
            } else if (USER_ALREADY_INIT) {
                gui.live_area.user_management = false;
                gui.live_area.app_selector = true;
            }
        }
    }
    if (menu.empty()) {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize("Automatic User Login").x / 2.f), WINDOW_SIZE.y - 50.f * SCALE.y));
        if (ImGui::Checkbox("Automatic User Login", &host.cfg.auto_user_login))
            config::serialize_config(host.cfg, host.cfg.config_path);
    }
    if (!gui.users.empty() && (gui.users.find(host.cfg.user_id) != gui.users.end())) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 220.f * SCALE.x, WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (SMALL_AVATAR_SIZE.y / 2.f)));
        if (gui.users_avatar.find(host.cfg.user_id) != gui.users_avatar.end())
            ImGui::Image(gui.users_avatar[host.cfg.user_id], SMALL_AVATAR_SIZE);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 180.f * SCALE.x, WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (ImGui::CalcTextSize(gui.users[host.cfg.user_id].name.c_str()).y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[host.cfg.user_id].name.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
