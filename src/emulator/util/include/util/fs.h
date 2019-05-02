// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

class Root {
    fs::path base_path;
    fs::path pref_path;

public:
    void set_base_path(const fs::path &p) {
        base_path = p;
    }

    fs::path get_base_path() const {
        return base_path;
    }

    std::string get_base_path_string() const {
        return base_path.generic_path().string();
    }

    void set_pref_path(const fs::path &p) {
        pref_path = p;
    }

    fs::path get_pref_path() const {
        return pref_path;
    }

    std::string get_pref_path_string() const {
        return pref_path.generic_path().string();
    }
}; // class root
