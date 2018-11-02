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

#include <miniz.h>
#include <psp2/types.h>

#include <io/file.h>

#include <cstdio>
#include <map>
#include <memory>

typedef std::shared_ptr<mz_zip_archive> ZipPtr;
typedef std::shared_ptr<mz_zip_reader_extract_iter_state> ZipFilePtr;

enum TtyType : std::uint8_t {
    TTY_IN = 0b01,
    TTY_OUT = 0b10,
    TTY_INOUT = TTY_IN | TTY_OUT
};

typedef std::map<SceUID, TtyType> TtyFiles;
typedef std::map<SceUID, std::string> FileSystem;

struct IOState {
    struct DevicePaths {
        std::string app0;
        std::string appmeta0;
        std::string cache0;
        std::string ux0_savedata;
        std::string ur0_savedata;
        std::string uma0_savedata;
        std::string addcont0;
        std::string patch0;
        std::string trophy0;
    } device_paths;

    std::string title_id;
    SceUID next_fd = 0;
    TtyFiles tty_files;
    FileSystem std_files;
    FileSystem dir_entries;
};
