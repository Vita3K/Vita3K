// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <display/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.hpp>
#include <io/state.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

static bool init_avatar(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id, const std::string avatar) {
    gui.users_avatar[user_id] = {};
    const auto avatar_path = avatar == "default" ? fs::path(emuenv.base_path) / "data/image/icon.png" : fs::path(string_utils::utf_to_wide(avatar));

    if (!fs::exists(avatar_path)) {
        LOG_WARN("Avatar image doesn't exist: {}.", avatar_path.string());
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;

#ifdef _WIN32
    FILE *f = _wfopen(avatar_path.c_str(), L"rb");
#else
    FILE *f = fopen(avatar_path.c_str(), "rb");
#endif

    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", avatar_path.string());
        return false;
    }

    gui.users_avatar[user_id].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
    fclose(f);

    return gui.users_avatar.find(user_id) != gui.users_avatar.end();
}

void get_users_list(GuiState &gui, EmuEnvState &emuenv) {
    gui.users.clear();
    const auto user_path{ fs::path(emuenv.pref_path) / "ux0/user" };
    if (fs::exists(user_path) && !fs::is_empty(user_path)) {
        for (const auto &path : fs::directory_iterator(user_path)) {
            pugi::xml_document user_xml;
            if (fs::is_directory(path) && user_xml.load_file(((path / "user.xml").c_str()))) {
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
                    user.sort_apps_type = SortType(sort_apps_list.attribute("type").as_uint());
                    user.sort_apps_state = SortState(sort_apps_list.attribute("state").as_uint());
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
                    user.backgrounds.push_back(bg.text().as_string());
            }
        }
    }
}

void save_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id) {
    const auto user_path{ fs::path(emuenv.pref_path) / "ux0/user" / user_id };
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
        LOG_ERROR("Fail save xml for user id: {}, name: {}, in path: {}", user.id, user.name, user_path.string());
}

