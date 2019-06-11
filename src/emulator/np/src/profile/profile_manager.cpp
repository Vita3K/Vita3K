// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <np/profile/profile_manager.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace emu::np::profile {

NPProfile *NPProfileManager::get_current_profile() {
    if (current_profile_index >= static_cast<std::uint32_t>(profiles.size())) {
        return nullptr;
    }

    return &profiles[current_profile_index];
}

bool NPProfileManager::serialize_info() {
    YAML::Emitter emitter;
    emitter << YAML::BeginMap;

    emitter << "default-index" << current_profile_index;

    for (auto &profile: profiles) {
        emitter << YAML::Key << profile.online_id;
        emitter << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "online-id" << YAML::Value << profile.online_id;
        emitter << YAML::Key << "index" << YAML::Value << profile.index;
        emitter << YAML::EndMap;
    }

    emitter << YAML::EndMap;

    std::ofstream out("user_profiles.yml");
    out << emitter.c_str();

    return true;
}

bool NPProfileManager::deserialize_info() {
    // Read from yaml file
    YAML::Node profile_file;

    try {
        profile_file = YAML::LoadFile("user_profiles.yml");
    } catch (...) {
        return false;
    }

    // Read infos
    // Read default user
    current_profile_index = profile_file["default-index"].as<std::uint32_t>();
    NPProfile np_profile;
    
    for (auto &profile: profile_file) {
        try {
            np_profile.online_id = profile["online-id"].as<std::string>();
            np_profile.index = profile["index"].as<std::uint32_t>();

            profiles.push_back(np_profile);
        } catch (...) {
            continue;
        }
    }

    std::sort(profiles.begin(), profiles.end(), [](const NPProfile &lhs, const NPProfile &rhs) {
        return lhs.index < rhs.index;
    });

    return true;
}

NPProfile *NPProfileManager::create_new_profile(const std::string &online_id, const bool set_as_default) {
    // Check if the online ID already exists
    const auto result = std::find_if(profiles.begin(), profiles.end(), [=](const NPProfile &the_one) {
        return the_one.online_id == online_id;
    });

    if (result != profiles.end()) {
        return nullptr;
    }

    NPProfile profile;
    profile.online_id = online_id;
    profile.index = static_cast<std::uint32_t>(profiles.size());

    profiles.push_back(std::move(profile));

    if (set_as_default) {
        current_profile_index = static_cast<std::uint32_t>(profiles.size() - 1);
    }

    serialize_info();

    return &profiles.back();
}

bool NPProfileManager::init() {
    // Try to load all users
    if (!deserialize_info()) {
        create_new_profile("dummy");
    }

    return true;
}

bool NPProfileManager::deinit() {
    serialize_info();
    return true;
}

}