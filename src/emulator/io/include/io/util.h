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

#undef st_atime
#undef st_ctime
#undef st_mtime
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <util/fs.h>

typedef SceUInt8 TtyType;

inline constexpr TtyType TTY_UNKNOWN = 0b00;
inline constexpr TtyType TTY_IN = 0b01;
inline constexpr TtyType TTY_OUT = 0b10;
inline constexpr TtyType TTY_INOUT = TTY_IN | TTY_OUT;

struct FileInfo {
    // The actual location on the Vita
    const char *vita_loc;
    // The translated location on the Vita
    std::string translated;
    // The actual location in the preference path.
    fs::path sys_loc;
    // One or more SceIoMode flags
    int open_mode;
    // One or more SceIoFileMode flags
    int file_mode;
    // One or more SceIoAccessMode flags
    int access_mode;

    FileInfo()
        : vita_loc(nullptr)
        , open_mode(SCE_O_RDONLY)
        , file_mode(SCE_SO_IFREG | SCE_SO_IROTH)
        , access_mode(SCE_S_IRUSR) {}
};

inline bool can_write(const int mode) {
    return mode & SCE_O_WRONLY || mode & SCE_O_APPEND || mode & SCE_O_CREAT;
}

class VitaStats {
protected:
    FileInfo file_info;

public:
    VitaStats() {
        file_info.vita_loc = nullptr;
    }

    VitaStats(const char *vita, const std::string &t, const fs::path &file) {
        file_info.vita_loc = vita;
        file_info.translated = t;
        file_info.sys_loc = file;
    }

    const char *get_vita_loc() const {
        return file_info.vita_loc;
    }

    const std::string &get_translated_path() const {
        return file_info.translated;
    }

    const fs::path &get_system_location() const {
        return file_info.sys_loc;
    }

    int get_open_mode() const {
        return file_info.open_mode;
    }

    int get_file_mode() const {
        return file_info.file_mode;
    }

    int get_access_mode() const {
        return file_info.access_mode;
    }

// Overloaded functions for separate systems
#ifdef WIN32
    const wchar_t *get_char_path() const {
        return file_info.sys_loc.generic_path().wstring().c_str();
    }
#else
    const char *get_char_path() const {
        return file_info.sys_loc.generic_path().string().c_str();
    }
#endif

    // Modify IO parameters
    void create_io_perms(const int flags) {
        file_info.open_mode = flags;
    }

    void add_io_perms(const int flags) {
        file_info.open_mode |= flags;
    }

    // Reset to only allow reading
    void remove_perms() {
        file_info.open_mode = SCE_O_RDONLY;
    }
};