void init_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id) {
    emuenv.io.user_id = user_id;
    emuenv.io.user_name = gui.users[user_id].name;
    if (!gui.users[user_id].backgrounds.empty()) {
        for (const auto &bg : gui.users[user_id].backgrounds)
            init_user_background(gui, emuenv, user_id, bg);
    }
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

    gui.vita_area.start_screen = true;

#ifdef USE_VITA3K_UPDATE
    std::thread update_vita3k_thread([&gui]() {
        if (init_vita3k_update(gui))
            gui.help_menu.vita3k_update = true;
    });
    update_vita3k_thread.detach();
#endif
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

void draw_user_management(GuiState &gui, EmuEnvState &emuenv) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT);

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##user_management", &gui.vita_area.user_management, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (!emuenv.display.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.vita_area.information_bar = true;

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size, IM_COL32(10.f, 50.f, 140.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const auto user_path{ fs::path(emuenv.pref_path) / "ux0/user" };
    const auto AVATAR_SIZE = ImVec2(132 * SCALE.x, 132 * SCALE.y);
    const auto SMALL_AVATAR_SIZE = (ImVec2(34.f * SCALE.x, 34.f * SCALE.y));
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    const auto calc_title = ImGui::CalcTextSize(title.c_str()).y / 2.f;
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, (32.f * SCALE.y) - calc_title));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());

    const auto SIZE_USER = ImVec2(960.f * SCALE.x, 376.f * SCALE.y);
    const auto POS_SEPARATOR = 68.f * SCALE.y;
    const auto SPACE_AVATAR = AVATAR_SIZE.x + (20.f * SCALE.x);

    ImGui::SetCursorPosY(POS_SEPARATOR);
    ImGui::Separator();

    if (menu.empty())
        ImGui::SetNextWindowContentSize(ImVec2(SIZE_USER.x + ((gui.users.size() - 2.5f) * SPACE_AVATAR), 0.0f));
    ImGui::SetNextWindowPos(ImVec2(0.f, 102.f * SCALE.y), ImGuiCond_Always);
    ImGui::BeginChild("##user_child", SIZE_USER, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    const auto AVATAR_POS = ImVec2((SIZE_USER.x / 2) - (AVATAR_SIZE.x / 2.f), (((menu == "create" || menu == "edit") ? 48.f * SCALE.y : (SIZE_USER.y / 2) - (AVATAR_SIZE.y / 2.f))));
    const auto NEW_USER_POS = AVATAR_POS.x - (gui.users.size() ? SPACE_AVATAR : 0.f);
    const auto DELETE_USER_POS = AVATAR_POS.x + (SPACE_AVATAR * (gui.users.size()));
    const auto BUTTON_SIZE = ImVec2(220 * SCALE.x, 36 * SCALE.y);
    const auto BUTTON_POS = ImVec2((SIZE_USER.x / 2.f) - (BUTTON_SIZE.x / 2.f), 314.f * SCALE.y);
    const auto TEXT_USER_PADDING = AVATAR_SIZE.x + (5.f * SCALE.x);
    auto lang = gui.lang.user_management;
    auto common = emuenv.common_dialog.lang.common;

    if (menu.empty()) {
        // Users list
        title = lang["select_user"];
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
            const auto user = lang["user"].c_str();
            for (; i < gui.users.size(); i++) {
                if (get_users_index(gui, user + std::to_string(i)) == gui.users.end())
                    break;
            }
            temp.id = user_id;
            temp.name = user + std::to_string(i);
            temp.avatar = "default";
            temp.theme_id = "default";
            temp.use_theme_bg = true;
            temp.start_type = "default";
            init_avatar(gui, emuenv, "temp", "default");
        }
        ImGui::SetWindowFontScale(0.84f);
        const auto CREATE_USER_SIZE_STR = (AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["create_user"].c_str(), 0, false, TEXT_USER_PADDING).x / 2.f);
        ImGui::PushTextWrapPos(NEW_USER_POS + TEXT_USER_PADDING);
        ImGui::SetCursorPos(ImVec2(NEW_USER_POS + CREATE_USER_SIZE_STR, AVATAR_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["create_user"].c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorPos(AVATAR_POS);
        for (const auto &user : gui.users) {
            ImGui::PushID(user.first.c_str());
            const auto USER_POS = ImGui::GetCursorPos();
            const auto EDIT_USER_STR_SIZE = ImGui::CalcTextSize(lang["edit_user"].c_str(), 0, false, AVATAR_SIZE.x - (ImGui::GetStyle().FramePadding.x * 2.f));
            const auto EDIT_USER_STR_POS = ImVec2(USER_POS.x + (AVATAR_SIZE.x / 2.f) - (EDIT_USER_STR_SIZE.x / 2.f), USER_POS.y - EDIT_USER_STR_SIZE.y - ImGui::GetStyle().FramePadding.y);
            ImGui::PushTextWrapPos(USER_POS.x + AVATAR_SIZE.x);
            ImGui::SetCursorPos(EDIT_USER_STR_POS);
            ImGui::Text("%s", lang["edit_user"].c_str());
            ImGui::PopTextWrapPos();
            const auto EDIT_USER_POS_SEL = ImVec2(USER_POS.x + ImGui::GetStyle().FramePadding.x, USER_POS.y - EDIT_USER_STR_SIZE.y - ImGui::GetStyle().FramePadding.y);
            ImGui::SetCursorPos(EDIT_USER_POS_SEL);
            if (ImGui::Selectable("##edit_user", false, ImGuiSelectableFlags_None, ImVec2(AVATAR_SIZE.x - (ImGui::GetStyle().FramePadding.x * 2.f), EDIT_USER_STR_SIZE.y))) {
                user_id = user.first;
                init_avatar(gui, emuenv, "temp", gui.users[user.first].avatar);
                temp = gui.users[user.first];
                menu = "edit";
            }
            if (gui.users_avatar.contains(user.first)) {
                ImGui::SetCursorPos(USER_POS);
                ImGui::Image(gui.users_avatar[user.first], AVATAR_SIZE);
            }
            ImGui::SetCursorPos(USER_POS);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
            if (ImGui::Selectable("##avatar", false, ImGuiSelectableFlags_None, AVATAR_SIZE)) {
                if (emuenv.io.user_id != user.first)
                    init_user(gui, emuenv, user.first);
                if (emuenv.cfg.user_id != user.first) {
                    emuenv.cfg.user_id = user.first;
                    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                }
                open_user(gui, emuenv);
            }
            ImGui::PopStyleColor();
            if (ImGui::BeginPopupContextItem("##user_context_menu")) {
                if (ImGui::MenuItem(lang["open_user_folder"].c_str()))
                    open_path((user_path / user.first).string());
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
            ImGui::SetWindowFontScale(0.84f);
            ImGui::PushTextWrapPos(DELETE_USER_POS + TEXT_USER_PADDING);
            const auto DEL_USER_POS_STR = DELETE_USER_POS + (AVATAR_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["delete_user"].c_str(), 0, false, TEXT_USER_PADDING).x / 2.f);
            ImGui::SetCursorPos(ImVec2(DEL_USER_POS_STR, AVATAR_POS.y + TEXT_USER_PADDING));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["delete_user"].c_str());
            ImGui::PopTextWrapPos();
        } else {
            ImGui::PopStyleVar();
        }
    } else if ((menu == "create") || (menu == "edit")) {
        title = menu == "create" ? lang["create_user"].c_str() : lang["edit_user"].c_str();
        ImGui::SetWindowFontScale(0.6f);
        const auto RESET_AVATAR_BTN_SIZE = ImGui::CalcTextSize(lang["reset_avatar"].c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.f);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + (AVATAR_SIZE.x / 2.f) - (RESET_AVATAR_BTN_SIZE / 2.f), AVATAR_POS.y - BUTTON_SIZE.y));
        if ((temp.avatar != "default") && ImGui::Button(lang["reset_avatar"].c_str(), ImVec2(RESET_AVATAR_BTN_SIZE, BUTTON_SIZE.y))) {
            temp.avatar = "default";
            init_avatar(gui, emuenv, "temp", "default");
        }
        if (gui.users_avatar.contains("temp")) {
            ImGui::SetCursorPos(ImVec2(AVATAR_POS));
            ImGui::Image(gui.users_avatar["temp"], AVATAR_SIZE);
        }
        const auto CHANGE_AVATAR_BTN_SIZE = ImGui::CalcTextSize(lang["change_avatar"].c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.f);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + (AVATAR_SIZE.x / 2.f) - (CHANGE_AVATAR_BTN_SIZE / 2.f), AVATAR_POS.y + AVATAR_SIZE.y));
        if (ImGui::Button(lang["change_avatar"].c_str(), ImVec2(CHANGE_AVATAR_BTN_SIZE, BUTTON_SIZE.y))) {
            std::filesystem::path avatar_path = "";
            host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(avatar_path, { { "Image file", { "bmp", "gif", "jpg", "png", "tif" } } });

            if ((result == host::dialog::filesystem::Result::SUCCESS) && init_avatar(gui, emuenv, "temp", avatar_path.string()))
                temp.avatar = avatar_path.string();
        }
        ImGui::SetWindowFontScale(0.8f);
        const auto INPUT_NAME_SIZE = 330.f * SCALE.x;
        const auto INPUT_NAME_POS = ImVec2((SIZE_USER.x / 2.f) - (INPUT_NAME_SIZE / 2.f), 244.f * SCALE.y);
        ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x, INPUT_NAME_POS.y - (30.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["name"].c_str());
        ImGui::SetCursorPos(INPUT_NAME_POS);
        ImGui::PushItemWidth(INPUT_NAME_SIZE);
        ImGui::InputText("##user_name", &temp.name);
        ImGui::PopItemWidth();
        const auto free_name = get_users_index(gui, temp.name) == gui.users.end();
        const auto check_free_name = (menu == "create" ? free_name : (temp.name == gui.users[user_id].name) || free_name);
        if (!check_free_name) {
            ImGui::SetCursorPos(ImVec2(INPUT_NAME_POS.x + INPUT_NAME_SIZE + 10.f, INPUT_NAME_POS.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "! %s", lang["user_name_used"].c_str());
        }
        ImGui::SetCursorPos(BUTTON_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        const auto confirm = lang["confirm"].c_str();
        if (!temp.name.empty() && check_free_name ? ImGui::Button(confirm, BUTTON_SIZE) : ImGui::Selectable(confirm, false, ImGuiSelectableFlags_Disabled, BUTTON_SIZE)) {
            gui.users_avatar[user_id] = std::move(gui.users_avatar["temp"]);
            gui.users[user_id] = temp;
            save_user(gui, emuenv, user_id);
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
        const std::string msg = lang["user_created"];
        const auto calc_text = (SIZE_USER.x / 2.f) - (ImGui::CalcTextSize(msg.c_str()).x / 2.f);
        ImGui::SetCursorPos(ImVec2(calc_text, (44.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", msg.c_str());
        ImGui::SetCursorPos(AVATAR_POS);
        if (gui.users_avatar.find(user_id) != gui.users_avatar.end())
            ImGui::Image(gui.users_avatar[user_id], AVATAR_SIZE);
        ImGui::SetCursorPos(ImVec2(AVATAR_POS.x + AVATAR_SIZE.x / 2 - ImGui::CalcTextSize(gui.users[user_id].name.c_str()).x / 2, AVATAR_POS.y + AVATAR_SIZE.y + (5.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[user_id].name.c_str());
        ImGui::SetCursorPos(BUTTON_POS);
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            clear_temp(gui);
            menu.clear();
        }
    } else if (menu == "delete") {
        title = lang["delete_user"];
        if (user_id.empty()) {
            ImGui::SetWindowFontScale(1.f);
            ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - (ImGui::CalcTextSize(lang["user_delete"].c_str()).x / 2.f), 5.f * SCALE.y));
            const auto CHILD_DELETE_USER_SIZE = ImVec2(674 * SCALE.x, 308.f * SCALE.y);
            const auto SELECT_SIZE = ImVec2(674.f * SCALE.x, 46.f * SCALE.y);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["user_delete"].c_str());
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
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["user_delete_msg"].c_str());
                ImGui::SetCursorPos(ImVec2(194.f * SCALE.x, 148.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[user_id].name.c_str());
                ImGui::SetCursorPos(ImVec2(148.f * SCALE.x, 194.f * SCALE.y));
                ImGui::TextWrapped("%s", lang["user_delete_message"].c_str());
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button(common["delete"].c_str(), BUTTON_SIZE))
                    del_menu = "warn";
            } else if (del_menu == "warn") {
                const auto calc_text = (SIZE_USER.x / 2.f) - (ImGui::CalcTextSize(lang["user_delete_warn"].c_str()).x / 2.f);
                ImGui::SetCursorPos(ImVec2(calc_text, 146.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["user_delete_warn"].c_str());
                ImGui::SetCursorPos(BUTTON_POS);
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - BUTTON_SIZE.x - 20.f, BUTTON_POS.y));
                if (ImGui::Button(common["no"].c_str(), BUTTON_SIZE)) {
                    user_id.clear();
                    del_menu.clear();
                }
                ImGui::SameLine(0, 40.f * SCALE.x);
                if (ImGui::Button(common["yes"].c_str(), BUTTON_SIZE)) {
                    fs::remove_all(user_path / user_id);
                    gui.users_avatar.erase(user_id);
                    gui.users.erase(get_users_index(gui, gui.users[user_id].name));
                    if (gui.users.empty() || (user_id == emuenv.io.user_id)) {
                        emuenv.cfg.user_id.clear();
                        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                        emuenv.io.user_id.clear();
                    }
                    del_menu = "confirm";
                }
            } else if (del_menu == "confirm") {
                ImGui::SetCursorPos(ImVec2((SIZE_USER.x / 2.f) - (ImGui::CalcTextSize(lang["user_deleted"].c_str()).x / 2.f), 146.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["user_deleted"].c_str());
                ImGui::SetWindowFontScale(1.f);
                ImGui::SetCursorPos(BUTTON_POS);
                if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                    del_menu.clear();
                    user_id.clear();
                    if (gui.users.empty())
                        menu.clear();
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::SetCursorPosY(WINDOW_SIZE.y - POS_SEPARATOR);
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.f);
    const auto USER_ALREADY_INIT = !gui.users.empty() && !emuenv.io.user_id.empty() && (emuenv.cfg.user_id == emuenv.io.user_id);
    if ((menu.empty() && USER_ALREADY_INIT) || (!menu.empty() && (menu != "confirm") && del_menu.empty())) {
        ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, ImGui::GetCursorPosY() + (10.f * SCALE.y)));
        if (ImGui::Button(common["cancel"].c_str(), ImVec2(80.f * SCALE.x, 40.f * SCALE.y))) {
            if (!menu.empty()) {
                if ((menu == "create") || (menu == "edit"))
                    clear_temp(gui);
                if ((menu == "delete") && !user_id.empty())
                    user_id.clear();
                else
                    menu.clear();
            } else if (USER_ALREADY_INIT) {
                gui.vita_area.user_management = false;
                gui.vita_area.home_screen = true;
            }
        }
    }
    if (menu.empty() && !gui.users.empty()) {
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["automatic_user_login"].c_str()).x / 2.f), WINDOW_SIZE.y - 50.f * SCALE.y));
        if (ImGui::Checkbox(lang["automatic_user_login"].c_str(), &emuenv.cfg.auto_user_login))
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    if (!gui.users.empty() && (gui.users.find(emuenv.cfg.user_id) != gui.users.end())) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 220.f * SCALE.x, WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (SMALL_AVATAR_SIZE.y / 2.f)));
        if (gui.users_avatar.find(emuenv.cfg.user_id) != gui.users_avatar.end())
            ImGui::Image(gui.users_avatar[emuenv.cfg.user_id], SMALL_AVATAR_SIZE);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 180.f * SCALE.x, WINDOW_SIZE.y - (POS_SEPARATOR / 2.f) - (ImGui::CalcTextSize(gui.users[emuenv.cfg.user_id].name.c_str()).y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.users[emuenv.cfg.user_id].name.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
