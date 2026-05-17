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

#include <app/functions.h>
#include <app/state.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <util/fs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <fmt/format.h>

namespace app {

namespace {

static std::string next_free_id(const std::map<std::string, User> &users) {
    int id = 0;
    for (const auto &[key, _] : users) {
        int val = 0;
        try {
            val = std::stoi(key);
        } catch (...) {
            continue;
        }
        if (id != val)
            break;
        ++id;
    }
    return fmt::format("{:0>2d}", id);
}

} // namespace

void load_users(EmuEnvState &emuenv) {
    auto &user_list = emuenv.app.user_list;
    user_list.users.clear();

    const auto user_path = emuenv.pref_path / "ux0/user";
    if (!fs::exists(user_path) || fs::is_empty(user_path))
        return;

    for (const auto &path : fs::directory_iterator(user_path)) {
        if (!fs::is_directory(path))
            continue;

        pugi::xml_document user_xml;
        if (!user_xml.load_file((path.path() / "user.xml").c_str()))
            continue;

        const auto user_child = user_xml.child("user");

        std::string user_id;
        if (!user_child.attribute("id").empty())
            user_id = user_child.attribute("id").as_string();
        else
            user_id = path.path().stem().string();

        auto &user = user_list.users[user_id];
        user.id = user_id;
        if (!user_child.attribute("name").empty())
            user.name = user_child.attribute("name").as_string();

        auto theme = user_child.child("theme");
        if (!theme.attribute("use-background").empty())
            user.use_theme_bg = theme.attribute("use-background").as_bool();

        if (!theme.child("content-id").text().empty())
            user.theme_id = theme.child("content-id").text().as_string();

        auto start = user_child.child("start-screen");
        if (!start.attribute("type").empty())
            user.start_type = start.attribute("type").as_string();

        if (!start.child("path").text().empty())
            user.start_path = start.child("path").text().as_string();

        for (const auto &bg : user_child.child("backgrounds"))
            user.backgrounds.emplace_back(bg.text().as_string());
    }
}

void save_user(EmuEnvState &emuenv, const std::string &user_id) {
    const auto &user_list = emuenv.app.user_list;
    const auto it = user_list.users.find(user_id);
    if (it == user_list.users.end()) {
        LOG_ERROR("Cannot save unknown user id: {}", user_id);
        return;
    }

    const auto user_path = emuenv.pref_path / "ux0/user" / user_id;
    fs::create_directories(user_path);

    const auto &user = it->second;

    pugi::xml_document user_xml;
    auto declaration = user_xml.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "utf-8";

    auto user_child = user_xml.append_child("user");
    user_child.append_attribute("id") = user.id.c_str();
    user_child.append_attribute("name") = user.name.c_str();

    auto theme = user_child.append_child("theme");
    theme.append_attribute("use-background") = user.use_theme_bg;
    theme.append_child("content-id").append_child(pugi::node_pcdata).set_value(user.theme_id.c_str());

    auto start_screen = user_child.append_child("start-screen");
    start_screen.append_attribute("type") = user.start_type.c_str();
    start_screen.append_child("path").append_child(pugi::node_pcdata).set_value(user.start_path.c_str());

    auto bg_path = user_child.append_child("backgrounds");
    for (const auto &bg : user.backgrounds)
        bg_path.append_child("background").append_child(pugi::node_pcdata).set_value(bg.c_str());

    const auto save_xml = user_xml.save_file((user_path / "user.xml").c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml for user id: {}, name: {}, in path: {}", user.id, user.name, user_path);
}

std::string create_user(EmuEnvState &emuenv, const std::string &name) {
    auto &user_list = emuenv.app.user_list;
    const std::string new_id = next_free_id(user_list.users);

    User user;
    user.id = new_id;
    user.name = name;
    user.theme_id = "default";
    user.use_theme_bg = true;
    user.start_type = "default";

    user_list.users[new_id] = std::move(user);
    save_user(emuenv, new_id);

    return new_id;
}

void delete_user(EmuEnvState &emuenv, const std::string &user_id) {
    auto &user_list = emuenv.app.user_list;

    const auto user_path = emuenv.pref_path / "ux0/user" / user_id;
    if (fs::exists(user_path))
        fs::remove_all(user_path);

    user_list.users.erase(user_id);

    if (emuenv.cfg.user_id == user_id) {
        emuenv.cfg.user_id.clear();
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        emuenv.io.user_id.clear();
        emuenv.io.user_name.clear();
    }
}

bool activate_user(EmuEnvState &emuenv, const std::string &user_id) {
    const auto &user_list = emuenv.app.user_list;
    const auto it = user_list.users.find(user_id);
    if (it == user_list.users.end()) {
        LOG_ERROR("Cannot activate unknown user id: {}", user_id);
        return false;
    }

    emuenv.io.user_id = user_id;
    emuenv.io.user_name = it->second.name;
    return true;
}

} // namespace app