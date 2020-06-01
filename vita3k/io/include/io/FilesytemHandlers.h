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
#include "io/VitaIoDevice.h"
#include "io/UX0.h"

// Description: A class that handles devices operations,
// is singleton so that way it can be easily accessed from everywhere in
// the project, while also making sure that there is always only one mount and no
// collision in file handling.
class FilesystemHandlers {
    static FilesystemHandlers *my_instance;

protected:
    fs::path file_base_path;
    FilesystemHandlers(fs::path out_base_path) : file_base_path(out_base_path) { }


public:
    /**
     * Singletons should not be cloneable.
     */
    FilesystemHandlers(FilesystemHandlers &other) = delete;
    /**
     * Singletons should not be assignable.
     */
    void operator=(const FilesystemHandlers &) = delete;
    FilesystemHandlers(const FilesystemHandlers &) = delete;

    AbstractFS* open_filesystem(better_enums_data_VitaIoDevice::_enumClassForSwitchStatements device);

    static FilesystemHandlers *GetInstance(const fs::path &base_path);
};