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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace emu::np::profile {

enum class NPProfilePlatform {
    INVALID = 0,
    PS3 = 1,
    VITA = 2
};

struct NPProfile {
    std::string online_id;
    NPProfilePlatform platform = NPProfilePlatform::VITA;
    std::uint32_t index;
};

class NPProfileManager {
    std::vector<NPProfile> profiles;
    std::uint32_t current_profile_index { 0 };

    explicit NPProfileManager();
    ~NPProfileManager();

    bool serialize_info();
    bool deserialize_info();
    
    /**
     * \brief   Create a new profile.
     * 
     * \param   online_id        The online ID of the profile. This must be unique and not overlapped with any
     *                           current name.
     * 
     * \param   set_as_default   If this is true, the current profile will be the newly created one.
     * 
     * \returns A pointer to the profile on success, else nullptr on failure.
     */
    NPProfile *create_new_profile(const std::string &online_id, const bool set_as_default = true);

    NPProfile *get_current_profile();
};

}