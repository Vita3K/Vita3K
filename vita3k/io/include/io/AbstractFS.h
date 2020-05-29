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
#include <vector>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

class AbstractFS {
    fs::path base_path;
    fs::path device_path;
    
protected:
    std::vector<fs::path> filesystem_list;

public:
    bool file_exists(std::string file_name);
    virtual void init_filesystem_list();
};